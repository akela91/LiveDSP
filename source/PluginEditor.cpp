#include "PluginEditor.h"
#include "ui/LandingView.h"
#include "ui/GuitarView.h"
#include "ui/VocalView.h"

GuitarDspEditor::GuitarDspEditor (GuitarDspProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p)
{
    setLookAndFeel (&lnf);
    showMode (processorRef.getAppMode());
}

GuitarDspEditor::~GuitarDspEditor()
{
    view.reset();
    setLookAndFeel (nullptr);
}

void GuitarDspEditor::showMode (int mode)
{
    using AppMode = GuitarDspProcessor::AppMode;

    view.reset();

    if (mode == (int) AppMode::guitar)
    {
        view = std::make_unique<GuitarView> (processorRef);
    }
    else if (mode == (int) AppMode::vocal)
    {
        view = std::make_unique<VocalView> (processorRef);
    }
    else
    {
        auto landing = std::make_unique<LandingView>();
        landing->onChoose = [this] (int chosen)
        {
            processorRef.setAppMode (chosen);
            showMode (chosen);
        };
        view = std::move (landing);
    }

    // Közös callbackek minden nézetre.
    view->onBackToMenu = [this]
    {
        processorRef.setAppMode ((int) AppMode::none);
        showMode ((int) AppMode::none);
    };
    view->onRequestSize = [this] (int w, int h) { setSize (w, h); };

    addAndMakeVisible (*view);

    const bool resiz = view->wantsResizable();
    setResizable (resiz, resiz);
    if (resiz)
        setResizeLimits (900, 440, 1500, 1000);

    setSize (view->defaultWidth(), view->defaultHeight());
    resized();
}

void GuitarDspEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (GuitarLookAndFeel::cBackground));
}

void GuitarDspEditor::resized()
{
    if (view != nullptr)
        view->setBounds (getLocalBounds());
}
