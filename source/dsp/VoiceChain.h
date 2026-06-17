#pragma once

#include <JuceHeader.h>
#include <cmath>

/**
    Live vocal processing chain for a quiet dynamic microphone (e.g. Shure SM58) +
    Focusrite Scarlett audio interface. Built from low-latency, zero-latency
    processing elements — specifically for live monitoring.

    The signal chain STRICTLY in this order (juce::dsp::ProcessorChain):
      1. Input Gain  — digital preamp (0 .. +24 dB).
      2. Low-Cut     — high-pass filter, FIXED at 90 Hz.
      3. Noise Gate  — after the filter; threshold (-80..-20 dB), ratio 10:1 / attack
                       2 ms / release 150 ms FIXED.
      4. Warmth      — subtle tanh saturation (WaveShaper); WARMTH = drive 1..3,
                       level-preserving (pre-gain -> tanh -> post-gain 1/drive).
      5. Compressor  — Threshold/Ratio adjustable, attack 5 ms / release 100 ms FIXED.
      6. High-Shelf  — FIXED 6 kHz "air/presence" (0 .. +12 dB).
      7. Delay       — simple delay; TIME 50..500 ms, MIX 0..50%, feedback 0.3 FIXED.
      8. Reverb      — medium reverb (FIXED room/damp), Wet/Dry mix (0..100%).
      9. Limiter     — brickwall at the very end of the chain, FIXED -0.1 dB ceiling.

    Operates on a stereo block (the mono microphone signal copied to both channels).
    prepare/reset must be called FROM THE MESSAGE THREAD; the setters run before
    process (on the audio thread), so simple (non-atomic) members are sufficient.
*/
class VoiceChain
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        chain.prepare (spec);

        updateLowCut();
        updateAir();

        // Noise gate: high ratio, fast attack, medium release (FIXED).
        auto& gate = chain.get<gateIdx>();
        gate.setRatio   (10.0f);
        gate.setAttack  (2.0f);
        gate.setRelease (150.0f);

        // Warmth: the WaveShaper is a FIXED tanh function; the "amount" comes from the pre/post gain.
        chain.get<shaperIdx>().functionToUse = [] (float x) { return std::tanh (x); };
        applyWarmth();

        // Compressor: fast attack, medium release (FIXED).
        auto& comp = chain.get<compIdx>();
        comp.setAttack  (5.0f);
        comp.setRelease (100.0f);

        // Safe maximum for the delay buffer (2 s) at the actual sample rate.
        auto& dly = chain.get<delayIdx>();
        dly.prepareDelay (sampleRate, (int) spec.maximumBlockSize);

        updateReverb();

        // Brickwall limiter at the end of the chain: FIXED -0.1 dB ceiling.
        auto& lim = chain.get<limIdx>();
        lim.setThreshold (-0.1f);
        lim.setRelease   (50.0f);

        chain.reset();
    }

    void reset() { chain.reset(); }

    //==============================================================================
    // Live parameter setters (run on the audio thread, before process).
    void setInputGainDb (float db) noexcept
    {
        chain.get<gainIdx>().setGainDecibels (db);
        gainLin = juce::Decibels::decibelsToGain (db);
    }

    void setGateThreshold (float db) noexcept { chain.get<gateIdx>().setThreshold (db); gateThrDb = db; }

    void setWarmth (float drive) noexcept
    {
        warmthDrive = juce::jlimit (1.0f, 3.0f, drive);
        applyWarmth();
    }

    void setCompThreshold (float db) noexcept { chain.get<compIdx>().setThreshold (db); compThrDb = db; }
    void setCompRatio     (float r)  noexcept { chain.get<compIdx>().setRatio (r); compRatio = r; }

    void setAirDb (float db) noexcept { if (db != airDb) { airDb = db; updateAir(); } }

    void setDelayTimeMs (float ms)  noexcept { chain.get<delayIdx>().timeMs = juce::jlimit (50.0f, 500.0f, ms); }
    void setDelayMix    (float mix) noexcept { chain.get<delayIdx>().mix    = juce::jlimit (0.0f, 0.5f, mix); }

    // mix: 0..1 (Wet/Dry ratio).
    void setReverbMix (float mix) noexcept
    {
        mix = juce::jlimit (0.0f, 1.0f, mix);
        if (mix != reverbMix) { reverbMix = mix; updateReverb(); }
    }

    // Bypass switches (the ProcessorChain can be bypassed element by element).
    void setGateEnabled   (bool b) noexcept { chain.setBypassed<gateIdx>   (! b); gateOn = b; }
    void setWarmthEnabled (bool b) noexcept
    {
        warmthOn = b;
        chain.setBypassed<warmthPreIdx>  (! b);
        chain.setBypassed<shaperIdx>     (! b);
        chain.setBypassed<warmthPostIdx> (! b);
    }
    void setCompEnabled   (bool b) noexcept { chain.setBypassed<compIdx>  (! b); compOn = b; }
    void setAirEnabled    (bool b) noexcept { chain.setBypassed<shelfIdx> (! b); }
    void setDelayEnabled  (bool b) noexcept { chain.setBypassed<delayIdx> (! b); }
    void setReverbEnabled (bool b) noexcept { chain.setBypassed<revIdx>   (! b); }

    //==============================================================================
    // Display-only meters for the UI LEDs (they do NOT affect the audio): the gate's
    // current (estimated) gain 0..1 (0 = muted), and the compressor's estimated gain
    // reduction 0..1 (0 = no compression). To be read from the message thread.
    float getGateGain()      const noexcept { return gateDisp.load(); }
    float getCompReduction() const noexcept { return compDisp.load(); }

    //==============================================================================
    void process (juce::dsp::AudioBlock<float> block) noexcept
    {
        updateMeters (block);   // BEFORE processing (from the input level)
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chain.process (ctx);
    }

private:
    //==========================================================================
    /** Simple stereo delay effect inserted into the ProcessorChain (juce::dsp::
        DelayLine + wet/dry mix + FIXED feedback). The time/mix members are set on
        the audio thread before process. */
    struct VocalDelay
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> line { 96000 };
        double sr  { 48000.0 };
        float  timeMs { 200.0f };
        float  mix    { 0.12f };
        static constexpr float feedback = 0.3f;   // FIXED, to avoid runaway feedback

        void prepare (const juce::dsp::ProcessSpec& spec) { sr = spec.sampleRate; }

        // Safely set the maximum delay buffer (2 s) + prepare.
        void prepareDelay (double sampleRate, int maxBlock)
        {
            sr = sampleRate;
            line.setMaximumDelayInSamples ((int) (sampleRate * 2.0) + maxBlock);
            juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, 2 };
            line.prepare (spec);
            line.reset();
        }

        void reset() { line.reset(); }

        template <typename Context>
        void process (const Context& ctx) noexcept
        {
            if (ctx.isBypassed)
                return;

            auto& out = ctx.getOutputBlock();
            const int nch = (int) out.getNumChannels();
            const int n   = (int) out.getNumSamples();

            line.setDelay ((float) (timeMs * 0.001 * sr));

            for (int ch = 0; ch < nch; ++ch)
            {
                auto* d = out.getChannelPointer ((size_t) ch);
                for (int i = 0; i < n; ++i)
                {
                    const float in      = d[i];
                    const float delayed = line.popSample (ch);
                    line.pushSample (ch, in + delayed * feedback);
                    d[i] = in + delayed * mix;
                }
            }
        }
    };

    // Update the display meter from the input level (before processing). It only
    // provides an estimate for the LEDs — the actual gate/comp operates in the chain.
    void updateMeters (const juce::dsp::AudioBlock<float>& block) noexcept
    {
        const int n = (int) block.getNumSamples();
        if (n <= 0 || block.getNumChannels() == 0)
            return;

        const auto* d = block.getChannelPointer (0);
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = juce::jmax (peak, std::abs (d[i]));

        const float lvl = peak * gainLin;                 // estimate of the level reaching the gate
        if (lvl > dispEnv) dispEnv = lvl;                 // fast rise
        else               dispEnv = dispEnv * 0.9f + lvl * 0.1f;  // slow fall

        const float lvlDb = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, dispEnv));

        // Gate: opens when the level is above the threshold (smoothed).
        const float gateTarget = (lvlDb > gateThrDb) ? 1.0f : 0.0f;
        gateSm = gateSm * 0.8f + gateTarget * 0.2f;
        gateDisp.store (gateOn ? gateSm : 1.0f);

        // Compressor's estimated gain reduction (dB) -> 0..1 for the LED.
        float redDb = 0.0f;
        if (compOn && lvlDb > compThrDb && compRatio > 1.0f)
            redDb = (lvlDb - compThrDb) * (1.0f - 1.0f / compRatio);
        compDisp.store (juce::jlimit (0.0f, 1.0f, redDb / 12.0f));
    }

    void updateLowCut()
    {
        *chain.get<hpfIdx>().state =
            *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 90.0);
    }

    void updateAir()
    {
        *chain.get<shelfIdx>().state =
            *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sampleRate, 6000.0, 0.707, juce::Decibels::decibelsToGain (airDb));
    }

    void applyWarmth()
    {
        // Level-preserving saturation: pre = drive, post = 1/drive  ->  y = tanh(drive*x)/drive.
        // At small signals ≈ unity (subtle), at large signals a soft tanh clip.
        const float d = warmthOn ? warmthDrive : 1.0f;
        chain.get<warmthPreIdx>().setGainLinear  (d);
        chain.get<warmthPostIdx>().setGainLinear (1.0f / d);
    }

    void updateReverb()
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize   = 0.5f;
        p.damping    = 0.45f;
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        p.wetLevel   = reverbMix;
        p.dryLevel   = 1.0f - reverbMix;
        chain.get<revIdx>().setParameters (p);
    }

    using Gain   = juce::dsp::Gain<float>;
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;
    using Gate   = juce::dsp::NoiseGate<float>;
    using Shaper = juce::dsp::WaveShaper<float>;
    using Comp   = juce::dsp::Compressor<float>;
    using Verb   = juce::dsp::Reverb;
    using Lim    = juce::dsp::Limiter<float>;

    enum { gainIdx = 0, hpfIdx, gateIdx, warmthPreIdx, shaperIdx, warmthPostIdx,
           compIdx, shelfIdx, delayIdx, revIdx, limIdx };

    juce::dsp::ProcessorChain<Gain, Filter, Gate, Gain, Shaper, Gain,
                              Comp, Filter, VocalDelay, Verb, Lim> chain;

    double sampleRate  { 48000.0 };
    float  airDb       { 0.0f };
    float  reverbMix   { 0.2f };
    float  warmthDrive { 1.2f };
    bool   warmthOn    { true };

    // Display meter state (written only on the audio thread, read atomically by the UI).
    float gainLin   { 2.0f };
    float gateThrDb { -55.0f };
    float compThrDb { -18.0f };
    float compRatio { 3.0f };
    bool  gateOn    { true };
    bool  compOn    { true };
    float dispEnv   { 0.0f };
    float gateSm    { 1.0f };
    std::atomic<float> gateDisp { 1.0f };
    std::atomic<float> compDisp { 0.0f };
};
