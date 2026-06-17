#pragma once

#include <JuceHeader.h>
#include <cmath>
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
        guitar.title = juce::String::fromUTF8 ("GITÁR");
        guitar.subtitle = juce::String::fromUTF8 ("amp-szimulátor");
        guitar.onClick = [this] { if (onChoose) onChoose (1); };
        addAndMakeVisible (guitar);

        vocal.kind = ChoiceCard::Kind::vocal;
        vocal.title = juce::String::fromUTF8 ("ÉNEK");
        vocal.subtitle = juce::String::fromUTF8 ("mikrofon csatorna");
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
            const auto bg  = juce::Colour (GuitarLookAndFeel::cBackground);
            const auto c   = box.getCentre();

            // Átlós elrendezés (mint a fotókon): a tartalmat a középpont körül
            // megdöntjük, majd "függőlegesen" rajzolunk.
            juce::Graphics::ScopedSaveState save (g);

            if (kind == Kind::guitar)
            {
                g.addTransform (juce::AffineTransform::rotation (-0.42f, c.x, c.y));

                const float w  = box.getWidth();
                const float h  = box.getHeight();
                const float bw = w * 0.50f;                  // test szélesség
                const float bh = h * 0.40f;                  // test magasság
                const float bx = c.x - bw * 0.5f;
                const float by = box.getBottom() - bh - h * 0.04f;

                // Test: két "szarvval" rendelkező lekerekített forma (superstrat).
                juce::Path body;
                body.addRoundedRectangle (bx, by, bw, bh, bw * 0.42f, bh * 0.5f);
                body.addEllipse (bx - bw * 0.06f, by - bh * 0.18f, bw * 0.42f, bh * 0.5f);   // felső szarv
                body.addEllipse (bx + bw * 0.64f, by - bh * 0.18f, bw * 0.42f, bh * 0.5f);
                g.setColour (accent);
                g.fillPath (body);

                // Pickupok (sötét sávok).
                g.setColour (bg);
                g.fillRoundedRectangle (c.x - bw * 0.22f, by + bh * 0.26f, bw * 0.44f, bh * 0.13f, 1.5f);
                g.fillRoundedRectangle (c.x - bw * 0.22f, by + bh * 0.50f, bw * 0.44f, bh * 0.13f, 1.5f);

                // Nyak + fogólap a test tetejétől felfelé.
                const float neckW = w * 0.12f;
                const float neckTop = box.getY() + h * 0.06f;
                g.setColour (dim);
                g.fillRoundedRectangle (c.x - neckW * 0.5f, neckTop, neckW, by - neckTop + bh * 0.2f, 2.0f);

                // Fej + 3 hangolókulcs.
                g.setColour (accent);
                g.fillRoundedRectangle (c.x - neckW * 0.8f, box.getY(), neckW * 1.6f, h * 0.12f, 2.0f);
                g.setColour (bg);
                for (int i = 0; i < 3; ++i)
                    g.fillEllipse (c.x - neckW * 0.5f + i * neckW * 0.5f, box.getY() + h * 0.03f, 3.0f, 3.0f);
            }
            else
            {
                g.addTransform (juce::AffineTransform::rotation (0.42f, c.x, c.y));

                const float w   = box.getWidth();
                const float h   = box.getHeight();
                const float ball = w * 0.52f;                // gömbrács átmérő
                const float bx  = c.x - ball * 0.5f;
                const float by  = box.getY() + h * 0.02f;

                // Gömbrács (SM58 fej).
                g.setColour (accent);
                g.fillEllipse (bx, by, ball, ball);
                // Rács: ívelt vonalak.
                g.setColour (bg.withAlpha (0.9f));
                for (float t = 0.28f; t < 0.95f; t += 0.18f)
                {
                    const float yy = by + ball * t;
                    const float dx = ball * 0.5f * std::sqrt (juce::jmax (0.0f, 1.0f - (2.0f * t - 1.0f) * (2.0f * t - 1.0f)));
                    g.drawLine (c.x - dx, yy, c.x + dx, yy, 1.3f);
                }

                // Gyűrű a fej és a nyél között.
                const float bodyW = ball * 0.66f;
                g.setColour (dim);
                g.fillRoundedRectangle (c.x - bodyW * 0.5f, by + ball * 0.86f, bodyW, h * 0.06f, 2.0f);

                // Nyél (kissé szűkülő test).
                g.setColour (accent.darker (0.2f));
                juce::Path bodyP;
                const float top = by + ball * 0.92f;
                bodyP.startNewSubPath (c.x - bodyW * 0.5f, top);
                bodyP.lineTo (c.x + bodyW * 0.5f, top);
                bodyP.lineTo (c.x + bodyW * 0.40f, box.getBottom());
                bodyP.lineTo (c.x - bodyW * 0.40f, box.getBottom());
                bodyP.closeSubPath();
                g.fillPath (bodyP);
            }
        }
    };

    ChoiceCard guitar, vocal;
};
