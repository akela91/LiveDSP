#pragma once

#include <JuceHeader.h>

/**
    Élő ének-feldolgozó lánc halk dinamikus mikrofonhoz (pl. Shure SM58) +
    Focusrite Scarlett hangkártyához. Alacsony latenciás, nulla-latenciás
    feldolgozó elemekből (IIR / kompresszor / reverb / limiter), kifejezetten
    live monitorozáshoz — nincs zenei alap, csak a mikrofonjel megy a kimenetre.

    A jelút SZIGORÚAN ebben a sorrendben (juce::dsp::ProcessorChain):
      1. Input Gain  — digitális előerősítő (0 .. +24 dB).
      2. Low-Cut     — high-pass szűrő, FIXEN 90 Hz (puffogás + proximity).
      3. Compressor  — énekre optimalizálva; Attack 5 ms / Release 100 ms FIX,
                       Threshold (-40..0 dB) és Ratio (1:1..10:1) állítható.
      4. High-Shelf  — FIXEN 6 kHz "air/presence" emelés (0 .. +12 dB).
      5. Reverb      — közepes hall (FIX room/damp), Wet/Dry mix (0..100%).
      6. Limiter     — brickwall a lánc legvégén, FIX -0.1 dB ceiling (clip-védelem).

    Sztereó blokkon dolgozik (a mono mikrofonjel mindkét csatornára másolva),
    így a reverb teret kap. A prepare/reset ÜZENETSZÁLRÓL hívandó.
*/
class VocalChain
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        chain.prepare (spec);

        // Fix szűrők.
        updateLowCut();
        updateAir();

        // Kompresszor: gyors attack, közepes release (FIX); a többi élő.
        auto& comp = chain.get<compIdx>();
        comp.setAttack  (5.0f);
        comp.setRelease (100.0f);

        // Reverb: kellemes, énekhez illő közepes hall (FIX room/damp/width).
        updateReverb();

        // Brickwall limiter a lánc végén: FIX -0.1 dB ceiling.
        auto& lim = chain.get<limIdx>();
        lim.setThreshold (-0.1f);
        lim.setRelease   (50.0f);

        chain.reset();
    }

    void reset() { chain.reset(); }

    //==============================================================================
    // Élő paraméter-beállítók (a setter olcsó; a process előtt hívható).
    void setInputGainDb (float db) noexcept { chain.get<gainIdx>().setGainDecibels (db); }

    void setCompThreshold (float db) noexcept { chain.get<compIdx>().setThreshold (db); }
    void setCompRatio     (float r)  noexcept { chain.get<compIdx>().setRatio (r); }

    void setAirDb (float db) noexcept
    {
        if (db != airDb) { airDb = db; updateAir(); }
    }

    // mix: 0..1 (Wet/Dry arány).
    void setReverbMix (float mix) noexcept
    {
        mix = juce::jlimit (0.0f, 1.0f, mix);
        if (mix != reverbMix) { reverbMix = mix; updateReverb(); }
    }

    // Bypass-kapcsolók (a ProcessorChain elemenként megkerülhető).
    void setCompEnabled   (bool b) noexcept { chain.setBypassed<compIdx>  (! b); }
    void setAirEnabled    (bool b) noexcept { chain.setBypassed<shelfIdx> (! b); }
    void setReverbEnabled (bool b) noexcept { chain.setBypassed<revIdx>   (! b); }

    //==============================================================================
    void process (juce::dsp::AudioBlock<float> block) noexcept
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        chain.process (ctx);
    }

private:
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

    void updateReverb()
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize   = 0.5f;          // közepes hall
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
    using Comp   = juce::dsp::Compressor<float>;
    using Verb   = juce::dsp::Reverb;
    using Lim    = juce::dsp::Limiter<float>;

    enum { gainIdx = 0, hpfIdx, compIdx, shelfIdx, revIdx, limIdx };
    juce::dsp::ProcessorChain<Gain, Filter, Comp, Filter, Verb, Lim> chain;

    double sampleRate { 48000.0 };
    float  airDb      { 0.0f };
    float  reverbMix  { 0.2f };
};
