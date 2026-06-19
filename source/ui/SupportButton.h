#pragma once

#include <JuceHeader.h>
#include "LiveLookAndFeel.h"

/**
    Small, discreet "Buy me a pizza" support button — opens the Buy Me a Coffee
    page (buymeacoffee.com/akela91) in the default browser. Styled to the generated
    Buy Me a Coffee button: app-accent background (#4AA8E0), black text, 🍕 emoji.

    Used on the landing screen and both module views (guitar / vocals).
*/
class CoffeeButton : public juce::TextButton
{
public:
    CoffeeButton()
    {
        // "🍕 Buy me a pizza" — the pizza is written as raw UTF-8 bytes so it is
        // editor-encoding independent (F0 9F 8D 95 = U+1F355 SLICE OF PIZZA).
        setButtonText (juce::String::fromUTF8 ("\xF0\x9F\x8D\x95  Buy me a pizza"));
        setColour (juce::TextButton::buttonColourId,  juce::Colour (LiveLookAndFeel::cAccent));
        setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        setColour (juce::TextButton::textColourOnId,  juce::Colours::black);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip ("Support LiveDSP \xE2\x80\x94 buymeacoffee.com/akela91");
        onClick = [] { juce::URL ("https://buymeacoffee.com/akela91").launchInDefaultBrowser(); };
    }
};
