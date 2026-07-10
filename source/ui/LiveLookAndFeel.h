#pragma once

#include <JuceHeader.h>

/**
    Modern, dark LookAndFeel for LiveDSP.
    Ice-blue accent, dark panels, rotary knobs with curved value indicators.

    Design language (shared by every widget):
      - 5-8 px rounded corners, hairline strokes for depth (white @ ~5 %)
      - value boxes are borderless rounded "chips" on the panel-head colour
      - accent is used sparingly: value arcs/fills, toggled state, focus marks
      - Segoe UI typography on Windows (falls back to the platform default)
*/
class LiveLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Colour palette
    static constexpr juce::uint32 cBackground = 0xff0d0d10;
    static constexpr juce::uint32 cPanel      = 0xff17171c;
    static constexpr juce::uint32 cPanelHead  = 0xff20202a;
    static constexpr juce::uint32 cAccent     = 0xff4aa8e0;  // icy steel blue
    static constexpr juce::uint32 cAccentDim  = 0xff2c4a5e;
    static constexpr juce::uint32 cText        = 0xffe9e9ec;
    static constexpr juce::uint32 cTextDim     = 0xff8a8a93;
    static constexpr juce::uint32 cTrack       = 0xff34343e;

    LiveLookAndFeel()
    {
       #if JUCE_WINDOWS
        setDefaultSansSerifTypefaceName ("Segoe UI");
       #endif

        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (cBackground));

        setColour (juce::Label::textColourId,                juce::Colour (cText));

        // Slider value boxes render as borderless rounded chips (see drawLabel).
        setColour (juce::Slider::textBoxTextColourId,       juce::Colour (cText));
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (cPanelHead));
        setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId,  juce::Colour (cAccentDim));

        setColour (juce::TextEditor::highlightColourId,      juce::Colour (cAccentDim));
        setColour (juce::TextEditor::highlightedTextColourId, juce::Colour (cText));
        setColour (juce::CaretComponent::caretColourId,       juce::Colour (cAccent));

        setColour (juce::TextButton::buttonColourId,   juce::Colour (cPanelHead));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (cAccent));
        setColour (juce::TextButton::textColourOffId,  juce::Colour (cText));
        setColour (juce::TextButton::textColourOnId,   juce::Colours::black);

        setColour (juce::ComboBox::backgroundColourId,       juce::Colour (cPanelHead));
        setColour (juce::ComboBox::textColourId,             juce::Colour (cText));
        setColour (juce::ComboBox::outlineColourId,          juce::Colour (cTrack));
        setColour (juce::ComboBox::arrowColourId,            juce::Colour (cAccent));

        setColour (juce::PopupMenu::backgroundColourId,      juce::Colour (cPanel));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (cAccentDim));
        setColour (juce::PopupMenu::textColourId,            juce::Colour (cText));
    }

    //==============================================================================
    // Shared panel frame: rounded body with a hairline stroke for depth. Used by
    // every module panel plus the tuner/metronome strips, so they all match.
    static void drawPanelFrame (juce::Graphics& g, juce::Rectangle<float> b)
    {
        g.setColour (juce::Colour (cPanel));
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    }

    // Shared panel header strip (rounded on top, square below, shadow line under).
    static void drawPanelHeader (juce::Graphics& g, juce::Rectangle<float> header)
    {
        g.setColour (juce::Colour (cPanelHead));
        g.fillRoundedRectangle (header, 8.0f);
        g.fillRect (header.withTop (header.getCentreY()));
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRect (header.removeFromBottom (1.0f));
    }

    //==============================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto  centre = bounds.getCentre();
        const float lineW  = radius * 0.16f;
        const float arcR   = radius - lineW;
        const float angle  = startAngle + sliderPos * (endAngle - startAngle);

        // background arc
        juce::Path bg;
        bg.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (cTrack));
        g.strokePath (bg, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // value arc with a soft glow underneath
        juce::Path val;
        val.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (juce::Colour (cAccent).withAlpha (0.28f));
        g.strokePath (val, juce::PathStrokeType (lineW * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (cAccent));
        g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // knob body: soft drop shadow + vertical sheen for depth
        const float knobR = radius - lineW * 1.8f;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (centre.x - knobR, centre.y - knobR + 1.5f, knobR * 2.0f, knobR * 2.0f);

        juce::ColourGradient grad (juce::Colour (cPanelHead).brighter (0.3f),
                                   centre.x, centre.y - knobR,
                                   juce::Colour (cPanelHead).darker (0.15f),
                                   centre.x, centre.y + knobR, false);
        g.setGradientFill (grad);
        g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

        // pointer + hub
        juce::Point<float> tip (centre.x + (knobR - 3.0f) * std::cos (angle - juce::MathConstants<float>::halfPi),
                                centre.y + (knobR - 3.0f) * std::sin (angle - juce::MathConstants<float>::halfPi));
        g.setColour (juce::Colour (cAccent));
        g.drawLine ({ centre, tip }, 2.5f);
        g.fillEllipse (centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);
    }

    //==============================================================================
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minPos, float maxPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            const float cx = (float) x + width * 0.5f;
            const float trackW = 4.0f;

            g.setColour (juce::Colour (cTrack));
            g.fillRoundedRectangle (cx - trackW * 0.5f, (float) y, trackW, (float) height, 2.0f);

            // centre line (0 dB)
            const float midY = (float) y + height * 0.5f;
            g.setColour (juce::Colour (cTextDim).withAlpha (0.4f));
            g.fillRect ((float) x + 2.0f, midY - 0.5f, (float) width - 4.0f, 1.0f);

            // fill from the centre point to the thumb
            g.setColour (juce::Colour (cAccent));
            const float top = juce::jmin (sliderPos, midY);
            const float bot = juce::jmax (sliderPos, midY);
            g.fillRoundedRectangle (cx - trackW * 0.5f, top, trackW, bot - top, 2.0f);

            // thumb
            g.setColour (juce::Colour (cText));
            g.fillRoundedRectangle (cx - 8.0f, sliderPos - 3.0f, 16.0f, 6.0f, 3.0f);
        }
        else if (style == juce::Slider::LinearHorizontal)
        {
            const float cy = (float) y + height * 0.5f;
            const float trackH = 4.0f;

            g.setColour (juce::Colour (cTrack));
            g.fillRoundedRectangle ((float) x, cy - trackH * 0.5f, (float) width, trackH, 2.0f);

            // fill up to the thumb
            g.setColour (juce::Colour (cAccent));
            g.fillRoundedRectangle ((float) x, cy - trackH * 0.5f, sliderPos - (float) x, trackH, 2.0f);

            // thumb: accent core with a soft halo and a bright centre
            g.setColour (juce::Colour (cAccent).withAlpha (0.25f));
            g.fillEllipse (sliderPos - 10.0f, cy - 10.0f, 20.0f, 20.0f);
            g.setColour (juce::Colour (cAccent));
            g.fillEllipse (sliderPos - 6.5f, cy - 6.5f, 13.0f, 13.0f);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.fillEllipse (sliderPos - 2.0f, cy - 2.0f, 4.0f, 4.0f);

            juce::ignoreUnused (minPos, maxPos, slider);
        }
        else
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
        }
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (juce::Font (juce::FontOptions (12.0f)));
        return label;
    }

    //==============================================================================
    // Respect each label's own font (a fixed override here used to flatten ALL
    // text to 12 px — killing the title/readout hierarchy).
    juce::Font getLabelFont (juce::Label& label) override
    {
        return label.getFont();
    }

    // Labels with a non-transparent background render as borderless rounded
    // "chips" (slider value boxes, stepper values) instead of hard rectangles.
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        const auto bg = label.findColour (juce::Label::backgroundColourId);
        if (! bg.isTransparent())
        {
            g.setColour (bg);
            g.fillRoundedRectangle (label.getLocalBounds().toFloat(), 4.0f);
        }

        if (! label.isBeingEdited())
        {
            const float alpha = label.isEnabled() ? 1.0f : 0.5f;
            const auto font = getLabelFont (label);
            g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
            g.setFont (font);

            auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
            g.drawFittedText (label.getText(), textArea, label.getJustificationType(),
                              juce::jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())),
                              label.getMinimumHorizontalScale());
        }
    }

    //==============================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const float corner = 5.0f;

        auto base = backgroundColour;
        if (shouldDrawButtonAsDown)             base = base.darker (0.2f);
        else if (shouldDrawButtonAsHighlighted) base = base.brighter (0.15f);

        g.setColour (base);
        g.fillRoundedRectangle (bounds, corner);

        // hairline border: accent-tinted when toggled on, subtle otherwise
        g.setColour (button.getToggleState() ? base.brighter (0.3f).withAlpha (0.9f)
                                             : juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (12.5f, (float) buttonHeight * 0.55f),
                                              juce::Font::bold));
    }

    //==============================================================================
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        g.setColour (findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

        // chevron
        const float cx = (float) width - 12.0f;
        const float cy = (float) height * 0.5f;
        juce::Path p;
        p.startNewSubPath (cx - 4.0f, cy - 2.0f);
        p.lineTo (cx, cy + 2.5f);
        p.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (juce::Colour (cAccent));
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
        juce::ignoreUnused (box);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions (13.0f));
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font (juce::FontOptions (13.0f));
    }
};
