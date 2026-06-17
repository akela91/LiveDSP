#include "PluginEditor.h"

GuitarDspEditor::GuitarDspEditor (GuitarDspProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      tuner (p),
      genericEditor (p)
{
    modelLabel.setJustificationType (juce::Justification::centredRight);
    modelLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    addAndMakeVisible (modelLabel);

    modelSelector.setTextWhenNothingSelected ("nincs modell");
    populateModelSelector();
    modelSelector.onChange = [this]
    {
        const int idx = modelSelector.getSelectedId() - 1;
        if (idx >= 0 && idx < modelFiles.size())
        {
            processorRef.loadNamModel (modelFiles[idx]);   // szálbiztos csere
            updateStatusLabel();
        }
    };
    addAndMakeVisible (modelSelector);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (statusLabel);
    updateStatusLabel();

    tunerButton.setClickingTogglesState (true);
    tunerButton.onClick = [this]
    {
        tunerVisible = tunerButton.getToggleState();
        tuner.setVisible (tunerVisible);
        resized();
    };
    addAndMakeVisible (tunerButton);

    addChildComponent (tuner);   // kezdetben rejtett
    addAndMakeVisible (genericEditor);

    setSize (500, 660);
    setResizable (true, true);
}

void GuitarDspEditor::populateModelSelector()
{
   #if defined (GUITARDSP_DEFAULT_MODELS_DIR)
    juce::File dir { GUITARDSP_DEFAULT_MODELS_DIR };
    if (dir.isDirectory())
        modelFiles = dir.findChildFiles (juce::File::findFiles, true, "*.nam");
   #endif

    int id = 1;
    for (const auto& f : modelFiles)
        modelSelector.addItem (f.getFileNameWithoutExtension(), id++);

    // A jelenleg betöltött modell kiválasztása.
    const auto loaded = processorRef.getNam().getLoadedName();
    for (int i = 0; i < modelFiles.size(); ++i)
        if (modelFiles[i].getFileNameWithoutExtension() == loaded)
            modelSelector.setSelectedId (i + 1, juce::dontSendNotification);
}

void GuitarDspEditor::updateStatusLabel()
{
    const auto namName = processorRef.getNam().isLoaded()
                            ? processorRef.getNam().getLoadedName()
                            : juce::String ("nincs modell [") + processorRef.getNam().getLastStatus() + "]";
    const auto irName  = processorRef.getCab().isLoaded()
                            ? processorRef.getCab().getLoadedName()
                            : juce::String ("nincs IR");

    statusLabel.setText ("Amp (NAM): " + namName + "   |   Cab (IR): " + irName,
                         juce::dontSendNotification);
}

void GuitarDspEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void GuitarDspEditor::resized()
{
    auto area = getLocalBounds();

    // 1. sor: Amp: [modellválasztó] ............ [Tuner]
    auto row1 = area.removeFromTop (30);
    tunerButton.setBounds (row1.removeFromRight (90).reduced (4, 3));
    modelLabel.setBounds (row1.removeFromLeft (44).reduced (2, 3));
    modelSelector.setBounds (row1.reduced (2, 3));

    // 2. sor: státusz
    statusLabel.setBounds (area.removeFromTop (22).reduced (8, 2));

    if (tunerVisible)
        tuner.setBounds (area.removeFromTop (140).reduced (6, 4));

    genericEditor.setBounds (area);
}
