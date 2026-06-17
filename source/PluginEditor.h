#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "TunerComponent.h"
#include "ui/GuitarLookAndFeel.h"
#include "ui/Panels.h"

/**
    Modern, sötét guitarDSP UI (Neural DSP-stílus): felső sáv (cím + modellválasztó
    + tuner), moduláris panelek a jelút sorrendjében, 9-sávos grafikus EQ.
*/
class GuitarDspEditor  : public juce::AudioProcessorEditor
{
public:
    explicit GuitarDspEditor (GuitarDspProcessor& p);
    ~GuitarDspEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void populateModelSelector();
    void updateStatusLabel();
    void buildPanels();
    void layoutRow (juce::Rectangle<int> area, juce::OwnedArray<PanelBase>& panels);

    GuitarDspProcessor& processorRef;
    GuitarLookAndFeel   lnf;

    juce::Label      titleLabel;
    juce::ComboBox   modelSelector;
    juce::Label      statusLabel;
    juce::TextButton tunerButton { "TUNER" };
    TunerComponent   tuner;

    juce::OwnedArray<PanelBase> row1, row2;

    juce::Array<juce::File> modelFiles;
    bool tunerVisible { false };

    static constexpr int baseW = 880;
    static constexpr int baseH = 470;
    static constexpr int tunerH = 138;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDspEditor)
};
