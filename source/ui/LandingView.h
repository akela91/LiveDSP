#pragma once

#include <JuceHeader.h>
#include <cstring>
#include "GuitarLookAndFeel.h"
#include "AppView.h"

/**
    Induló képernyő: két nagy "kártya" gomb — GITÁR (amp-szimulátor) vagy
    ÉNEK (mikrofon csatorna). A választás az onChoose callbacken keresztül
    érkezik (1 = gitár, 2 = ének).
*/
class LandingView : public AppView
{
public:
    // 1 = Gitár, 2 = Ének.
    std::function<void (int)> onChoose;

    LandingView()
    {
        guitar.kind = ChoiceCard::Kind::guitar;
        guitar.title = "guitarDSP";
        guitar.subtitle = juce::String::fromUTF8 ("gitár — amp-szimulátor");
        guitar.onClick = [this] { if (onChoose) onChoose (1); };
        addAndMakeVisible (guitar);

        vocal.kind = ChoiceCard::Kind::vocal;
        vocal.title = "VoiceDSP";
        vocal.subtitle = juce::String::fromUTF8 ("ének — mikrofon csatorna");
        vocal.onClick = [this] { if (onChoose) onChoose (2); };
        addAndMakeVisible (vocal);
    }

    int  defaultWidth()  const override { return 640; }
    int  defaultHeight() const override { return 400; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (GuitarLookAndFeel::cBackground));

        g.setColour (juce::Colour (GuitarLookAndFeel::cAccent));
        g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
        g.drawText ("LiveDSP", getLocalBounds().removeFromTop (78).withTrimmedTop (24),
                    juce::Justification::centred);

        g.setColour (juce::Colour (GuitarLookAndFeel::cTextDim));
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText (juce::String::fromUTF8 ("Válassz módot"),
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
    /** Nagy, ikonos választókártya (hover-kiemeléssel). */
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
            g.setColour (juce::Colour (GuitarLookAndFeel::cPanel));
            g.fillRoundedRectangle (b, 12.0f);

            const auto accent = juce::Colour (GuitarLookAndFeel::cAccent);
            g.setColour ((highlighted || down) ? accent
                                               : juce::Colour (GuitarLookAndFeel::cTrack));
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
            g.setColour (juce::Colour (GuitarLookAndFeel::cText));
            g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
            g.drawText (title, textArea.removeFromTop (30), juce::Justification::centred);
            g.setColour (juce::Colour (GuitarLookAndFeel::cTextDim));
            g.setFont (juce::Font (juce::FontOptions (12.5f)));
            g.drawText (subtitle, textArea.removeFromTop (22), juce::Justification::centred);
        }

    private:
        std::unique_ptr<juce::Drawable> iconDrawable;

        // Lucide ikonok (ISC licenc), jégkék (#4AA8E0) stroke-kal beágyazva.
        static constexpr const char* kGuitarSvg =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" "
            "stroke=\"#4AA8E0\" stroke-width=\"1.6\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
            "<path d=\"m11.9 12.1 4.514-4.514\"/>"
            "<path d=\"M20.1 2.3a1 1 0 0 0-1.4 0l-1.114 1.114A2 2 0 0 0 17 4.828v1.344a2 2 0 0 1-.586 1.414A2 2 0 0 1 17.828 7h1.344a2 2 0 0 0 1.414-.586L21.7 5.3a1 1 0 0 0 0-1.4z\"/>"
            "<path d=\"m6 16 2 2\"/>"
            "<path d=\"M8.23 9.85A3 3 0 0 1 11 8a5 5 0 0 1 5 5 3 3 0 0 1-1.85 2.77l-.92.38A2 2 0 0 0 12 18a4 4 0 0 1-4 4 6 6 0 0 1-6-6 4 4 0 0 1 4-4 2 2 0 0 0 1.85-1.23z\"/>"
            "</svg>";
        static constexpr const char* kMicSvg =
            "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" "
            "stroke=\"#4AA8E0\" stroke-width=\"1.6\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
            "<path d=\"M12 19v3\"/>"
            "<path d=\"M19 10v2a7 7 0 0 1-14 0v-2\"/>"
            "<rect x=\"9\" y=\"2\" width=\"6\" height=\"13\" rx=\"3\"/>"
            "</svg>";
    };

    ChoiceCard guitar, vocal;
};
