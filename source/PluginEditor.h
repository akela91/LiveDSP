#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/LiveLookAndFeel.h"
#include "ui/AppView.h"

/**
    Vékony shell-szerkesztő: a közös LookAndFeel-t birtokolja, és az app-mód
    szerint EGY AppView-t mutat (Landing / Guitar / Vocal). A nézet a teljes
    ablakot kitölti; módváltáskor az ablak átméreteződik a nézet igénye szerint.
*/
class LiveDspEditor  : public juce::AudioProcessorEditor
{
public:
    explicit LiveDspEditor (LiveDspProcessor& p);
    ~LiveDspEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void showMode (int mode);

    LiveDspProcessor&        processorRef;
    LiveLookAndFeel          lnf;
    std::unique_ptr<AppView>   view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveDspEditor)
};
