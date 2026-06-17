#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/**
    Minimál placeholder szerkesztő (fázis 1).

    A teljes, Neural DSP-stílusú UI későbbi fázis. Most a beépített
    GenericAudioProcessorEditor jeleníti meg az összes paramétert csúszkaként,
    hogy a jelút azonnal tesztelhető legyen. Felül egy státuszsor mutatja a
    betöltött NAM modellt és IR-t.
*/
class GuitarDspEditor  : public juce::AudioProcessorEditor
{
public:
    explicit GuitarDspEditor (GuitarDspProcessor& p);
    ~GuitarDspEditor() override = default;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    GuitarDspProcessor& processorRef;
    juce::Label statusLabel;
    juce::GenericAudioProcessorEditor genericEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDspEditor)
};
