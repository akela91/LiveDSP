#pragma once

#include <JuceHeader.h>
#include <cmath>

/**
    Élő ének-feldolgozó lánc halk dinamikus mikrofonhoz (pl. Shure SM58) +
    Focusrite Scarlett hangkártyához. Alacsony latenciás, nulla-latenciás
    feldolgozó elemekből — kifejezetten live monitorozáshoz.

    A jelút SZIGORÚAN ebben a sorrendben (juce::dsp::ProcessorChain):
      1. Input Gain  — digitális előerősítő (0 .. +24 dB).
      2. Low-Cut     — high-pass szűrő, FIXEN 90 Hz.
      3. Noise Gate  — a szűrő után; küszöb (-80..-20 dB), ratio 10:1 / attack
                       2 ms / release 150 ms FIX.
      4. Warmth      — finom tanh-szaturáció (WaveShaper); WARMTH = drive 1..3,
                       szinttartó (pre-gain -> tanh -> post-gain 1/drive).
      5. Compressor  — Threshold/Ratio állítható, attack 5 ms / release 100 ms FIX.
      6. High-Shelf  — FIXEN 6 kHz "air/presence" (0 .. +12 dB).
      7. Delay       — egyszerű delay; TIME 50..500 ms, MIX 0..50%, feedback 0.3 FIX.
      8. Reverb      — közepes hall (FIX room/damp), Wet/Dry mix (0..100%).
      9. Limiter     — brickwall a lánc legvégén, FIX -0.1 dB ceiling.

    Sztereó blokkon dolgozik (a mono mikrofonjel mindkét csatornára másolva).
    A prepare/reset ÜZENETSZÁLRÓL hívandó; a setterek a process előtt (a hang-
    szálon) futnak, ezért egyszerű (nem-atomikus) tagok elegendők.
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

        // Noise gate: magas ratio, gyors attack, közepes release (FIX).
        auto& gate = chain.get<gateIdx>();
        gate.setRatio   (10.0f);
        gate.setAttack  (2.0f);
        gate.setRelease (150.0f);

        // Warmth: a WaveShaper FIX tanh-függvény; az "amount"-ot a pre/post gain adja.
        chain.get<shaperIdx>().functionToUse = [] (float x) { return std::tanh (x); };
        applyWarmth();

        // Compressor: gyors attack, közepes release (FIX).
        auto& comp = chain.get<compIdx>();
        comp.setAttack  (5.0f);
        comp.setRelease (100.0f);

        // Delay buffer biztonságos maximuma (2 mp) a tényleges SR-hez.
        auto& dly = chain.get<delayIdx>();
        dly.prepareDelay (sampleRate, (int) spec.maximumBlockSize);

        updateReverb();

        // Brickwall limiter a lánc végén: FIX -0.1 dB ceiling.
        auto& lim = chain.get<limIdx>();
        lim.setThreshold (-0.1f);
        lim.setRelease   (50.0f);

        chain.reset();
    }

    void reset() { chain.reset(); }

    //==============================================================================
    // Élő paraméter-beállítók (a hangszálon futnak, a process előtt).
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

    // mix: 0..1 (Wet/Dry arány).
    void setReverbMix (float mix) noexcept
    {
        mix = juce::jlimit (0.0f, 1.0f, mix);
        if (mix != reverbMix) { reverbMix = mix; updateReverb(); }
    }

    // Bypass-kapcsolók (a ProcessorChain elemenként megkerülhető).
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
    // Kijelző-célú mérők a UI LED-ekhez (a hangot NEM érintik): a kapu aktuális
    // (becsült) gain-je 0..1 (0 = némít), és a kompresszor becsült gain-csökkentése
    // 0..1 (0 = nincs vágás). Üzenetszálról olvasandó.
    float getGateGain()      const noexcept { return gateDisp.load(); }
    float getCompReduction() const noexcept { return compDisp.load(); }

    //==============================================================================
    void process (juce::dsp::AudioBlock<float> block) noexcept
    {
        updateMeters (block);   // a feldolgozás ELŐTT (a bemeneti szintből)
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chain.process (ctx);
    }

private:
    //==========================================================================
    /** Egyszerű sztereó delay effekt a ProcessorChain-be illesztve (juce::dsp::
        DelayLine + wet/dry mix + FIX feedback). A time/mix tagok a hangszálon
        állítódnak a process előtt. */
    struct VocalDelay
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> line { 96000 };
        double sr  { 48000.0 };
        float  timeMs { 200.0f };
        float  mix    { 0.12f };
        static constexpr float feedback = 0.3f;   // FIX, hogy ne gerjedjen

        void prepare (const juce::dsp::ProcessSpec& spec) { sr = spec.sampleRate; }

        // A maximális delay-puffer biztonságos beállítása (2 mp) + prepare.
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

    // Kijelző-mérő frissítése a bemeneti szintből (a feldolgozás előtt). Csak a
    // LED-ekhez ad becslést — a tényleges gate/comp a chainben dolgozik.
    void updateMeters (const juce::dsp::AudioBlock<float>& block) noexcept
    {
        const int n = (int) block.getNumSamples();
        if (n <= 0 || block.getNumChannels() == 0)
            return;

        const auto* d = block.getChannelPointer (0);
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = juce::jmax (peak, std::abs (d[i]));

        const float lvl = peak * gainLin;                 // a gate-et érő szint becslése
        if (lvl > dispEnv) dispEnv = lvl;                 // gyors fel
        else               dispEnv = dispEnv * 0.9f + lvl * 0.1f;  // lassú le

        const float lvlDb = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, dispEnv));

        // Kapu: nyit, ha a szint a küszöb felett van (simítva).
        const float gateTarget = (lvlDb > gateThrDb) ? 1.0f : 0.0f;
        gateSm = gateSm * 0.8f + gateTarget * 0.2f;
        gateDisp.store (gateOn ? gateSm : 1.0f);

        // Kompresszor becsült gain-csökkentése (dB) -> 0..1 a LED-hez.
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
        // Szinttartó szaturáció: pre = drive, post = 1/drive  ->  y = tanh(drive*x)/drive.
        // Kis jelnél ≈ egységnyi (finom), nagy jelnél lágy tanh-levágás.
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

    // Kijelző-mérő állapot (csak a hangszálon írt, a UI atomikusan olvassa).
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
