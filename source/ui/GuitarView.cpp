#include "GuitarView.h"

GuitarView::GuitarView (GuitarDspProcessor& p)
    : processorRef (p),
      tuner (p)
{
    titleLabel.setText ("guitarDSP", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (19.0f, juce::Font::bold)));
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

    presetSelector.setTextWhenNothingSelected (juce::String::fromUTF8 ("preset…"));
    populatePresetSelector();
    presetSelector.onChange = [this]
    {
        const int idx = presetSelector.getSelectedId() - 1;
        if (idx >= 0 && idx < presetFiles.size())
        {
            juce::MemoryBlock mb;
            if (presetFiles[idx].loadFileAsData (mb))
                processorRef.setStateInformation (mb.getData(), (int) mb.getSize());
            updateStatusLabel();
        }
    };
    addAndMakeVisible (presetSelector);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (GuitarLookAndFeel::cTextDim));
    statusLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (statusLabel);
    updateStatusLabel();

    latencyLabel.setJustificationType (juce::Justification::centredRight);
    latencyLabel.setColour (juce::Label::textColourId, juce::Colour (GuitarLookAndFeel::cAccent));
    latencyLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    addAndMakeVisible (latencyLabel);

    menuButton.setColour (juce::TextButton::buttonColourId,  juce::Colour (GuitarLookAndFeel::cPanelHead));
    menuButton.setColour (juce::TextButton::textColourOffId, juce::Colour (GuitarLookAndFeel::cText));
    menuButton.onClick = [this] { if (onBackToMenu) onBackToMenu(); };
    addAndMakeVisible (menuButton);

    tunerButton.setClickingTogglesState (true);
    tunerButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (GuitarLookAndFeel::cPanelHead));
    tunerButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (GuitarLookAndFeel::cAccent));
    tunerButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (GuitarLookAndFeel::cText));
    tunerButton.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
    tunerButton.onClick = [this]
    {
        tunerVisible = tunerButton.getToggleState();
        tuner.setVisible (tunerVisible);
        if (onRequestSize)
            onRequestSize (baseW, baseH + (tunerVisible ? tunerH : 0));
    };
    addAndMakeVisible (tunerButton);

    addChildComponent (tuner);

    buildPanels();

    startTimerHz (12);
}

GuitarView::~GuitarView()
{
    stopTimer();
}

void GuitarView::buildPanels()
{
    // 1. sor — a jelút eleje: bemenet, dinamika, hangmagasság, drive, amp-fej.
    row1.add (new InputPanel (processorRef.apvts, "inputGain", "IN",
                              processorRef.getTotalNumInputChannels(),
                              processorRef.getGuitarInputCh(),
                              [this] (int ch) { processorRef.setGuitarInputCh (ch); }));
    row1.add (new ModulePanel (processorRef.apvts, "GATE",   "gateOn",  { { "gateThreshold", "THRESH" } }));
    row1.add (new PitchPanel (processorRef.apvts));
    row1.add (new ModulePanel (processorRef.apvts, "DRIVE",  "driveOn",
                               { { "driveAmount", "DRIVE" }, { "driveTone", "TONE" }, { "driveLevel", "LEVEL" } }));

    auto* ampPanel = new ModulePanel (processorRef.apvts, "AMP", "namOn", {});
    ampPanel->setIcon (ModulePanel::Icon::amp);
    row1.add (ampPanel);

    // 2. sor — a jelút vége: hangláda, EQ, idő-FX, kimenet (a természetes
    // AMP|CAB töréspontnál tördelve, így mindkét sor kitöltött és kiegyensúlyozott).
    auto* cabPanel = new ModulePanel (processorRef.apvts, "CAB", "cabOn", {});
    cabPanel->setIcon (ModulePanel::Icon::cab);
    row2.add (cabPanel);

    row2.add (new EqPanel (processorRef.apvts,
                           { "65", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" }));
    row2.add (new ModulePanel (processorRef.apvts, "DELAY",  "delayOn",
                               { { "delayTime", "TIME" }, { "delayFeedback", "FB" }, { "delayMix", "MIX" } }));
    row2.add (new ModulePanel (processorRef.apvts, "REVERB", "reverbOn",
                               { { "reverbAmount", "MIX" } }));
    row2.add (new ModulePanel (processorRef.apvts, "OUTPUT", {},        { { "outputGain", "OUT" } }));

    for (auto* pnl : row1) addAndMakeVisible (pnl);
    for (auto* pnl : row2) addAndMakeVisible (pnl);
}

void GuitarView::populateModelSelector()
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

void GuitarView::populatePresetSelector()
{
   #if defined (GUITARDSP_FAVS_DIR)
    juce::File dir { GUITARDSP_FAVS_DIR };
    if (dir.isDirectory())
        presetFiles = dir.findChildFiles (juce::File::findFiles, false, "*");
   #endif

    int id = 1;
    for (const auto& f : presetFiles)
        presetSelector.addItem (f.getFileName(), id++);
}

void GuitarView::updateStatusLabel()
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

void GuitarView::timerCallback()
{
    const double sr = processorRef.getSampleRate();
    if (sr > 0.0)
    {
        // Becsült input->output késleltetés: az AKTÍV modulok latenciája
        // (dinamikus, követi a pitch kapcsolót) + a ki/be pufferelés (~2 blokk).
        const double samples = processorRef.getEffectiveLatencySamples()
                               + 2.0 * processorRef.getCurrentBlockSize();
        latencyLabel.setText ("Latency ~ " + juce::String (samples / sr * 1000.0, 1) + " ms",
                              juce::dontSendNotification);
    }
}

void GuitarView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (GuitarLookAndFeel::cBackground));
    g.setColour (juce::Colour (GuitarLookAndFeel::cPanelHead));
    g.fillRect (0, 46, getWidth(), 1);
}

void GuitarView::layoutRow (juce::Rectangle<int> area, juce::OwnedArray<PanelBase>& panels)
{
    if (panels.isEmpty())
        return;

    // A panelek arányosan KITÖLTIK a sor teljes szélességét (a preferált
    // szélességek arányában), így nincs üres jobb oldal és nem zsúfolt.
    const int gap = 8;
    int totalPref = 0;
    for (auto* pnl : panels)
        totalPref += pnl->getPreferredWidth();

    const int avail = area.getWidth() - gap * (panels.size() - 1);
    int x = area.getX();
    for (int i = 0; i < panels.size(); ++i)
    {
        const int w = (i == panels.size() - 1)
                        ? (area.getRight() - x)   // utolsó: a maradék (kerekítés-mentes)
                        : juce::roundToInt (avail * (panels[i]->getPreferredWidth() / (float) totalPref));
        panels[i]->setBounds (x, area.getY(), w, area.getHeight());
        x += w + gap;
    }
}

void GuitarView::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto top = area.removeFromTop (32);
    tunerButton.setBounds (top.removeFromRight (84).reduced (0, 2));
    top.removeFromRight (8);
    menuButton.setBounds (top.removeFromRight (72).reduced (0, 2));
    top.removeFromRight (8);
    presetSelector.setBounds (top.removeFromRight (150).reduced (0, 2));
    top.removeFromRight (8);
    titleLabel.setBounds (top.removeFromLeft (108));
    modelSelector.setBounds (top.reduced (4, 2));

    area.removeFromTop (12);

    auto bottom = area.removeFromBottom (18);
    latencyLabel.setBounds (bottom.removeFromRight (150));
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
