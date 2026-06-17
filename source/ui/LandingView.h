#pragma once

#include <JuceHeader.h>
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
        guitar.title = "GITÁR";
        guitar.subtitle = "amp-szimulátor";
        guitar.onClick = [this] { if (onChoose) onChoose (1); };
        addAndMakeVisible (guitar);

        vocal.kind = ChoiceCard::Kind::vocal;
        vocal.title = "ÉNEK";
        vocal.subtitle = "mikrofon csatorna";
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
        g.drawText ("guitarDSP", getLocalBounds().removeFromTop (78).withTrimmedTop (24),
                    juce::Justification::centred);

        g.setColour (juce::Colour (GuitarLookAndFeel::cTextDim));
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawText ("Válassz módot", getLocalBounds().removeFromTop (108).withTrimmedTop (62),
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

            auto icon = b.withTrimmedBottom (b.getHeight() * 0.42f).reduced (b.getWidth() * 0.28f);
            drawIcon (g, icon, accent);

            auto textArea = b.withTop (b.getCentreY() + 18.0f).toNearestInt();
            g.setColour (juce::Colour (GuitarLookAndFeel::cText));
            g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
            g.drawText (title, textArea.removeFromTop (30), juce::Justification::centred);
            g.setColour (juce::Colour (GuitarLookAndFeel::cTextDim));
            g.setFont (juce::Font (juce::FontOptions (12.5f)));
            g.drawText (subtitle, textArea.removeFromTop (22), juce::Justification::centred);
        }

    private:
        void drawIcon (juce::Graphics& g, juce::Rectangle<float> box, juce::Colour accent)
        {
            const auto dim = juce::Colour (GuitarLookAndFeel::cTextDim);
            const auto c = box.getCentre();

            if (kind == Kind::guitar)
            {
                // amp-fej sziluett: keret + grille-vonalak.
                g.setColour (dim);
                g.drawRoundedRectangle (box, 8.0f, 2.0f);
                auto grille = box.reduced (12.0f).withTrimmedTop (10.0f);
                g.setColour (accent.withAlpha (0.85f));
                for (float gx = grille.getX() + 3.0f; gx < grille.getRight(); gx += 7.0f)
                    g.drawLine (gx, grille.getY(), gx, grille.getBottom(), 1.4f);
                g.setColour (accent);
                g.fillEllipse (box.getX() + 12.0f, box.getY() + 6.0f, 6.0f, 6.0f);
                g.fillEllipse (box.getX() + 26.0f, box.getY() + 6.0f, 6.0f, 6.0f);
            }
            else
            {
                // mikrofon: kapszula + nyél + talp.
                const float w = box.getWidth() * 0.42f;
                const float h = box.getHeight() * 0.62f;
                juce::Rectangle<float> capsule (c.x - w * 0.5f, box.getY(), w, h);
                g.setColour (accent);
                g.fillRoundedRectangle (capsule, w * 0.5f);
                g.setColour (juce::Colour (GuitarLookAndFeel::cBackground));
                for (float gy = capsule.getY() + 8.0f; gy < capsule.getBottom() - 6.0f; gy += 6.0f)
                    g.drawLine (capsule.getX() + 4.0f, gy, capsule.getRight() - 4.0f, gy, 1.4f);
                g.setColour (dim);
                g.drawLine (c.x, capsule.getBottom(), c.x, box.getBottom() - 6.0f, 2.4f);
                g.drawLine (c.x - 12.0f, box.getBottom() - 6.0f, c.x + 12.0f, box.getBottom() - 6.0f, 2.4f);
            }
        }
    };

    ChoiceCard guitar, vocal;
};
