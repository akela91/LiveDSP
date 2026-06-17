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

    // EGYETLEN méretezés, hogy ne villanjon: ha van top-level ablak, csak azt
    // méretezzük (a szerkesztő mint tartalom követi) — nem hívunk külön
    // editor.setSize-t is, mert a kettő két layout-ot/átméretezést okozna.
    if (auto* top = getTopLevelComponent(); top != nullptr && top != this)
    {
        const int w = view->defaultWidth()  + chromeW;
        const int h = view->defaultHeight() + chromeH;
        if (top->getWidth() != w || top->getHeight() != h)
            top->setSize (w, h);   // ez átméretezi a tartalmat -> a nézet kitölti
        else
            resized();             // a méret már jó, csak az új nézetet rendezzük
    }
    else
    {
        setSize (view->defaultWidth(), view->defaultHeight());
    }
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
