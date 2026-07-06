#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LiveLookAndFeel.h"
#include "Panels.h"
#include "AppView.h"
#include "SupportButton.h"

/**
    Vocal view: the GuitarDSP-style modular panel row for the vocal signal chain.
    Top bar: title + "menu" button (back to the Landing screen) + latency display.

    Panels (in signal chain order):
      INPUT (Gain) · LOW CUT (90 Hz, fixed) · COMP (thresh/ratio) ·
      AIR (6 kHz shelf) · REVERB (mix) · LIMITER (-0.1 dB, fixed)
*/
class VoiceView : public AppView,
                  private juce::Timer
{
public:
    explicit VoiceView (LiveDspProcessor& p) : processorRef (p)
    {
        titleLabel.setText ("VoiceDSP", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (juce::FontOptions (19.0f, juce::Font::bold)));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (LiveLookAndFeel::cAccent));
        addAndMakeVisible (titleLabel);

        menuButton.setColour (juce::TextButton::buttonColourId,  juce::Colour (LiveLookAndFeel::cPanelHead));
        menuButton.setColour (juce::TextButton::textColourOffId, juce::Colour (LiveLookAndFeel::cText));
        menuButton.onClick = [this] { if (onBackToMenu) onBackToMenu(); };
        addAndMakeVisible (menuButton);

        latencyLabel.setJustificationType (juce::Justification::centredRight);
        latencyLabel.setColour (juce::Label::textColourId, juce::Colour (LiveLookAndFeel::cAccent));
        latencyLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        addAndMakeVisible (latencyLabel);

        infoLabel.setJustificationType (juce::Justification::centredLeft);
        infoLabel.setColour (juce::Label::textColourId, juce::Colour (LiveLookAndFeel::cTextDim));
        infoLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
        infoLabel.setText (juce::String::fromUTF8 ("Live vocals — SM58 / Scarlett · brickwall limiter against clipping"),
                           juce::dontSendNotification);
        addAndMakeVisible (infoLabel);

        recButton.onClick = [this]
        {
            if (processorRef.isRecording())
                processorRef.stopRecording();
            else
                processorRef.startRecording();
            updateRecButton (processorRef.isRecording());
        };
        updateRecButton (processorRef.isRecording());
        addAndMakeVisible (recButton);

        addAndMakeVisible (coffeeButton);

        auto& s = processorRef.apvts;

        // Row 1 — input, filter, gate, warmth, compressor.
        inputPanel = new InputPanel (s, "vocGain", "GAIN",
                                     processorRef.getTotalNumInputChannels(),
                                     processorRef.getVocalInputCh(),
                                     [this] (int ch) { processorRef.setVocalInputCh (ch); });
        row1.add (inputPanel);
        row1.add (new InfoPanel  ("LOW CUT", "90 Hz"));
        gatePanel = new ModulePanel (s, "GATE", "vocGateOn", { { "vocGateThresh", "GATE" } });
        gatePanel->enableLed (true);
        row1.add (gatePanel);

        row1.add (new ModulePanel (s, "WARMTH", "vocWarmthOn", { { "vocWarmth", "WARMTH" } }));

        compPanel = new ModulePanel (s, "COMP", "vocCompOn",
                                     { { "vocCompThresh", "THRESH" }, { "vocCompRatio", "RATIO" } });
        compPanel->enableLed (true);
        row1.add (compPanel);

        // Row 2 — autotune, air, delay, reverb, limiter.
        autotunePanel = new AutotunePanel (s);
        row2.add (autotunePanel);
        row2.add (new ModulePanel (s, "AIR", "vocAirOn", { { "vocAir", "AIR" } }));
        row2.add (new ModulePanel (s, "DELAY", "vocDelayOn",
                                   { { "vocDelayTime", "TIME" }, { "vocDelayMix", "MIX" } }));
        row2.add (new ModulePanel (s, "REVERB", "vocReverbOn", { { "vocReverbMix", "MIX" } }));
        row2.add (new InfoPanel  ("LIMITER", "-0.1 dB"));

        for (auto* pnl : row1) addAndMakeVisible (pnl);
        for (auto* pnl : row2) addAndMakeVisible (pnl);

        startTimerHz (12);
    }

    ~VoiceView() override { stopTimer(); }

    int defaultWidth()  const override { return 900; }
    int defaultHeight() const override { return 440; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (LiveLookAndFeel::cBackground));
        g.setColour (juce::Colour (LiveLookAndFeel::cPanelHead));
        g.fillRect (0, 46, getWidth(), 1);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        auto top = area.removeFromTop (32);
        menuButton.setBounds (top.removeFromLeft (84).reduced (0, 2));
        top.removeFromLeft (10);
        titleLabel.setBounds (top.removeFromLeft (120));
        coffeeButton.setBounds (top.removeFromRight (140).reduced (0, 3));
        top.removeFromRight (10);
        recButton.setBounds (top.removeFromRight (64).reduced (0, 2));
        top.removeFromRight (8);
        latencyLabel.setBounds (top.removeFromRight (150));

        area.removeFromTop (12);

        auto bottom = area.removeFromBottom (18);
        infoLabel.setBounds (bottom);
        area.removeFromBottom (6);

        // Two panel rows, each filling the width proportionally.
        auto r1 = area.removeFromTop (juce::jmin (area.getHeight() / 2, 150));
        area.removeFromTop (10);
        layoutRow (r1, row1);
        layoutRow (area, row2);
    }

private:
    // Proportional width fill (like the rows in the guitar view).
    static void layoutRow (juce::Rectangle<int> area, juce::OwnedArray<PanelBase>& panels)
    {
        if (panels.isEmpty())
            return;

        const int gap = 10;
        int totalPref = 0;
        for (auto* pnl : panels) totalPref += pnl->getPreferredWidth();
        const int avail = area.getWidth() - gap * (panels.size() - 1);

        int x = area.getX();
        for (int i = 0; i < panels.size(); ++i)
        {
            const int w = (i == panels.size() - 1)
                            ? (area.getRight() - x)
                            : juce::roundToInt (avail * (panels[i]->getPreferredWidth() / (float) totalPref));
            panels[i]->setBounds (x, area.getY(), w, area.getHeight());
            x += w + gap;
        }
    }

    // REC button visuals (red while recording).
    void updateRecButton (bool recording)
    {
        recLit = recording;
        recButton.setButtonText (recording ? "STOP" : "REC");
        recButton.setColour (juce::TextButton::buttonColourId,
                             recording ? juce::Colours::red.darker (0.25f)
                                       : juce::Colour (LiveLookAndFeel::cPanelHead));
        recButton.setColour (juce::TextButton::textColourOffId,
                             recording ? juce::Colours::white
                                       : juce::Colour (LiveLookAndFeel::cText));
    }

    void timerCallback() override
    {
        const double sr = processorRef.getSampleRate();
        if (sr > 0.0)
        {
            // The vocal chain is zero-latency; only Autotune (when on) + the in/out
            // buffering (~2 blocks) matter.
            const double samples = processorRef.getEffectiveLatencySamples()
                                   + 2.0 * processorRef.getCurrentBlockSize();
            latencyLabel.setText ("Latency ~ " + juce::String (samples / sr * 1000.0, 1) + " ms",
                                  juce::dontSendNotification);
        }

        // LEDs: the gate lights up when muting, the compressor when reducing gain.
        if (gatePanel != nullptr)
            gatePanel->setLedLevel (1.0f - processorRef.getVocalGateGain());
        if (compPanel != nullptr)
            compPanel->setLedLevel (processorRef.getVocalCompReduction());

        // INPUT level meter.
        if (inputPanel != nullptr)
            inputPanel->setInputLevel (processorRef.getInputLevel());

        // Autotune readout: detected note + applied correction. getDisplayMidi()
        // is -1 when the effect is off or no pitch is held, so the panel shows a
        // dash in those cases without extra state checks here.
        if (autotunePanel != nullptr)
        {
            auto& at = processorRef.getAutotune();
            autotunePanel->setPitchReadout (at.getDisplayMidi(), at.getDisplayCorrection());
        }

        // Keep the REC button visuals in sync if recording was stopped elsewhere
        // (e.g. by a mode switch).
        if (processorRef.isRecording() != recLit)
            updateRecButton (processorRef.isRecording());
    }

    LiveDspProcessor& processorRef;

    juce::Label      titleLabel;
    juce::TextButton menuButton { juce::String::fromUTF8 ("‹ MENU") };
    juce::TextButton recButton  { "REC" };
    juce::Label      latencyLabel;
    juce::Label      infoLabel;
    CoffeeButton     coffeeButton;
    bool             recLit { false };   // cached REC state, so the timer only repaints on change

    juce::OwnedArray<PanelBase> row1, row2;
    ModulePanel*   gatePanel     { nullptr };
    ModulePanel*   compPanel     { nullptr };
    InputPanel*    inputPanel    { nullptr };
    AutotunePanel* autotunePanel { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceView)
};
