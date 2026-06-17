#pragma once

#include <JuceHeader.h>

/**
    Modern, sötét LookAndFeel a guitarDSP-hez (Neural DSP-stílus inspiráció).
    Meleg borostyán akcentus, sötét panelek, ívelt értékjelzős rotary knob-ok.
*/
class GuitarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Színpaletta
    static constexpr juce::uint32 cBackground = 0xff0d0d10;
    static constexpr juce::uint32 cPanel      = 0xff17171c;
    static constexpr juce::uint32 cPanelHead  = 0xff20202a;
    static constexpr juce::uint32 cAccent     = 0xffe0a04a;  // borostyán
    static constexpr juce::uint32 cAccentDim  = 0xff5a4a30;
    static constexpr juce::uint32 cText        = 0xffe9e9ec;
    static constexpr juce::uint32 cTextDim     = 0xff8a8a93;
    static constexpr juce::uint32 cTrack       = 0xff34343e;

    GuitarLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (cBackground));
        setColour (juce::Slider::textBoxTextColourId,        juce::Colour (cText));
        setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId,                juce::Colour (cText));
        setColour (juce::ComboBox::backgroundColourId,       juce::Colour (cPanelHead));
        setColour (juce::ComboBox::textColourId,             juce::Colour (cText));
        setColour (juce::ComboBox::outlineColourId,          juce::Colour (cTrack));
        setColour (juce::ComboBox::arrowColourId,            juce::Colour (cAccent));
        setColour (juce::PopupMenu::backgroundColourId,      juce::Colour (cPanel));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (cAccentDim));
        setColour (juce::PopupMenu::textColourId,            juce::Colour (cText));
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

        // háttér ív
        juce::Path bg;
        bg.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (cTrack));
        g.strokePath (bg, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // érték ív
        juce::Path val;
        val.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (juce::Colour (cAccent));
        g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // knob test
        const float knobR = radius - lineW * 1.8f;
        g.setColour (juce::Colour (cPanelHead).brighter (0.15f));
        g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour (juce::Colour (cTrack));
        g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

        // mutató
        juce::Point<float> tip (centre.x + (knobR - 3.0f) * std::cos (angle - juce::MathConstants<float>::halfPi),
                                centre.y + (knobR - 3.0f) * std::sin (angle - juce::MathConstants<float>::halfPi));
        g.setColour (juce::Colour (cAccent));
        g.drawLine ({ centre, tip }, 2.5f);
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

            // középvonal (0 dB)
            const float midY = (float) y + height * 0.5f;
            g.setColour (juce::Colour (cTextDim).withAlpha (0.4f));
            g.fillRect ((float) x + 2.0f, midY - 0.5f, (float) width - 4.0f, 1.0f);

            // kitöltés a középponttól a thumb-ig
            g.setColour (juce::Colour (cAccent));
            const float top = juce::jmin (sliderPos, midY);
            const float bot = juce::jmax (sliderPos, midY);
            g.fillRoundedRectangle (cx - trackW * 0.5f, top, trackW, bot - top, 2.0f);

            // thumb
            g.setColour (juce::Colour (cText));
            g.fillRoundedRectangle (cx - 8.0f, sliderPos - 3.0f, 16.0f, 6.0f, 3.0f);
        }
        else
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minPos, maxPos, style, slider);
        }
    }

    //==============================================================================
    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions (12.0f));
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        g.setColour (findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (juce::Colour (cTrack));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

        juce::Rectangle<int> arrow (width - 22, 0, 18, height);
        juce::Path p;
        p.addTriangle ((float) arrow.getCentreX() - 5, (float) arrow.getCentreY() - 2,
                       (float) arrow.getCentreX() + 5, (float) arrow.getCentreY() - 2,
                       (float) arrow.getCentreX(),     (float) arrow.getCentreY() + 4);
        g.setColour (juce::Colour (cAccent));
        g.fillPath (p);
        juce::ignoreUnused (box);
    }
};
