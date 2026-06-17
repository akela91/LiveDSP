#include "PluginEditor.h"
#include "ui/LandingView.h"
#include "ui/GuitarView.h"
#include "ui/VoiceView.h"

LiveDspEditor::LiveDspEditor (LiveDspProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processorRef (p)
{
    setLookAndFeel (&lnf);
    showMode (processorRef.getAppMode());
}

LiveDspEditor::~LiveDspEditor()
{
    view.reset();
    setLookAndFeel (nullptr);
}

void LiveDspEditor::showMode (int mode)
{
    using AppMode = LiveDspProcessor::AppMode;

    // The enclosing (standalone) window's "chrome" = window − editor (title bar +
    // options bar). Constant across views; we measure it while still in the old,
    // consistent state so that on a mode change we can force the FIXED size onto the
    // window (otherwise, after a previously resized guitar window, the landing/vocal
    // layout is buggy).
    int chromeW = 0, chromeH = 0;
    if (auto* top = getTopLevelComponent(); top != nullptr && top != this)
    {
        chromeW = top->getWidth()  - getWidth();
        chromeH = top->getHeight() - getHeight();
    }

    view.reset();

    if (mode == (int) AppMode::guitar)
    {
        view = std::make_unique<GuitarView> (processorRef);
    }
    else if (mode == (int) AppMode::vocal)
    {
        view = std::make_unique<VoiceView> (processorRef);
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

    // Shared callbacks for every view.
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
    else
        // IMPORTANT: for a non-resizable view use a FIXED limit, otherwise a previous
        // guitar mode's (min 900x440) limit would clamp the smaller target size, the
        // window would not resize, and the view would stay EMPTY (see bug).
        setResizeLimits (view->defaultWidth(), view->defaultHeight(),
                         view->defaultWidth(), view->defaultHeight());

    if (auto* top = getTopLevelComponent(); top != nullptr && top != this)
        top->setSize (view->defaultWidth() + chromeW, view->defaultHeight() + chromeH);
    else
        setSize (view->defaultWidth(), view->defaultHeight());

    // ALWAYS lay out the current view — even if the window size did not change
    // (otherwise the freshly created view's panels would appear with 0 size, i.e.
    // empty).
    resized();
}

void LiveDspEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (LiveLookAndFeel::cBackground));
}

void LiveDspEditor::resized()
{
    if (view != nullptr)
        view->setBounds (getLocalBounds());
}
