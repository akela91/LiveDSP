#include "PluginEditor.h"

GuitarDspEditor::GuitarDspEditor (GuitarDspProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p),
      tuner (p)
{
    setLookAndFeel (&lnf);

    titleLabel.setText ("guitarDSP", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (GuitarLookAndFeel::cAccent));
    addAndMakeVisible (titleLabel);

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
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (GuitarLookAndFeel::cTextDim));
    statusLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (statusLabel);
    updateStatusLabel();

    tunerButton.setClickingTogglesState (true);
    tunerButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (GuitarLookAndFeel::cPanelHead));
    tunerButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (GuitarLookAndFeel::cAccent));
    tunerButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (GuitarLookAndFeel::cText));
    tunerButton.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
    tunerButton.onClick = [this]
    {
        tunerVisible = tunerButton.getToggleState();
        tuner.setVisible (tunerVisible);
        setSize (baseW, baseH + (tunerVisible ? tunerH : 0));
    };
    addAndMakeVisible (tunerButton);

    addChildComponent (tuner);

    buildPanels();

    setSize (baseW, baseH);
    setResizable (true, true);
    setResizeLimits (820, 440, 1400, 1000);
}

GuitarDspEditor::~GuitarDspEditor()
{
    setLookAndFeel (nullptr);
}

void GuitarDspEditor::buildPanels()
{
    // 1. sor — jelút eleje
    row1.add (new ModulePanel (processorRef.apvts, "INPUT",  {},        { { "inputGain", "IN" } }));
    row1.add (new ModulePanel (processorRef.apvts, "GATE",   "gateOn",  { { "gateThreshold", "THRESH" } }));
    row1.add (new ModulePanel (processorRef.apvts, "PITCH",  "pitchOn", { { "pitchSemitones", "SEMI" } }));
    row1.add (new ModulePanel (processorRef.apvts, "DRIVE",  "driveOn",
                               { { "driveAmount", "DRIVE" }, { "driveTone", "TONE" }, { "driveLevel", "LEVEL" } }));
    row1.add (new ModulePanel (processorRef.apvts, "AMP",    "namOn",   {}));
    row1.add (new ModulePanel (processorRef.apvts, "CAB",    "cabOn",   {}));
    row1.add (new ModulePanel (processorRef.apvts, "OUTPUT", {},        { { "outputGain", "OUT" } }));

    // 2. sor — EQ + idő-FX
    row2.add (new EqPanel (processorRef.apvts,
                           { "65", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" }));
    row2.add (new ModulePanel (processorRef.apvts, "DELAY",  "delayOn",
                               { { "delayTime", "TIME" }, { "delayFeedback", "FB" }, { "delayMix", "MIX" } }));
    row2.add (new ModulePanel (processorRef.apvts, "REVERB", "reverbOn",
                               { { "reverbAmount", "MIX" } }));

    for (auto* pnl : row1) addAndMakeVisible (pnl);
    for (auto* pnl : row2) addAndMakeVisible (pnl);
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

    statusLabel.setText ("Amp: " + namName + "    |    Cab IR: " + irName,
                         juce::dontSendNotification);
}

void GuitarDspEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (GuitarLookAndFeel::cBackground));

    // felső sáv finom elválasztó
    g.setColour (juce::Colour (GuitarLookAndFeel::cPanelHead));
    g.fillRect (0, 46, getWidth(), 1);
}

void GuitarDspEditor::layoutRow (juce::Rectangle<int> area, juce::OwnedArray<PanelBase>& panels)
{
    int x = area.getX();
    for (auto* pnl : panels)
    {
        const int w = pnl->getPreferredWidth();
        pnl->setBounds (x, area.getY(), w, area.getHeight());
        x += w + 8;
    }
}

void GuitarDspEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto top = area.removeFromTop (32);
    tunerButton.setBounds (top.removeFromRight (90).reduced (0, 2));
    top.removeFromRight (10);
    titleLabel.setBounds (top.removeFromLeft (150));
    modelSelector.setBounds (top.reduced (4, 2));

    area.removeFromTop (12);

    auto bottom = area.removeFromBottom (18);
    statusLabel.setBounds (bottom);
    area.removeFromBottom (6);

    if (tunerVisible)
    {
        tuner.setBounds (area.removeFromTop (tunerH - 8));
        area.removeFromTop (8);
    }

    auto r1 = area.removeFromTop (132);
    area.removeFromTop (10);
    auto r2 = area;

    layoutRow (r1, row1);
    layoutRow (r2, row2);
}
