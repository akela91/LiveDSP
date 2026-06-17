#include "GuitarView.h"
#include "../AppPaths.h"

GuitarView::GuitarView (LiveDspProcessor& p)
    : processorRef (p),
      tuner (p)
{
    titleLabel.setText ("GuitarDSP", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (19.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (LiveLookAndFeel::cAccent));
    addAndMakeVisible (titleLabel);

    presetSelector.setTextWhenNothingSelected (juce::String::fromUTF8 ("preset…"));
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
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (LiveLookAndFeel::cTextDim));
    statusLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    addAndMakeVisible (statusLabel);

    latencyLabel.setJustificationType (juce::Justification::centredRight);
    latencyLabel.setColour (juce::Label::textColourId, juce::Colour (LiveLookAndFeel::cAccent));
    latencyLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    addAndMakeVisible (latencyLabel);

    menuButton.setColour (juce::TextButton::buttonColourId,  juce::Colour (LiveLookAndFeel::cPanelHead));
    menuButton.setColour (juce::TextButton::textColourOffId, juce::Colour (LiveLookAndFeel::cText));
    menuButton.onClick = [this] { if (onBackToMenu) onBackToMenu(); };
    addAndMakeVisible (menuButton);

    tunerButton.setClickingTogglesState (true);
    tunerButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (LiveLookAndFeel::cPanelHead));
    tunerButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (LiveLookAndFeel::cAccent));
    tunerButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (LiveLookAndFeel::cText));
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
    populateAmpModels();
    populateCabIrs();
    populatePresetSelector();
    updateStatusLabel();

    startTimerHz (12);
}

GuitarView::~GuitarView()
{
    stopTimer();
}

void GuitarView::buildPanels()
{
    // Row 1 — start of the signal chain: input, gate (with LED), pitch, drive, amp.
    row1.add (new InputPanel (processorRef.apvts, "inputGain", "IN",
                              processorRef.getTotalNumInputChannels(),
                              processorRef.getGuitarInputCh(),
                              [this] (int ch) { processorRef.setGuitarInputCh (ch); }));

    gatePanel = new ModulePanel (processorRef.apvts, "GATE", "gateOn", { { "gateThreshold", "THRESH" } });
    gatePanel->enableLed (true);
    row1.add (gatePanel);

    row1.add (new PitchPanel (processorRef.apvts));
    row1.add (new ModulePanel (processorRef.apvts, "DRIVE",  "driveOn",
                               { { "driveAmount", "DRIVE" }, { "driveTone", "TONE" }, { "driveLevel", "LEVEL" } }));

    ampPanel = new ComboPanel (processorRef.apvts, "AMP/RIG", "namOn", ComboPanel::Icon::amp);
    // Browse: import an external .nam rig; it is copied into the writable models
    // folder and loaded. The download link points to a free rig to try and is
    // shown only while no model is loaded yet (see updateStatusLabel()).
    ampPanel->enableBrowse ("*.nam", juce::String::fromUTF8 ("Import .nam…"),
                            [this] (const juce::File& f) { importAmpModel (f); });
    ampPanel->setDownloadLink (juce::String::fromUTF8 ("Download a rig to try →"),
                               juce::URL ("https://www.tone3000.com/tones/mesa-dual-rectifier-mw-red-modern-mesa-4x12-full-rig-69206"));
    row1.add (ampPanel);

    // Row 2 — end of the signal chain: cabinet (with IR selector), EQ, time-FX, output.
    cabPanel = new ComboPanel (processorRef.apvts, "CAB", "cabOn", ComboPanel::Icon::cab);
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

void GuitarView::populateAmpModels()
{
    if (ampPanel == nullptr) return;
    auto& combo = ampPanel->getCombo();

    // Idempotent: may be called again after importing a model.
    combo.clear (juce::dontSendNotification);
    modelFiles.clear();
    if (auto dir = livedsp::getModelsDir(); dir.isDirectory())
        modelFiles = dir.findChildFiles (juce::File::findFiles, true, "*.nam");

    int id = 1;
    for (const auto& f : modelFiles)
        combo.addItem (f.getFileNameWithoutExtension(), id++);

    const auto loaded = processorRef.getNam().getLoadedName();
    for (int i = 0; i < modelFiles.size(); ++i)
        if (modelFiles[i].getFileNameWithoutExtension() == loaded)
            combo.setSelectedId (i + 1, juce::dontSendNotification);

    combo.onChange = [this]
    {
        const int idx = ampPanel->getCombo().getSelectedId() - 1;
        if (idx >= 0 && idx < modelFiles.size())
        {
            processorRef.loadNamModel (modelFiles[idx]);   // thread-safe swap
            updateStatusLabel();
        }
    };
}

void GuitarView::importAmpModel (const juce::File& source)
{
    if (ampPanel == nullptr || ! source.existsAsFile()) return;

    // Copy the chosen .nam into the writable models folder (created on demand).
    auto dir = livedsp::getModelsDir();
    dir.createDirectory();
    auto dest = dir.getChildFile (source.getFileName());
    if (source != dest)
        source.copyFileTo (dest);

    // Rescan, then select the imported model — selecting fires the combo's
    // onChange, which loads it and refreshes the status label.
    populateAmpModels();
    for (int i = 0; i < modelFiles.size(); ++i)
        if (modelFiles[i] == dest)
        {
            ampPanel->getCombo().setSelectedId (i + 1);   // notifies -> loadNamModel
            break;
        }
}

void GuitarView::populateCabIrs()
{
    if (cabPanel == nullptr) return;
    auto& combo = cabPanel->getCombo();

    if (auto dir = livedsp::getModelsDir(); dir.isDirectory())
        irFiles = dir.findChildFiles (juce::File::findFiles, true, "*.wav");

    int id = 1;
    for (const auto& f : irFiles)
        combo.addItem (f.getFileNameWithoutExtension(), id++);

    const auto loaded = processorRef.getCab().getLoadedName();
    for (int i = 0; i < irFiles.size(); ++i)
        if (irFiles[i].getFileNameWithoutExtension() == loaded)
            combo.setSelectedId (i + 1, juce::dontSendNotification);

    combo.onChange = [this]
    {
        const int idx = cabPanel->getCombo().getSelectedId() - 1;
        if (idx >= 0 && idx < irFiles.size())
        {
            processorRef.loadCabIr (irFiles[idx]);
            updateStatusLabel();
        }
    };
}

void GuitarView::populatePresetSelector()
{
    if (auto dir = livedsp::getFavsDir(); dir.isDirectory())
        presetFiles = dir.findChildFiles (juce::File::findFiles, false, "*");

    int id = 1;
    for (const auto& f : presetFiles)
        presetSelector.addItem (f.getFileName(), id++);
}

void GuitarView::updateStatusLabel()
{
    const auto namName = processorRef.getNam().isLoaded()
                            ? processorRef.getNam().getLoadedName()
                            : juce::String ("no model [") + processorRef.getNam().getLastStatus() + "]";
    const auto irName  = processorRef.getCab().isLoaded()
                            ? processorRef.getCab().getLoadedName()
                            : juce::String ("no IR");

    statusLabel.setText ("Amp: " + namName + "    |    Cab IR: " + irName,
                         juce::dontSendNotification);

    // The "download a rig" hint is only useful before the first model is loaded.
    if (ampPanel != nullptr)
        ampPanel->setDownloadLinkVisible (! processorRef.getNam().isLoaded());
}

void GuitarView::timerCallback()
{
    const double sr = processorRef.getSampleRate();
    if (sr > 0.0)
    {
        // Estimated input->output latency: the latency of the ACTIVE modules
        // (dynamic, follows the pitch switch) + the in/out buffering (~2 blocks).
        const double samples = processorRef.getEffectiveLatencySamples()
                               + 2.0 * processorRef.getCurrentBlockSize();
        latencyLabel.setText ("Latency ~ " + juce::String (samples / sr * 1000.0, 1) + " ms",
                              juce::dontSendNotification);
    }

    // Update the gate LED (lights up when the gate is ducking).
    if (gatePanel != nullptr)
        gatePanel->setLedLevel (1.0f - processorRef.getGate().getCurrentGain());
}

void GuitarView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (LiveLookAndFeel::cBackground));
    g.setColour (juce::Colour (LiveLookAndFeel::cPanelHead));
    g.fillRect (0, 46, getWidth(), 1);
}

void GuitarView::layoutRow (juce::Rectangle<int> area, juce::OwnedArray<PanelBase>& panels)
{
    if (panels.isEmpty())
        return;

    // The panels proportionally FILL the entire width of the row.
    const int gap = 8;
    int totalPref = 0;
    for (auto* pnl : panels)
        totalPref += pnl->getPreferredWidth();

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

void GuitarView::resized()
{
    auto area = getLocalBounds().reduced (12);

    // Top bar: MENU + title on the left; preset + TUNER on the right (the model
    // selector has moved into the AMP panel, so the bar is less crowded).
    auto top = area.removeFromTop (32);
    menuButton.setBounds (top.removeFromLeft (72).reduced (0, 2));
    top.removeFromLeft (10);
    titleLabel.setBounds (top.removeFromLeft (120));
    tunerButton.setBounds (top.removeFromRight (84).reduced (0, 2));
    top.removeFromRight (8);
    presetSelector.setBounds (top.removeFromRight (170).reduced (0, 2));

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
