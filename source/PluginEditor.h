#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "TunerComponent.h"

/**
    Fázis-1 szerkesztő: modellválasztó + státuszsor + Tuner kapcsoló + a beépített
    GenericAudioProcessorEditor a paraméterekhez. A teljes egyedi UI későbbi fázis.
*/
class GuitarDspEditor  : public juce::AudioProcessorEditor
{
public:
    explicit GuitarDspEditor (GuitarDspProcessor& p);
    ~GuitarDspEditor() override = default;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void populateModelSelector();
    void updateStatusLabel();

    GuitarDspProcessor& processorRef;

    juce::ComboBox   modelSelector;
    juce::Label      modelLabel { {}, "Amp:" };
    juce::Label      statusLabel;
    juce::TextButton tunerButton { "Tuner" };
    TunerComponent   tuner;
    juce::GenericAudioProcessorEditor genericEditor;

    juce::Array<juce::File> modelFiles;
    bool tunerVisible { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDspEditor)
};
