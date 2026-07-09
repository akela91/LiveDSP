#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cmath>

/**
    Full-featured practice metronome for the guitar mode. All sounds are
    synthesized (no samples): classic beep, woodblock, kick drum and hi-hat.

    Features (the usual guitar-practice set):
      - tempo 30..300 BPM, live-adjustable (the UI adds tap tempo on top)
      - 1..12 beats per bar, accented downbeat (toggleable)
      - subdivisions: quarter (1), eighth (2), triplet (3), sixteenth (4);
        subdivision ticks play quieter than the main beats
      - speed trainer: +N BPM every M bars (capped at 300)
      - gap trainer: X bars audible, then Y bars muted (inner-clock practice);
        the visual beat display keeps running through the muted bars
      - display state for the UI: effective BPM, current beat, muted-bar flag

    The click is ADDED to the processed output (after the output gain, so the
    OUTPUT knob does not affect it — the metronome has its own volume), and it
    sounds even when the amp is off. Latency-free.

    prepare() runs on the MESSAGE THREAD (audio stopped); the setters and
    process() are real-time safe (atomics only, no locks/allocations).
*/
class Metronome
{
public:
    enum Sound { beep = 0, wood = 1, kick = 2, hat = 3 };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        wasPlaying = false;
        voice.active = false;
        dispBeat.store (-1);
    }

    //==========================================================================
    // Transport (not persisted — the metronome always starts stopped).
    void setPlaying (bool shouldPlay) noexcept { playing.store (shouldPlay); }
    bool isPlaying() const noexcept            { return playing.load(); }

    // Parameters (stored per block from the APVTS).
    void setBpm         (float v) noexcept { bpmBase.store (juce::jlimit (30.0f, 300.0f, v)); }
    void setBeatsPerBar (int v)   noexcept { beatsPerBar.store (juce::jlimit (1, 12, v)); }
    void setSubdivision (int v)   noexcept { subdivision.store (juce::jlimit (1, 4, v)); }
    void setSound       (int v)   noexcept { sound.store (juce::jlimit (0, 3, v)); }
    void setVolume      (float pct) noexcept { volume.store (juce::jlimit (0.0f, 100.0f, pct)); }
    void setAccent      (bool b)  noexcept { accentOn.store (b); }

    void setTrainer (bool on, int incBpm, int everyBars) noexcept
    {
        trainerOn.store (on);
        trainerInc.store (juce::jlimit (1, 20, incBpm));
        trainerBars.store (juce::jlimit (1, 16, everyBars));
    }

    void setGap (bool on, int playBars, int muteBars) noexcept
    {
        gapOn.store (on);
        gapPlay.store (juce::jlimit (1, 16, playBars));
        gapMute.store (juce::jlimit (1, 16, muteBars));
    }

    //==========================================================================
    // Display state for the UI (message thread reads).
    float getDisplayBpm()  const noexcept { return dispBpm.load(); }    // effective (trainer!) BPM
    int   getDisplayBeat() const noexcept { return dispBeat.load(); }   // 0-based beat in bar, -1 = stopped
    bool  isBarMuted()     const noexcept { return dispMuted.load(); }  // gap trainer muting now

    //==========================================================================
    // Adds the click to every channel of the (already processed) output buffer.
    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const bool p = playing.load();

        if (p && ! wasPlaying)   // start: reset and click immediately on beat 1
        {
            samplesToTick = 0.0;
            tickInBar     = 0;
            barCount      = 0;
            barsSinceInc  = 0;
            trainerOffset = 0;
            voice.active  = false;
        }
        if (! p && wasPlaying)   // stop: cut the voice, clear the display
        {
            voice.active = false;
            dispBeat.store (-1);
            dispMuted.store (false);
        }
        wasPlaying = p;

        if (! p)
        {
            dispBpm.store (bpmBase.load());
            return;
        }

        const int   sub    = subdivision.load();
        const int   beats  = beatsPerBar.load();
        const float bpmEff = juce::jlimit (30.0f, 300.0f,
                                           bpmBase.load() + (float) trainerOffset);
        dispBpm.store (bpmEff);

        const double samplesPerTick = sampleRate * 60.0 / ((double) bpmEff * sub);
        const int    ticksPerBar    = beats * sub;

        const float master = std::pow (volume.load() * 0.01f, 1.5f) * 0.9f;

        const int numSamples  = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            samplesToTick -= 1.0;
            if (samplesToTick <= 0.0)
            {
                samplesToTick += samplesPerTick;
                triggerTick (ticksPerBar, sub);
            }

            const float y = voice.render() * master;
            if (y != 0.0f)
                for (int ch = 0; ch < numChannels; ++ch)
                    buffer.getWritePointer (ch)[i] += y;
        }
    }

private:
    //==========================================================================
    void triggerTick (int ticksPerBar, int sub) noexcept
    {
        if (tickInBar >= ticksPerBar)   // beats/subdiv were reduced live
            tickInBar = 0;

        const bool isDown = tickInBar == 0;
        const bool isBeat = (tickInBar % sub) == 0;

        // Gap trainer: after gapPlay audible bars, gapMute bars are silent.
        bool muted = false;
        if (gapOn.load())
        {
            const int cycle = gapPlay.load() + gapMute.load();
            muted = (barCount % cycle) >= gapPlay.load();
        }
        dispMuted.store (muted);

        if (isBeat)
            dispBeat.store (tickInBar / sub);

        if (! muted)
        {
            const bool accent = isDown && accentOn.load();
            const float amp   = accent ? 1.0f : (isBeat ? 0.78f : 0.4f);
            voice.trigger (sound.load(), accent, amp, sampleRate);
        }

        if (++tickInBar >= ticksPerBar)
        {
            tickInBar = 0;
            ++barCount;

            // Speed trainer: +inc BPM every N bars (the cap is applied in process()).
            if (trainerOn.load() && ++barsSinceInc >= trainerBars.load())
            {
                barsSinceInc  = 0;
                trainerOffset += trainerInc.load();
            }
        }
    }

    //==========================================================================
    // Single synthesized click voice (a new tick retriggers it).
    struct Voice
    {
        bool   active { false };
        int    sound  { 0 };
        float  amp    { 0.0f };
        int    age    { 0 };
        double phase  { 0.0 }, sr { 48000.0 };
        float  env    { 0.0f }, envMult { 0.0f };
        float  baseHz { 0.0f };
        float  fEnv   { 0.0f }, fMult { 0.0f };   // kick pitch-sweep envelope
        float  prevNoise { 0.0f };
        juce::uint32 rng { 0x12345678 };

        void trigger (int soundType, bool accent, float amplitude, double sampleRate) noexcept
        {
            active = true;
            sound  = soundType;
            amp    = amplitude;
            age    = 0;
            phase  = 0.0;
            env    = 1.0f;
            sr     = sampleRate;

            auto decay = [this] (float seconds) { return std::exp (-1.0f / (seconds * (float) sr)); };

            switch (sound)
            {
                case Metronome::beep:   // classic tuner-style beep
                    baseHz  = accent ? 1568.0f : 1046.0f;   // G6 / C6
                    envMult = decay (0.028f);
                    break;
                case Metronome::wood:   // short woodblock-like click
                    baseHz  = accent ? 3200.0f : 2500.0f;
                    envMult = decay (0.006f);
                    break;
                case Metronome::kick:   // 808-ish kick: pitch-swept sine + soft clip
                    baseHz  = 46.0f;
                    fEnv    = accent ? 150.0f : 115.0f;
                    fMult   = decay (0.035f);
                    envMult = decay (0.12f);
                    break;
                case Metronome::hat:    // short high-passed noise burst
                default:
                    envMult = decay (0.016f);
                    break;
            }
        }

        float render() noexcept
        {
            if (! active)
                return 0.0f;

            float y = 0.0f;
            switch (sound)
            {
                case Metronome::beep:
                case Metronome::wood:
                    phase += juce::MathConstants<double>::twoPi * baseHz / sr;
                    y = (float) std::sin (phase) * env;
                    break;

                case Metronome::kick:
                {
                    const float f = baseHz + fEnv;
                    fEnv *= fMult;
                    phase += juce::MathConstants<double>::twoPi * f / sr;
                    y = std::tanh (2.4f * (float) std::sin (phase)) * env;
                    if (age < 96)   // attack "beater" click on top
                        y += nextNoise() * 0.35f * (1.0f - age / 96.0f);
                    break;
                }

                case Metronome::hat:
                default:
                {
                    const float n = nextNoise();
                    y = (n - prevNoise) * env * 0.9f;   // 1st-order HP on noise
                    prevNoise = n;
                    break;
                }
            }

            env *= envMult;
            if (age < 12)                       // tiny attack ramp against edge clicks
                y *= (float) age / 12.0f;
            ++age;
            if (env < 1.0e-4f)
                active = false;

            return y * amp;
        }

        float nextNoise() noexcept             // xorshift white noise, RT-safe
        {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            return (float) rng / (float) 0xffffffffu * 2.0f - 1.0f;
        }
    };

    //==========================================================================
    double sampleRate { 48000.0 };

    std::atomic<bool>  playing     { false };
    std::atomic<float> bpmBase     { 120.0f };
    std::atomic<int>   beatsPerBar { 4 };
    std::atomic<int>   subdivision { 1 };
    std::atomic<int>   sound       { 0 };
    std::atomic<float> volume      { 80.0f };
    std::atomic<bool>  accentOn    { true };

    std::atomic<bool> trainerOn   { false };
    std::atomic<int>  trainerInc  { 2 };
    std::atomic<int>  trainerBars { 4 };

    std::atomic<bool> gapOn   { false };
    std::atomic<int>  gapPlay { 4 };
    std::atomic<int>  gapMute { 4 };

    // Display state
    std::atomic<float> dispBpm   { 120.0f };
    std::atomic<int>   dispBeat  { -1 };
    std::atomic<bool>  dispMuted { false };

    // Audio-thread runtime state
    bool   wasPlaying    { false };
    double samplesToTick { 0.0 };
    int    tickInBar     { 0 };
    int    barCount      { 0 };
    int    barsSinceInc  { 0 };
    int    trainerOffset { 0 };
    Voice  voice;
};
