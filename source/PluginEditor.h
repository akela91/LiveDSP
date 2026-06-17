#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/LiveLookAndFeel.h"
#include "ui/AppView.h"

/**
    Thin shell editor: owns the shared LookAndFeel and shows exactly ONE AppView
    according to the app mode (Landing / Guitar / Vocal). The view fills the whole
    window; on a mode change the window resizes to the view's requested size.
*/
class LiveDspEditor  : public juce::AudioProcessorEditor
{
public:
    explicit LiveDspEditor (LiveDspProcessor& p);
    ~LiveDspEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void parentHierarchyChanged() override;

private:
    void showMode (int mode);
    // Hide the standalone window's "Options" (audio settings) button on the
    // landing screen; show it once a mode (guitar/vocal) is chosen.
    void updateOptionsButtonVisibility();

    LiveDspProcessor&        processorRef;
    LiveLookAndFeel          lnf;
    std::unique_ptr<AppView>   view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveDspEditor)
};
