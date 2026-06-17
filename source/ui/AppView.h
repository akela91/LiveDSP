#pragma once

#include <JuceHeader.h>

/**
    Közös bázis az app-nézetekhez (Landing / Guitar / Vocal). A shell-szerkesztő
    (GuitarDspEditor) egyszerre egy AppView-t mutat, a kiválasztott app-mód
    szerint. A nézet a teljes ablakot kitölti.
*/
struct AppView : juce::Component
{
    // Vissza az induló (Landing) képernyőre. A shell köti be.
    std::function<void()> onBackToMenu;

    // Ablakméret-kérés a shelltől (pl. a gitár tuner ki/be nyitásakor).
    std::function<void (int, int)> onRequestSize;

    // A nézet alapértelmezett mérete és átméretezhetősége.
    virtual int  defaultWidth()   const = 0;
    virtual int  defaultHeight()  const = 0;
    virtual bool wantsResizable() const { return false; }
};
