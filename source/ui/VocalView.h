#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GuitarLookAndFeel.h"
#include "Panels.h"
#include "AppView.h"

/**
    Ének (Vocal) nézet: a guitarDSP stílusú moduláris panelsor az ének jelúthoz.
    Felső sáv: cím + "menü" gomb (vissza a Landing képernyőre) + latencia-kijelző.

    Panelek (a jelút sorrendjében):
      INPUT (Gain) · LOW CUT (90 Hz, fix) · COMP (thresh/ratio) ·
      AIR (6 kHz shelf) · REVERB (mix) · LIMITER (-0.1 dB, fix)
*/
class VocalView : public AppView,
                  private juce::Timer
{
public:
    explicit VocalView (GuitarDspProcessor& p) : processorRef (p)
    {
        titleLabel.setText ("VOCAL", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (juce::FontOptions (19.0f, juce::Font::bold)));
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (GuitarLookAndFeel::cAccent));
        addAndMakeVisible (titleLabel);

        menuButton.setColour (juce::TextButton::buttonColourId,  juce::Colour (GuitarLookAndFeel::cPanelHead));
        menuButton.setColour (juce::TextButton::textColourOffId, juce::Colour (GuitarLookAndFeel::cText));
        menuButton.onClick = [this] { if (onBackToMenu) onBackToMenu(); };
        addAndMakeVisible (menuButton);

        latencyLabel.setJustificationType (juce::Justification::centredRight);
        latencyLabel.setColour (juce::Label::textColourId, juce::Colour (GuitarLookAndFeel::cAccent));
        latencyLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        addAndMakeVisible (latencyLabel);

        infoLabel.setJustificationType (juce::Justification::centredLeft);
        infoLabel.setColour (juce::Label::textColourId, juce::Colour (GuitarLookAndFeel::cTextDim));
        infoLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
        infoLabel.setText ("Élő ének — SM58 / Scarlett · brickwall limiter a clip ellen",
                           juce::dontSendNotification);
        addAndMakeVisible (infoLabel);

        auto& s = processorRef.apvts;
        panels.add (new ModulePanel (s, "INPUT", {}, { { "vocGain", "GAIN" } }));
        panels.add (new InfoPanel  ("LOW CUT", "90 Hz"));
        panels.add (new ModulePanel (s, "COMP", "vocCompOn",
                                     { { "vocCompThresh", "THRESH" }, { "vocCompRatio", "RATIO" } }));
        panels.add (new ModulePanel (s, "AIR", "vocAirOn", { { "vocAir", "AIR" } }));
        panels.add (new ModulePanel (s, "REVERB", "vocReverbOn", { { "vocReverbMix", "MIX" } }));
        panels.add (new InfoPanel  ("LIMITER", "-0.1 dB"));

        for (auto* pnl : panels) addAndMakeVisible (pnl);

        startTimerHz (12);
    }

    ~VocalView() override { stopTimer(); }

    int defaultWidth()  const override { return 740; }
    int defaultHeight() const override { return 280; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (GuitarLookAndFeel::cBackground));
        g.setColour (juce::Colour (GuitarLookAndFeel::cPanelHead));
        g.fillRect (0, 46, getWidth(), 1);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);

        auto top = area.removeFromTop (32);
        menuButton.setBounds (top.removeFromLeft (84).reduced (0, 2));
        top.removeFromLeft (10);
        titleLabel.setBounds (top.removeFromLeft (120));
        latencyLabel.setBounds (top.removeFromRight (150));

        area.removeFromTop (12);

        auto bottom = area.removeFromBottom (18);
        infoLabel.setBounds (bottom);
        area.removeFromBottom (6);

        // Egy panelsor a jelút sorrendjében.
        int x = area.getX();
        for (auto* pnl : panels)
        {
            const int w = pnl->getPreferredWidth();
            pnl->setBounds (x, area.getY(), w, area.getHeight());
            x += w + 8;
        }
    }

private:
    void timerCallback() override
    {
        const double sr = processorRef.getSampleRate();
        if (sr > 0.0)
        {
            // Az ének lánc nulla-latenciás; csak a ki/be pufferelés (~2 blokk) számít.
            const double samples = 2.0 * processorRef.getCurrentBlockSize();
            latencyLabel.setText ("Latency ~ " + juce::String (samples / sr * 1000.0, 1) + " ms",
                                  juce::dontSendNotification);
        }
    }

    GuitarDspProcessor& processorRef;

    juce::Label      titleLabel;
    juce::TextButton menuButton { "‹ MENÜ" };
    juce::Label      latencyLabel;
    juce::Label      infoLabel;

    juce::OwnedArray<PanelBase> panels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VocalView)
};
