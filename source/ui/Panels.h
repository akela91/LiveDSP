#pragma once

#include <JuceHeader.h>
#include <vector>
#include "LiveLookAndFeel.h"

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
        label.setColour (juce::Label::textColourId, juce::Colour (LiveLookAndFeel::cTextDim));
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
        label.setColour (juce::Label::textColourId, juce::Colour (LiveLookAndFeel::cTextDim));
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

        const auto col = on ? juce::Colour (LiveLookAndFeel::cAccent)
                            : juce::Colour (LiveLookAndFeel::cTextDim);
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

    // Aktivitás-LED: a fejlécben egy kis jelző, ami KIGYULLAD, amikor a modul
    // épp dolgozik (kapu némít / kompresszor vág). A 'lit' 0 = ki, 1 = teljes.
    void enableLed (bool b) { hasLed = b; }
    void setLedLevel (float lit)
    {
        lit = juce::jlimit (0.0f, 1.0f, lit);
        if (std::abs (lit - ledLit) > 0.02f) { ledLit = lit; repaint(); }
    }

    int getPreferredWidth() const override
    {
        if (icon != Icon::none && knobs.isEmpty())
            return 120;
        return juce::jmax (90, 18 + knobs.size() * knobWidth);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (LiveLookAndFeel::cPanel));
        g.fillRoundedRectangle (b, 8.0f);

        auto header = b.removeFromTop (headerH);
        g.setColour (juce::Colour (LiveLookAndFeel::cPanelHead));
        g.fillRoundedRectangle (header, 8.0f);
        g.fillRect (header.withTop (header.getCentreY()));

        g.setColour (juce::Colour (LiveLookAndFeel::cAccent));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        auto textArea = header.toNearestInt().reduced (10, 0);
        if (power != nullptr) textArea.removeFromRight (headerH);
        if (hasLed)          textArea.removeFromRight (16);
        g.drawText (title, textArea, juce::Justification::centredLeft);

        if (hasLed)
        {
            // A LED a power gomb mellett balra. Aktivitásnál (ledLit nagy) kigyullad.
            const float closed = ledLit;
            auto hb = header.toNearestInt();
            if (power != nullptr) hb.removeFromRight (headerH);
            auto dot = hb.removeFromRight (16).withSizeKeepingCentre (9, 9).toFloat();
            const auto lit = juce::Colour (0xffe0653a);   // meleg narancs = némítás
            g.setColour (juce::Colour (LiveLookAndFeel::cTrack));
            g.fillEllipse (dot);
            if (closed > 0.05f)
            {
                g.setColour (lit.withAlpha (closed));
                g.fillEllipse (dot);
                g.setColour (lit.withAlpha (0.25f * closed));
                g.fillEllipse (dot.expanded (3.0f));      // halo
            }
        }

        if (icon != Icon::none && knobs.isEmpty())
            drawIcon (g, getLocalBounds().withTrimmedTop (headerH).reduced (14).toFloat());
    }

    void drawIcon (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto accent = juce::Colour (LiveLookAndFeel::cAccent);
        const auto dim    = juce::Colour (LiveLookAndFeel::cTextDim);
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
            // A knob-blokkot KÖZÉPRE igazítjuk mindkét tengelyen, maximált
            // mérettel — így a panel szélesítésekor sem úsznak szét a knobok.
            const int kh = juce::jmin (r.getHeight(), 108);
            const int kw = juce::jmin (r.getWidth(),  knobs.size() * knobWidth);
            auto      row = r.withSizeKeepingCentre (kw, kh);
            const int w   = row.getWidth() / knobs.size();
            for (auto* k : knobs)
                k->setBounds (row.removeFromLeft (w).reduced (2));
        }
    }

private:
    static constexpr int headerH   = 24;
    static constexpr int knobWidth = 66;

    juce::String title;
    Icon icon { Icon::none };
    bool  hasLed  { false };
    float ledLit  { 0.0f };
    std::unique_ptr<PowerButton> power;
    juce::OwnedArray<KnobControl> knobs;
};

//==============================================================================
/** Választós panel (AMP / CAB): fejléc (cím + power) + egy ComboBox (modell/IR
    választó) + alatta a modul ikonja. A combót a befoglaló nézet tölti fel
    (getCombo()), így a fájl-logika ott marad. */
class ComboPanel : public PanelBase
{
public:
    enum class Icon { amp, cab };

    ComboPanel (APVTS& state, juce::String titleText, juce::String toggleId,
                Icon iconType, int widthPx = 200)
        : title (std::move (titleText)), icon (iconType), width (widthPx)
    {
        if (toggleId.isNotEmpty())
        {
            power = std::make_unique<PowerButton> (state, toggleId);
            addAndMakeVisible (*power);
        }

        combo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (LiveLookAndFeel::cPanelHead));
        combo.setColour (juce::ComboBox::textColourId,       juce::Colour (LiveLookAndFeel::cText));
        combo.setColour (juce::ComboBox::arrowColourId,      juce::Colour (LiveLookAndFeel::cAccent));
        combo.setColour (juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
        addAndMakeVisible (combo);
    }

    juce::ComboBox& getCombo() noexcept { return combo; }

    int getPreferredWidth() const override { return width; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (LiveLookAndFeel::cPanel));
        g.fillRoundedRectangle (b, 8.0f);

        auto header = b.removeFromTop (24.0f);
        g.setColour (juce::Colour (LiveLookAndFeel::cPanelHead));
        g.fillRoundedRectangle (header, 8.0f);
        g.fillRect (header.withTop (header.getCentreY()));
        g.setColour (juce::Colour (LiveLookAndFeel::cAccent));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        auto textArea = header.toNearestInt().reduced (10, 0);
        if (power != nullptr) textArea.removeFromRight (24);
        g.drawText (title, textArea, juce::Justification::centredLeft);

        // Ikon a combó alatti területen.
        auto iconArea = getLocalBounds().withTrimmedTop (24 + 30).reduced (14, 8).toFloat();
        drawIcon (g, iconArea);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (24);
        if (power != nullptr)
            power->setBounds (header.removeFromRight (24).reduced (4));

        r.reduce (8, 6);
        combo.setBounds (r.removeFromTop (24));
    }

private:
    void drawIcon (juce::Graphics& g, juce::Rectangle<float> area)
    {
        const auto accent = juce::Colour (LiveLookAndFeel::cAccent);
        const auto dim    = juce::Colour (LiveLookAndFeel::cTextDim);
        auto box = area.withSizeKeepingCentre (juce::jmin (area.getWidth(), 92.0f),
                                               juce::jmin (area.getHeight(), 64.0f));
        if (box.getHeight() < 18.0f) return;

        if (icon == Icon::amp)
        {
            g.setColour (dim);
            g.drawRoundedRectangle (box, 6.0f, 2.0f);
            auto grille = box.reduced (10.0f).withTrimmedTop (12.0f);
            g.setColour (accent.withAlpha (0.85f));
            for (float gx = grille.getX() + 3.0f; gx < grille.getRight(); gx += 6.0f)
                g.drawLine (gx, grille.getY(), gx, grille.getBottom(), 1.2f);
            for (int k = 0; k < 3; ++k)
            {
                g.setColour (accent);
                g.fillEllipse (box.getX() + 12.0f + k * 14.0f, box.getY() + 5.0f, 5.0f, 5.0f);
            }
        }
        else // cab
        {
            g.setColour (dim);
            g.drawRoundedRectangle (box, 6.0f, 2.0f);
            const auto c = box.getCentre();
            const float rr = juce::jmin (box.getWidth(), box.getHeight()) * 0.36f;
            g.setColour (accent.withAlpha (0.8f));
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 2.0f);
            g.setColour (accent);
            g.fillEllipse (c.x - rr * 0.45f, c.y - rr * 0.45f, rr * 0.9f, rr * 0.9f);
        }
    }

    juce::String title;
    Icon icon;
    int width;
    std::unique_ptr<PowerButton> power;
    juce::ComboBox combo;
};

//==============================================================================
/** INPUT-panel: bemeneti csatorna-választó ComboBox + Gain knob. A csatorna-
    választás a processzortól független (numChannels + currentCh + onChannel
    callbacken át), így a Panels.h nem hivatkozik a processzorra. */
class InputPanel : public PanelBase
{
public:
    InputPanel (APVTS& state, const juce::String& gainParamId, const juce::String& gainLabel,
                int numChannels, int currentCh, std::function<void (int)> onChannel)
        : onChannelChange (std::move (onChannel))
    {
        channel.setColour (juce::ComboBox::backgroundColourId, juce::Colour (LiveLookAndFeel::cPanelHead));
        channel.setColour (juce::ComboBox::textColourId,       juce::Colour (LiveLookAndFeel::cText));
        channel.setColour (juce::ComboBox::arrowColourId,      juce::Colour (LiveLookAndFeel::cAccent));
        channel.setColour (juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);

        const int n = juce::jmax (2, numChannels);   // legalább 2 elemet kínáljunk
        for (int i = 0; i < n; ++i)
            channel.addItem ("In " + juce::String (i + 1), i + 1);
        channel.setSelectedId (juce::jlimit (0, n - 1, currentCh) + 1, juce::dontSendNotification);
        channel.onChange = [this]
        {
            if (onChannelChange) onChannelChange (channel.getSelectedId() - 1);
        };
        addAndMakeVisible (channel);

        gain = std::make_unique<KnobControl> (state, gainParamId, gainLabel);
        addAndMakeVisible (*gain);
    }

    int getPreferredWidth() const override { return 104; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (LiveLookAndFeel::cPanel));
        g.fillRoundedRectangle (b, 8.0f);

        auto header = b.removeFromTop (24.0f);
        g.setColour (juce::Colour (LiveLookAndFeel::cPanelHead));
        g.fillRoundedRectangle (header, 8.0f);
        g.fillRect (header.withTop (header.getCentreY()));
        g.setColour (juce::Colour (LiveLookAndFeel::cAccent));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText ("INPUT", header.toNearestInt().reduced (10, 0), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (24);
        r.reduce (8, 6);
        channel.setBounds (r.removeFromTop (22));
        r.removeFromTop (4);
        const int kh = juce::jmin (r.getHeight(), 96);
        gain->setBounds (r.withSizeKeepingCentre (r.getWidth(), kh));
    }

private:
    juce::ComboBox channel;
    std::unique_ptr<KnobControl> gain;
    std::function<void (int)> onChannelChange;
};

//==============================================================================
/** PITCH-panel: power + SEMI/LAT knob + motorválasztó (Signalsmith/RubberBand). */
class PitchPanel : public PanelBase
{
public:
    explicit PitchPanel (APVTS& state)
    {
        power = std::make_unique<PowerButton> (state, "pitchOn");
        addAndMakeVisible (*power);

        semi = std::make_unique<KnobControl> (state, "pitchSemitones", "SEMI");
        lat  = std::make_unique<KnobControl> (state, "pitchLatency",   "LAT");
        addAndMakeVisible (*semi);
        addAndMakeVisible (*lat);

        auto styleCombo = [] (juce::ComboBox& c)
        {
            c.setColour (juce::ComboBox::backgroundColourId, juce::Colour (LiveLookAndFeel::cPanelHead));
            c.setColour (juce::ComboBox::textColourId,       juce::Colour (LiveLookAndFeel::cText));
            c.setColour (juce::ComboBox::arrowColourId,      juce::Colour (LiveLookAndFeel::cAccent));
            c.setColour (juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
        };

        engine.addItem ("Signalsmith", 1);
        engine.addItem ("RubberBand",  2);
        engine.addItem ("RB Live",     3);
        styleCombo (engine);
        addAndMakeVisible (engine);
        engineAtt = std::make_unique<APVTS::ComboBoxAttachment> (state, "pitchEngine", engine);

        // RB Live minőség/latencia profil (csak az RB Live motornál releváns).
        quality.addItem ("Fast", 1);
        quality.addItem ("Fine", 2);
        styleCombo (quality);
        addAndMakeVisible (quality);
        qualityAtt = std::make_unique<APVTS::ComboBoxAttachment> (state, "pitchLiveQuality", quality);
    }

    int getPreferredWidth() const override { return 200; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (LiveLookAndFeel::cPanel));
        g.fillRoundedRectangle (b, 8.0f);

        auto header = b.removeFromTop (24.0f);
        g.setColour (juce::Colour (LiveLookAndFeel::cPanelHead));
        g.fillRoundedRectangle (header, 8.0f);
        g.fillRect (header.withTop (header.getCentreY()));

        g.setColour (juce::Colour (LiveLookAndFeel::cAccent));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText ("PITCH", header.toNearestInt().reduced (10, 0).withTrimmedRight (24),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (24);
        power->setBounds (header.removeFromRight (24).reduced (4));

        r.reduce (8, 6);

        // Alsó sor: motorválasztó (bal) + RB Live minőség (jobb). A minőség-combo
        // szélesebb, hogy a "Fast"/"Fine" felirat ne vágódjon le.
        auto bottom = r.removeFromBottom (20);
        quality.setBounds (bottom.removeFromRight (juce::jmin (76, bottom.getWidth() / 2)));
        bottom.removeFromRight (4);
        engine.setBounds (bottom);
        r.removeFromBottom (4);

        const int w = r.getWidth() / 2;
        semi->setBounds (r.removeFromLeft (w).reduced (2));
        lat->setBounds  (r.reduced (2));
    }

private:
    std::unique_ptr<PowerButton>  power;
    std::unique_ptr<KnobControl>  semi;
    std::unique_ptr<KnobControl>  lat;
    juce::ComboBox                engine;
    juce::ComboBox                quality;
    std::unique_ptr<APVTS::ComboBoxAttachment> engineAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> qualityAtt;
};

//==============================================================================
/** Csak-kijelző panel: fejléc + egy középre igazított FIX érték (pl. "90 Hz").
    Olyan modulokhoz, amelyeknek nincs állítható paramétere (low-cut, limiter). */
class InfoPanel : public PanelBase
{
public:
    InfoPanel (juce::String titleText, juce::String valueText, int widthPx = 96)
        : title (std::move (titleText)), value (std::move (valueText)), width (widthPx) {}

    int getPreferredWidth() const override { return width; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour (LiveLookAndFeel::cPanel));
        g.fillRoundedRectangle (b, 8.0f);

        auto header = b.removeFromTop (24.0f);
        g.setColour (juce::Colour (LiveLookAndFeel::cPanelHead));
        g.fillRoundedRectangle (header, 8.0f);
        g.fillRect (header.withTop (header.getCentreY()));
        g.setColour (juce::Colour (LiveLookAndFeel::cAccent));
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawText (title, header.toNearestInt().reduced (10, 0), juce::Justification::centredLeft);

        // FIX érték a panel közepén.
        g.setColour (juce::Colour (LiveLookAndFeel::cText));
        g.setFont (juce::Font (juce::FontOptions (17.0f, juce::Font::bold)));
        g.drawText (value, getLocalBounds().withTrimmedTop (24), juce::Justification::centred);

        g.setColour (juce::Colour (LiveLookAndFeel::cTextDim));
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText ("fixed", getLocalBounds().withTrimmedTop (24).removeFromBottom (18),
                    juce::Justification::centred);
    }

private:
    juce::String title, value;
    int width;
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
        g.setColour (juce::Colour (LiveLookAndFeel::cPanel));
        g.fillRoundedRectangle (b, 8.0f);

        auto header = b.removeFromTop (24.0f);
        g.setColour (juce::Colour (LiveLookAndFeel::cPanelHead));
        g.fillRoundedRectangle (header, 8.0f);
        g.fillRect (header.withTop (header.getCentreY()));
        g.setColour (juce::Colour (LiveLookAndFeel::cAccent));
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
            g.setColour (juce::Colour (LiveLookAndFeel::cTrack).withAlpha (dB == 0 ? 0.9f : 0.4f));
            g.drawLine ((float) body.getX(), y, (float) body.getRight(), y, dB == 0 ? 1.2f : 0.8f);
            g.setColour (juce::Colour (LiveLookAndFeel::cTextDim));
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
