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

    // A befoglaló (standalone) ablak "kerete" = ablak − szerkesztő (címsor + opciók
    // sáv). Konstans a nézetek között; még a régi, konzisztens állapotból mérjük,
    // hogy módváltáskor a FIX méretet rá tudjuk kényszeríteni az ablakra (különben
    // egy korábban átméretezett gitár-ablak után a landing/vocal kinézet bugos).
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
    else
        // FONTOS: a nem-átméretezhető nézetnél FIX korlát, különben egy korábbi
        // gitár-mód (min 900x440) korlátja clamp-elné a kisebb cél-méretet, az
        // ablak nem méreteződne át, és a nézet ÜRESEN maradna (lásd bug).
        setResizeLimits (view->defaultWidth(), view->defaultHeight(),
                         view->defaultWidth(), view->defaultHeight());

    if (auto* top = getTopLevelComponent(); top != nullptr && top != this)
        top->setSize (view->defaultWidth() + chromeW, view->defaultHeight() + chromeH);
    else
        setSize (view->defaultWidth(), view->defaultHeight());

    // MINDIG elrendezzük az aktuális nézetet — akkor is, ha az ablakméret nem
    // változott (különben a frissen létrehozott nézet panelei 0 mérettel,
    // azaz üresen jelennének meg).
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
