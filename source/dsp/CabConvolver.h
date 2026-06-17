#pragma once

#include <JuceHeader.h>

/**
    IR (impulzusválasz) cab-szimulátor a juce::dsp::Convolution köré,
    ZERO-LATENCY módban (Latency{0}) — élő játékhoz.

    FONTOS: a "Full Rig" NAM modellek MÁR tartalmazzák a hangládát, ezért
    ilyenkor ezt a modult bypassolni kell (lásd setEnabled). Külön IR-t csak
    "amp-only" (preamp/DI) NAM modellhez érdemes betölteni.

    Sztereó jelen dolgozik (a NAM utáni mono jelet a processzor szélesíti
    sztereóvá ez előtt a modul előtt).
*/
class CabConvolver
{
public:
    CabConvolver()
        : convolution (juce::dsp::Convolution::Latency { 0 })
    {
    }

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        convolution.prepare (spec);
    }

    void reset() noexcept
    {
        convolution.reset();
    }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }
    bool isLoaded() const noexcept { return loaded; }
    juce::String getLoadedName() const { return loadedName; }

    /** .wav IR betöltése. Hívd üzenetszálról (fájl-IO!).
        A Convolution belsőleg háttérszálon dolgozza fel, és szálbiztosan
        cseréli az aktív IR-t. */
    bool loadIR (const juce::File& irFile)
    {
        if (! irFile.existsAsFile())
            return false;

        convolution.loadImpulseResponse (
            irFile,
            juce::dsp::Convolution::Stereo::yes,      // sztereóvá terjeszti, ha mono az IR
            juce::dsp::Convolution::Trim::yes,        // csendes farok levágása
            0,                                        // 0 = teljes IR hossz
            juce::dsp::Convolution::Normalise::yes);  // szintnormalizálás

        loadedName = irFile.getFileNameWithoutExtension();
        loaded = true;
        return true;
    }

    // Sztereó, in-place feldolgozás egy AudioBlock-on.
    void process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        if (! enabled || ! loaded)
            return;

        juce::dsp::ProcessContextReplacing<float> ctx (block);
        convolution.process (ctx);
    }

private:
    juce::dsp::Convolution convolution;
    bool enabled { false };          // alapból OFF (Full Rig NAM miatt)
    bool loaded  { false };
    juce::String loadedName;
};
