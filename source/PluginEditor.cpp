#include "PluginEditor.h"

GuitarDspEditor::GuitarDspEditor (GuitarDspProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      genericEditor (p)
{
    const auto namName = processorRef.getNam().isLoaded()
                            ? processorRef.getNam().getLoadedName()
                            : juce::String ("nincs modell");
    const auto irName  = processorRef.getCab().isLoaded()
                            ? processorRef.getCab().getLoadedName()
                            : juce::String ("nincs IR");

    statusLabel.setText ("Amp (NAM): " + namName + "   |   Cab (IR): " + irName,
                         juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setFont (juce::Font (juce::FontOptions (14.0f)));
    addAndMakeVisible (statusLabel);

    addAndMakeVisible (genericEditor);

    setSize (480, 600);
    setResizable (true, true);
}

void GuitarDspEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void GuitarDspEditor::resized()
{
    auto area = getLocalBounds();
    statusLabel.setBounds (area.removeFromTop (28).reduced (8, 4));
    genericEditor.setBounds (area);
}
