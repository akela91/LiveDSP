#pragma once

#include <JuceHeader.h>
#include <vector>
#include "GuitarLookAndFeel.h"

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
/** Közös bázis a modul-panelekhez (egységes elrendezéshez). */
struct PanelBase : juce::Component
{
    virtual int getPreferredWidth() const = 0;
};

//==============================================================================
/** Rotary knob + felirat + APVTS attachment egy csomagban. */
class KnobControl : public juce::Component
{
public:
    KnobControl (APVTS& state, const juce::String& paramId, const juce::String& labelText)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 16);
        addAndMakeVisible (slider);

        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colour (GuitarLookAndFeel::cTextDim));
        label.setFont (juce::Font (juce::FontOptions (11.0f)));
        addAndMakeVisible (label);

        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (14));
        slider.setBounds (r);
    }

    juce::Slider slider;

private:
    juce::Label label;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
/** Függőleges fader + felirat (EQ-hoz). */
class FaderControl : public juce::Component
{
public:
    FaderControl (APVTS& state, const juce::String& paramId, const juce::String& labelText)
    {
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (slider);

        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colour (GuitarLookAndFeel::cTextDim));
        label.setFont (juce::Font (juce::FontOptions (9.5f)));
        addAndMakeVisible (label);

        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramId, slider);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (14));
        slider.setBounds (r.reduced (2, 0));
    }

private:
    juce::Slider slider;
    juce::Label  label;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
/** Kerek "power" kapcsoló (akcentus, ha be van kapcsolva). */
class PowerButton : public juce::Button
{
public:
    PowerButton (APVTS& state, const juce::String& paramId) : juce::Button ({})
    {
        setClickingTogglesState (true);
        attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramId, *this);
    }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced (2.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const bool on = getToggleState();

        const auto col = on ? juce::Colour (GuitarLookAndFeel::cAccent)
                            : juce::Colour (GuitarLookAndFeel::cTextDim);
        g.setColour (col);
        g.drawEllipse (c.x - r + 1.5f, c.y - r + 1.5f, (r - 1.5f) * 2.0f, (r - 1.5f) * 2.0f, 1.6f);
        // power ikon: függőleges vonal felül
        g.fillRoundedRectangle (c.x - 1.0f, c.y - r + 2.0f, 2.0f, r * 0.8f, 1.0f);
        if (on)
            g.setColour (col.withAlpha (0.25f)), g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

//==============================================================================
/** Sötét, lekerekített modul-panel: fejléc (cím + opcionális power), tartalom. */
class ModulePanel : public PanelBase
{
public:
    enum class Icon { none, amp, cab };
    struct Knob { juce::String id, label; };

    ModulePanel (APVTS& state, juce::String titleText,
                 juce::String toggleId, std::vector<Knob> knobDefs)
        : title (std::move (titleText))
    {
        if (toggleId.isNotEmpty())
        {
            power = std::make_unique<PowerButton> (state, toggleId);
            addAndMakeVisible (*power);
        }
        for (auto& k : knobDefs)
        {
            auto* kc = new KnobControl (state, k.id, k.label);
            knobs.add (kc);
            addAndMakeVisible (kc);
        }
    }

    void setIcon (Icon i) { icon = i; repaint(); }

    int getPreferredWidth() const override
    {
        if (icon != Icon::none && knobs.isEmpty())
            return 120;
        return juce::jmax (90, 18 + knobs.size() * knobWidth);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (GuitarLookAndFeel::cPanel));
        g.fillRoundedRectangle (b, 8.0f);

        auto header = b.removeFromTop (headerH);
        g.setColour (juce::Colour (GuitarLookAndFeel::cPanelHead));
        g.fillRoundedRectangle (header, 8.0f);
        g.fillRect (header.withTop (header.getCentreY()));

        g.setColour (juce::Colour (GuitarLookAndFeel::cAccent));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        auto textArea = header.toNearestInt().reduced (10, 0);
        if (power != nullptr) textArea.removeFromRight (headerH);
        g.drawText (title, textArea, juce::Justification::centredLeft);

        if (icon != Icon::none && knobs.isEmpty())
            drawIcon (g, getLocalBounds().withTrimmedTop (headerH).reduced (14).toFloat());
    }

    void drawIcon (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto accent = juce::Colour (GuitarLookAndFeel::cAccent);
        const auto dim    = juce::Colour (GuitarLookAndFeel::cTextDim);
        auto box = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 84.0f),
                                               juce::jmin (area.getHeight(), 70.0f));

        if (icon == Icon::amp)
        {
            // amp fej: keret + rácsvonalak + 3 gomb felül
            g.setColour (dim);
            g.drawRoundedRectangle (box, 6.0f, 2.0f);
            auto grille = box.reduced (10.0f).withTrimmedTop (14.0f);
            g.setColour (accent.withAlpha (0.85f));
            for (float gx = grille.getX() + 3.0f; gx < grille.getRight(); gx += 6.0f)
                g.drawLine (gx, grille.getY(), gx, grille.getBottom(), 1.2f);
            for (int k = 0; k < 3; ++k)
            {
                const float cx = box.getX() + 14.0f + k * 16.0f;
                g.setColour (accent);
                g.fillEllipse (cx - 3.0f, box.getY() + 5.0f, 6.0f, 6.0f);
            }
        }
        else // cab
        {
            g.setColour (dim);
            g.drawRoundedRectangle (box, 6.0f, 2.0f);
            const auto c = box.getCentre();
            const float r = juce::jmin (box.getWidth(), box.getHeight()) * 0.36f;
            g.setColour (accent.withAlpha (0.8f));
            g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 2.0f);
            g.setColour (accent);
            g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.45f, r * 0.9f, r * 0.9f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (headerH);
        if (power != nullptr)
            power->setBounds (header.removeFromRight (headerH).reduced (4));

        r.reduce (8, 6);
        if (! knobs.isEmpty())
        {
            const int w = r.getWidth() / knobs.size();
            for (auto* k : knobs)
                k->setBounds (r.removeFromLeft (w).reduced (2));
        }
    }

private:
    static constexpr int headerH   = 24;
    static constexpr int knobWidth = 66;

    juce::String title;
    Icon icon { Icon::none };
    std::unique_ptr<PowerButton> power;
    juce::OwnedArray<KnobControl> knobs;
};

//==============================================================================
/** EQ-panel: 9 függőleges fader + power. */
class EqPanel : public PanelBase
{
public:
    EqPanel (APVTS& state, const juce::StringArray& bandLabels)
    {
        power = std::make_unique<PowerButton> (state, "eqOn");
        addAndMakeVisible (*power);

        for (int i = 0; i < bandLabels.size(); ++i)
        {
            auto* f = new FaderControl (state, "eqBand" + juce::String (i), bandLabels[i]);
            faders.add (f);
            addAndMakeVisible (f);
        }
    }

    int getPreferredWidth() const override { return 18 + scaleW + faders.size() * 30; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (GuitarLookAndFeel::cPanel));
        g.fillRoundedRectangle (b, 8.0f);

        auto header = b.removeFromTop (24.0f);
        g.setColour (juce::Colour (GuitarLookAndFeel::cPanelHead));
        g.fillRoundedRectangle (header, 8.0f);
        g.fillRect (header.withTop (header.getCentreY()));
        g.setColour (juce::Colour (GuitarLookAndFeel::cAccent));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText ("EQUALIZER", header.toNearestInt().reduced (10, 0).withTrimmedRight (24),
                    juce::Justification::centredLeft);

        // dB-skála + rácsvonalak a fader-régióban
        auto body = getContentBounds();
        auto travel = body.withTrimmedBottom (14);   // a fader csúszó-tartománya
        const int dbMarks[] = { 15, 10, 5, 0, -5, -10, -15 };
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        for (int dB : dbMarks)
        {
            const float t = 1.0f - (float) (dB + 15) / 30.0f;       // 0=tető, 1=alja
            const float y = travel.getY() + t * travel.getHeight();
            g.setColour (juce::Colour (GuitarLookAndFeel::cTrack).withAlpha (dB == 0 ? 0.9f : 0.4f));
            g.drawLine ((float) body.getX(), y, (float) body.getRight(), y, dB == 0 ? 1.2f : 0.8f);
            g.setColour (juce::Colour (GuitarLookAndFeel::cTextDim));
            g.drawText ((dB > 0 ? "+" : "") + juce::String (dB),
                        2, (int) y - 7, scaleW - 2, 14, juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (24);
        power->setBounds (header.removeFromRight (24).reduced (4));

        auto body = getContentBounds();
        if (! faders.isEmpty())
        {
            const int w = body.getWidth() / faders.size();
            for (auto* f : faders)
                f->setBounds (body.removeFromLeft (w));
        }
    }

private:
    juce::Rectangle<int> getContentBounds() const
    {
        auto r = getLocalBounds();
        r.removeFromTop (24);
        r.reduce (8, 6);
        r.removeFromLeft (scaleW);   // dB-skála gutter (bal oldal)
        return r;
    }

    static constexpr int scaleW = 26;
public:

private:
    std::unique_ptr<PowerButton> power;
    juce::OwnedArray<FaderControl> faders;
};
