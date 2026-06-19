#pragma once

#include <JuceHeader.h>
#include <BinaryData.h>
#include "LiveLookAndFeel.h"
#include "AppView.h"

/**
    Landing screen: two large "card" buttons — GUITAR (amp simulator) or
    VOCALS (microphone channel). The choice arrives via the onChoose callback
    (1 = guitar, 2 = vocals).
*/
class LandingView : public AppView
{
public:
    // 1 = Guitar, 2 = Vocals.
    std::function<void (int)> onChoose;

    LandingView()
    {
        guitar.kind = ChoiceCard::Kind::guitar;
        guitar.title = "GuitarDSP";
        guitar.subtitle = juce::String::fromUTF8 ("guitar — amp simulator");
        guitar.onClick = [this] { if (onChoose) onChoose (1); };
        addAndMakeVisible (guitar);

        vocal.kind = ChoiceCard::Kind::vocal;
        vocal.title = "VoiceDSP";
        vocal.subtitle = juce::String::fromUTF8 ("vocals — microphone channel");
        vocal.onClick = [this] { if (onChoose) onChoose (2); };
        addAndMakeVisible (vocal);
    }

    int  defaultWidth()  const override { return 640; }
    int  defaultHeight() const override { return 400; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (LiveLookAndFeel::cBackground));

        g.setColour (juce::Colour (LiveLookAndFeel::cAccent));
        g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
        g.drawText ("LiveDSP", getLocalBounds().removeFromTop (78).withTrimmedTop (24),
                    juce::Justification::centred);

        g.setColour (juce::Colour (LiveLookAndFeel::cTextDim));
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText (juce::String::fromUTF8 ("Choose a mode"),
                    getLocalBounds().removeFromTop (108).withTrimmedTop (62),
                    juce::Justification::centred);
    }

    void resized() override
    {
        auto area = getLocalBounds().withTrimmedTop (118).reduced (40, 30);
        const int gap = 24;
        const int w = (area.getWidth() - gap) / 2;
        guitar.setBounds (area.removeFromLeft (w));
        area.removeFromLeft (gap);
        vocal.setBounds (area);
    }

private:
    //==========================================================================
    /** Large icon choice card (with hover highlight). */
    class ChoiceCard : public juce::Button
    {
    public:
        enum class Kind { guitar, vocal };
        Kind kind { Kind::guitar };
        juce::String title, subtitle;

        ChoiceCard() : juce::Button ({}) {}

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.0f);
            g.setColour (juce::Colour (LiveLookAndFeel::cPanel));
            g.fillRoundedRectangle (b, 12.0f);

            const auto accent = juce::Colour (LiveLookAndFeel::cAccent);
            g.setColour ((highlighted || down) ? accent
                                               : juce::Colour (LiveLookAndFeel::cTrack));
            g.drawRoundedRectangle (b.reduced (1.0f), 12.0f, (highlighted || down) ? 2.4f : 1.4f);

            auto iconBox = b.withTrimmedBottom (b.getHeight() * 0.40f).reduced (b.getWidth() * 0.22f, 16.0f);
            if (iconImage.isNull())
            {
                iconImage = (kind == Kind::guitar)
                    ? juce::ImageCache::getFromMemory (BinaryData::GuitarDSP_png, BinaryData::GuitarDSP_pngSize)
                    : juce::ImageCache::getFromMemory (BinaryData::VoiceDSP_png,  BinaryData::VoiceDSP_pngSize);
            }
            if (iconImage.isValid())
                g.drawImageWithin (iconImage, iconBox.getX(), iconBox.getY(),
                                   iconBox.getWidth(), iconBox.getHeight(),
                                   juce::RectanglePlacement::centred, false);
            juce::ignoreUnused (accent);

            auto textArea = b.withTop (b.getCentreY() + 18.0f).toNearestInt();
            g.setColour (juce::Colour (LiveLookAndFeel::cText));
            g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
            g.drawText (title, textArea.removeFromTop (30), juce::Justification::centred);
            g.setColour (juce::Colour (LiveLookAndFeel::cTextDim));
            g.setFont (juce::Font (juce::FontOptions (12.5f)));
            g.drawText (subtitle, textArea.removeFromTop (22), juce::Justification::centred);
        }

    private:
        juce::Image iconImage;
    };

    ChoiceCard guitar, vocal;
};
