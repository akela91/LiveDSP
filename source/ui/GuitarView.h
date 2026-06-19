#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../TunerComponent.h"
#include "LiveLookAndFeel.h"
#include "Panels.h"
#include "AppView.h"
#include "SupportButton.h"

/**
    Guitar view: the full GuitarDSP amp-simulator UI (top bar with title + model
    and preset selectors + tuner + "menu" button, modular panels in signal-chain
    order, 9-band EQ, live latency readout).

    This used to be the LiveDspEditor itself; it is now an AppView that the shell
    editor shows in guitar mode.
*/
class GuitarView : public AppView,
                   private juce::Timer
{
public:
    explicit GuitarView (LiveDspProcessor& p);
    ~GuitarView() override;

    int  defaultWidth()   const override { return baseW; }
    int  defaultHeight()  const override { return baseH; }
    bool wantsResizable() const override { return true; }

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void buildPanels();
    void populateAmpModels();
    void importAmpModel (const juce::File& source);   // copy an external .nam in and load it
    void populateCabIrs();
    void importCabIr (const juce::File& source);     // copy an external .wav IR in and load it
    void populatePresetSelector();
    void updateStatusLabel();
    void syncSelectors();   // keep the AMP/RIG + CAB combos in sync with the loaded model/IR
    void layoutRow (juce::Rectangle<int> area, juce::OwnedArray<PanelBase>& panels);
    void timerCallback() override;

    LiveDspProcessor& processorRef;

    juce::Label      titleLabel;
    juce::ComboBox   presetSelector;
    juce::Label      statusLabel;
    juce::Label      latencyLabel;
    juce::TextButton tunerButton { "TUNER" };
    juce::TextButton menuButton  { juce::String::fromUTF8 ("‹ MENU") };
    CoffeeButton     coffeeButton;
    TunerComponent   tuner;

    juce::OwnedArray<PanelBase> row1, row2;

    // The model/IR selector combo boxes live inside these panels (file logic stays here).
    ComboPanel*  ampPanel   { nullptr };
    ComboPanel*  cabPanel   { nullptr };
    ModulePanel* gatePanel  { nullptr };
    InputPanel*  inputPanel { nullptr };

    juce::Array<juce::File> modelFiles;
    juce::Array<juce::File> irFiles;
    juce::Array<juce::File> presetFiles;
    bool tunerVisible { false };

    static constexpr int baseW = 960;
    static constexpr int baseH = 470;
    static constexpr int tunerH = 138;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarView)
};
