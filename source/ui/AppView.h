#pragma once

#include <JuceHeader.h>

/**
    Common base for the app views (Landing / Guitar / Vocal). The shell editor
    (LiveDspEditor) shows a single AppView at a time, according to the selected
    app mode. The view fills the entire window.
*/
struct AppView : juce::Component
{
    // Return to the Landing screen. Wired up by the shell.
    std::function<void()> onBackToMenu;

    // Window-size request to the shell (e.g. when the guitar tuner opens/closes).
    std::function<void (int, int)> onRequestSize;

    // The view's default size and resizability.
    virtual int  defaultWidth()   const = 0;
    virtual int  defaultHeight()  const = 0;
    virtual bool wantsResizable() const { return false; }
};
