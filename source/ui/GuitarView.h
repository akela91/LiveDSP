#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../TunerComponent.h"
#include "GuitarLookAndFeel.h"
#include "Panels.h"
#include "AppView.h"

/**
    Gitár nézet: a teljes guitarDSP amp-szimulátor UI (felső sáv cím + modell-
    és preset-választó + tuner + "menü" gomb, moduláris panelek a jelút
    sorrendjében, 9-sávos EQ, élő latencia-kijelző).

    Korábban ez maga a GuitarDspEditor volt; mostantól egy AppView, amelyet a
    shell-szerkesztő gitár módban mutat.
*/
class GuitarView : public AppView,
                   private juce::Timer
{
public:
    explicit GuitarView (GuitarDspProcessor& p);
    ~GuitarView() override;

    int  defaultWidth()   const override { return baseW; }
    int  defaultHeight()  const override { return baseH; }
    bool wantsResizable() const override { return true; }

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void populateModelSelector();
    void populatePresetSelector();
    void updateStatusLabel();
    void buildPanels();
    void layoutRow (juce::Rectangle<int> area, juce::OwnedArray<PanelBase>& panels);
    void timerCallback() override;

    GuitarDspProcessor& processorRef;

    juce::Label      titleLabel;
    juce::ComboBox   modelSelector;
    juce::ComboBox   presetSelector;
    juce::Label      statusLabel;
    juce::Label      latencyLabel;
    juce::TextButton tunerButton { "TUNER" };
    juce::TextButton menuButton  { "‹ MENÜ" };
    TunerComponent   tuner;

    juce::OwnedArray<PanelBase> row1, row2;

    juce::Array<juce::File> modelFiles;
    juce::Array<juce::File> presetFiles;
    bool tunerVisible { false };

    static constexpr int baseW = 960;
    static constexpr int baseH = 470;
    static constexpr int tunerH = 138;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarView)
};
