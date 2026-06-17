#pragma once

#include <JuceHeader.h>
#include <cstring>
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

            auto iconBox = b.withTrimmedBottom (b.getHeight() * 0.42f).reduced (b.getWidth() * 0.30f, 16.0f);
            if (iconDrawable == nullptr)
            {
                const char* svg = (kind == Kind::guitar) ? kGuitarSvg : kMicSvg;
                iconDrawable = juce::Drawable::createFromImageData (svg, std::strlen (svg));
            }
            if (iconDrawable != nullptr)
                iconDrawable->drawWithin (g, iconBox, juce::RectanglePlacement::centred, 1.0f);
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
        std::unique_ptr<juce::Drawable> iconDrawable;

        // Filled silhouette icons, embedded with an ice-blue (#4AA8E0) fill.
        // Guitar: game-icons.net (lorc, CC BY 3.0) — electric guitar silhouette.
        static constexpr const char* kGuitarSvg =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 512 512\">"
            "<path fill=\"#4AA8E0\" d=\"M491.938 18.813l-17.72 2.375-89.374 11.968-6.22.844-1.562 6.094-18.5 72.156-136.187 137.28c-2.094-4.4-4.324-8.708-6.875-12.843-7.317-11.86-18.338-22.357-34.844-25.687-6.457-1.303-12.664-1.702-18.53-1.28-17.602 1.26-32.182 9.775-41.69 22.5-10.95 14.654-15.87 34.054-15.31 54.405-36.16 4.516-66.336 31.382-80.657 64.313-15.608 35.885-11.856 80.956 24.655 111.156 43.28 35.8 88.28 31.622 119.875 11.22 28.593-18.467 47.778-48.14 50.813-74.752 18.615-2.81 38.424-9.03 56.375-17.968 20.474-10.195 38.536-23.433 48.406-40.063l7.625-12.874-14.908-1.22c-34.56-2.818-53.76-12.87-66.406-26.217l146-147.22 18.938 1.375 6.156.438 2.813-5.5 6.125-11.907 25.03 11.906L464 132.438l-24.53-11.656 7.655-14.874 25.844 12.28 8.03-16.874-25.313-12.03L464 73.155 491.03 86l8.033-16.875L472.53 56.53l11.22-21.81 8.188-15.907zm-124.532 111l13.22 13.093-200.22 201.875c-1.556-1.983-3.227-3.898-5.062-5.717-2.65-2.628-5.493-4.96-8.47-7l200.532-202.25zm-235.47 210.093c10.914-.046 21.837 4.094 30.25 12.438 16.834 16.69 16.938 43.576.25 60.406-16.685 16.83-43.573 16.94-60.405.25-16.83-16.69-16.936-43.576-.25-60.406 8.345-8.415 19.245-12.64 30.157-12.688z\"/>"
            "</svg>";
        // Microphone: Phosphor Icons (MIT) — microphone-stage, handheld (stage) mic.
        static constexpr const char* kMicSvg =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 256 256\">"
            "<path fill=\"#4AA8E0\" d=\"M168,16A72.07,72.07,0,0,0,96,88a73.29,73.29,0,0,0,.63,9.42L27.12,192.22A15.93,15.93,0,0,0,28.71,213L43,227.29a15.93,15.93,0,0,0,20.78,1.59l94.81-69.53A73.29,73.29,0,0,0,168,160a72,72,0,1,0,0-144Zm56,72a55.72,55.72,0,0,1-11.16,33.52L134.49,43.16A56,56,0,0,1,224,88ZM54.32,216,40,201.68,102.14,117A72.37,72.37,0,0,0,139,153.86ZM112,88a55.67,55.67,0,0,1,11.16-33.51l78.34,78.34A56,56,0,0,1,112,88Zm-2.35,58.34a8,8,0,0,1,0,11.31l-8,8a8,8,0,1,1-11.31-11.31l8-8A8,8,0,0,1,109.67,146.33Z\"/>"
            "</svg>";
    };

    ChoiceCard guitar, vocal;
};
