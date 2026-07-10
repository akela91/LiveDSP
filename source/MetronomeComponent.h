#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/Panels.h"   // KnobControl + LiveLookAndFeel

/**
    Integer stepper: [−] value [+] — the buttons sit on the TWO SIDES of the
    value box (juce::Slider's IncDecButtons stacks them next to each other,
    which gets unreadably small). Attached to an APVTS parameter; steps by the
    parameter's interval, clamped to its range.
*/
class Stepper : public juce::Component
{
public:
    Stepper (APVTS& state, const juce::String& paramId)
        : param (*state.getParameter (paramId)),
          attachment (param, [this] (float v) { setDisplayValue (v); })
    {
        auto initButton = [this] (juce::TextButton& b, int delta)
        {
            b.setColour (juce::TextButton::buttonColourId,  juce::Colour (LiveLookAndFeel::cPanelHead));
            b.setColour (juce::TextButton::textColourOffId, juce::Colour (LiveLookAndFeel::cAccent));
            b.onClick = [this, delta] { bump (delta); };
            addAndMakeVisible (b);
        };
        initButton (minusButton, -1);
        initButton (plusButton,  +1);

        valueLabel.setJustificationType (juce::Justification::centred);
        valueLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
        valueLabel.setColour (juce::Label::backgroundColourId, juce::Colour (LiveLookAndFeel::cPanelHead));
        valueLabel.setColour (juce::Label::textColourId,       juce::Colour (LiveLookAndFeel::cText));
        addAndMakeVisible (valueLabel);

        attachment.sendInitialUpdate();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        minusButton.setBounds (r.removeFromLeft (24));
        plusButton.setBounds  (r.removeFromRight (24));
        valueLabel.setBounds  (r.reduced (2, 0));
    }

private:
    void bump (int delta)
    {
        const auto& range = param.getNormalisableRange();
        const float step  = range.interval > 0.0f ? range.interval : 1.0f;
        attachment.setValueAsCompleteGesture (
            juce::jlimit (range.start, range.end, current + (float) delta * step));
    }

    void setDisplayValue (float v)
    {
        current = v;
        valueLabel.setText (juce::String (juce::roundToInt (v)), juce::dontSendNotification);
    }

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment   attachment;
    float current { 0.0f };

    juce::TextButton minusButton { juce::String::fromUTF8 ("\xe2\x88\x92") };   // −
    juce::TextButton plusButton  { "+" };
    juce::Label      valueLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Stepper)
};

/**
    Practice metronome panel (guitar mode). Slides in below the top bar, next to
    the tuner (the METRO button toggles it, like TUNER toggles the tuner).

    Controls:
      START/STOP + TAP tempo + big live BPM readout (follows the speed trainer)
      TEMPO slider (30..300) · VOL knob
      BEATS (1..12) · SUBDIV (1/4, 1/8, triplet, 1/16) · SOUND (beep/wood/kick/hat)
      ACCENT toggle · TRAINER (+BPM every N bars) · GAP (play X / mute Y bars)
      Beat dots: live beat position, downbeat highlighted, orange while a gap
      bar is muted.

    Everything except START/STOP is an APVTS parameter (persisted). Display
    state is polled from the Metronome engine on a Timer.
*/
class MetronomeComponent : public juce::Component,
                           private juce::Timer
{
public:
    explicit MetronomeComponent (LiveDspProcessor& p) : processorRef (p)
    {
        auto& s = processorRef.apvts;

        // --- transport ------------------------------------------------------
        playButton.setClickingTogglesState (true);
        playButton.setColour (juce::TextButton::buttonColourId,   juce::Colour (LiveLookAndFeel::cPanelHead));
        playButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (LiveLookAndFeel::cAccent));
        playButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (LiveLookAndFeel::cText));
        playButton.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
        playButton.onClick = [this]
        {
            processorRef.setMetronomePlaying (playButton.getToggleState());
            updatePlayButton();
        };
        addAndMakeVisible (playButton);

        tapButton.setColour (juce::TextButton::buttonColourId,  juce::Colour (LiveLookAndFeel::cPanelHead));
        tapButton.setColour (juce::TextButton::textColourOffId, juce::Colour (LiveLookAndFeel::cText));
        tapButton.onClick = [this] { tapTempo(); };
        addAndMakeVisible (tapButton);

        // --- tempo + volume --------------------------------------------------
        bpmSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        bpmSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 26);
        addAndMakeVisible (bpmSlider);
        bpmAttach = std::make_unique<APVTS::SliderAttachment> (s, "metBpm", bpmSlider);

        volKnob = std::make_unique<KnobControl> (s, "metVolume", "VOL");
        addAndMakeVisible (*volKnob);

        // --- steppers / combos ------------------------------------------------
        beatsStep   = std::make_unique<Stepper> (s, "metBeats");
        trIncStep   = std::make_unique<Stepper> (s, "metTrainerInc");
        trBarsStep  = std::make_unique<Stepper> (s, "metTrainerBars");
        gapPlayStep = std::make_unique<Stepper> (s, "metGapPlay");
        gapMuteStep = std::make_unique<Stepper> (s, "metGapMute");
        for (auto* st : { beatsStep.get(), trIncStep.get(), trBarsStep.get(),
                          gapPlayStep.get(), gapMuteStep.get() })
            addAndMakeVisible (*st);

        auto initCombo = [&s, this] (juce::ComboBox& box, const char* paramId,
                                     const juce::StringArray& items,
                                     std::unique_ptr<APVTS::ComboBoxAttachment>& att)
        {
            box.setColour (juce::ComboBox::backgroundColourId, juce::Colour (LiveLookAndFeel::cPanelHead));
            box.setColour (juce::ComboBox::textColourId,       juce::Colour (LiveLookAndFeel::cText));
            box.setColour (juce::ComboBox::arrowColourId,      juce::Colour (LiveLookAndFeel::cAccent));
            box.setColour (juce::ComboBox::outlineColourId,    juce::Colours::transparentBlack);
            int id = 1;
            for (const auto& it : items)
                box.addItem (it, id++);
            addAndMakeVisible (box);
            att = std::make_unique<APVTS::ComboBoxAttachment> (s, paramId, box);
        };
        initCombo (subdivBox, "metSubdiv", { "1/4", "1/8", "Triplet", "1/16" },   subdivAttach);
        initCombo (soundBox,  "metSound",  { "Beep", "Wood", "Kick", "Hi-hat" }, soundAttach);

        // --- toggles ----------------------------------------------------------
        auto initToggle = [&s, this] (juce::TextButton& b, const char* paramId,
                                      std::unique_ptr<APVTS::ButtonAttachment>& att)
        {
            b.setClickingTogglesState (true);
            b.setColour (juce::TextButton::buttonColourId,   juce::Colour (LiveLookAndFeel::cPanelHead));
            b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (LiveLookAndFeel::cAccent));
            b.setColour (juce::TextButton::textColourOffId,  juce::Colour (LiveLookAndFeel::cText));
            b.setColour (juce::TextButton::textColourOnId,   juce::Colours::black);
            addAndMakeVisible (b);
            att = std::make_unique<APVTS::ButtonAttachment> (s, paramId, b);
        };
        initToggle (accentButton,  "metAccent",    accentAttach);
        initToggle (trainerButton, "metTrainerOn", trainerAttach);
        initToggle (gapButton,     "metGapOn",     gapAttach);

        // --- small header labels ----------------------------------------------
        auto initLabel = [this] (juce::Label& l, const char* text)
        {
            l.setText (text, juce::dontSendNotification);
            l.setJustificationType (juce::Justification::centred);
            l.setColour (juce::Label::textColourId, juce::Colour (LiveLookAndFeel::cTextDim));
            l.setFont (juce::Font (juce::FontOptions (11.0f)));
            addAndMakeVisible (l);
        };
        initLabel (lTempo,   "TEMPO");
        initLabel (lBeats,   "BEATS");
        initLabel (lSubdiv,  "SUBDIV");
        initLabel (lSound,   "SOUND");
        initLabel (lTrInc,   "+BPM");
        initLabel (lTrBars,  "EVERY");
        initLabel (lGapPlay, "PLAY");
        initLabel (lGapMute, "MUTE");
        lTempo.setJustificationType (juce::Justification::centredLeft);

        updatePlayButton();
        startTimerHz (30);
    }

    ~MetronomeComponent() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced (10);
        LiveLookAndFeel::drawPanelFrame (g, area.toFloat());

        // Section separators (computed in resized()).
        g.setColour (juce::Colour (LiveLookAndFeel::cPanelHead));
        g.fillRect (mainSep);
        for (const auto& sep : groupSeps)
            g.fillRect (sep);

        // Big live BPM readout (shows the trainer-raised effective tempo).
        // NOTE: draw into a LOCAL copy — mutating the cached rect here would
        // shift the text a little on every repaint.
        auto readout = bpmRect;
        auto& met = processorRef.getMetronome();
        g.setColour (met.isPlaying() ? juce::Colour (LiveLookAndFeel::cAccent)
                                     : juce::Colours::grey);
        g.setFont (juce::Font (juce::FontOptions (42.0f, juce::Font::bold)));
        g.drawText (juce::String (juce::roundToInt (met.getDisplayBpm())),
                    readout.withTrimmedBottom (16), juce::Justification::centred);
        g.setColour (juce::Colour (LiveLookAndFeel::cTextDim));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText ("BPM", readout.removeFromBottom (14), juce::Justification::centred);

        drawBeatDots (g);
    }

    void resized() override
    {
        groupSeps.clear();
        auto r = getLocalBounds().reduced (10).reduced (14, 10);

        // Beat dots along the bottom, full width.
        dotsRect = r.removeFromBottom (26);
        r.removeFromBottom (8);

        // Left column: transport + big BPM readout.
        auto left = r.removeFromLeft (150);
        playButton.setBounds (left.removeFromTop (54));
        left.removeFromTop (8);
        tapButton.setBounds (left.removeFromTop (32));
        left.removeFromTop (4);
        bpmRect = left;

        r.removeFromLeft (12);
        mainSep = r.removeFromLeft (1);      // vertical separator next to the transport
        r.removeFromLeft (13);

        // Right edge: VOL knob column (spans the control rows).
        auto volCol = r.removeFromRight (88);
        volKnob->setBounds (volCol.withSizeKeepingCentre (88, juce::jmin (volCol.getHeight(), 124)));
        r.removeFromRight (12);
        groupSeps.push_back (r.removeFromRight (1));
        r.removeFromRight (11);

        // Row A: TEMPO label + big slider.
        auto rowA = r.removeFromTop (58);
        lTempo.setBounds (rowA.removeFromTop (16));
        bpmSlider.setBounds (rowA);
        r.removeFromTop (10);

        // Labelled cell: 16 px header label + 36 px control.
        auto place = [] (juce::Rectangle<int>& row, juce::Label* l, juce::Component& c,
                         int w, int gap = 10)
        {
            auto cell = row.removeFromLeft (w);
            row.removeFromLeft (gap);
            auto head = cell.removeFromTop (16);
            if (l != nullptr) l->setBounds (head);
            c.setBounds (cell);
        };
        auto groupGap = [this] (juce::Rectangle<int>& row)
        {
            row.removeFromLeft (8);
            groupSeps.push_back (row.removeFromLeft (1));
            row.removeFromLeft (9);
        };

        // Row B1: rhythm (BEATS + SUBDIV) | sound (SOUND + ACCENT).
        // NOTE: the row is ~600 px wide — the cell widths below MUST fit in it,
        // or the last cell gets truncated (its value box collapses to nothing).
        auto b1 = r.removeFromTop (52);
        place (b1, &lBeats,  *beatsStep, 92);
        place (b1, &lSubdiv, subdivBox, 118);
        groupGap (b1);
        place (b1, &lSound,  soundBox, 118);
        place (b1, nullptr,  accentButton, 88);
        r.removeFromTop (8);

        // Row B2: speed trainer | gap trainer.
        auto b2 = r.removeFromTop (52);
        place (b2, nullptr,   trainerButton, 84);
        place (b2, &lTrInc,   *trIncStep, 92);
        place (b2, &lTrBars,  *trBarsStep, 92);
        groupGap (b2);
        place (b2, nullptr,   gapButton, 62);
        place (b2, &lGapPlay, *gapPlayStep, 92);
        place (b2, &lGapMute, *gapMuteStep, 92);
    }

private:
    //==========================================================================
    void drawBeatDots (juce::Graphics& g)
    {
        auto& met = processorRef.getMetronome();
        const int beats = (int) processorRef.apvts.getRawParameterValue ("metBeats")->load();
        const int cur   = met.getDisplayBeat();
        const bool muted = met.isBarMuted();

        const float d = 13.0f, gap = 8.0f;
        float x = dotsRect.getCentreX() - (beats * d + (beats - 1) * gap) * 0.5f;
        const float y = dotsRect.getCentreY() - d * 0.5f;

        for (int i = 0; i < beats; ++i)
        {
            const bool active = (i == cur);
            juce::Colour col = i == 0 ? juce::Colour (LiveLookAndFeel::cAccent)
                                      : juce::Colours::white;
            if (muted) col = juce::Colours::orange;

            g.setColour (active ? col : col.withAlpha (0.18f));
            auto dot = juce::Rectangle<float> (x, y, d, d);
            if (active)
                dot = dot.expanded (1.5f);
            g.fillEllipse (dot);
            x += d + gap;
        }
    }

    // Tap tempo: average of the last few tap intervals -> metBpm parameter.
    void tapTempo()
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (lastTapMs > 0.0 && now - lastTapMs < 2500.0 && now - lastTapMs > 100.0)
        {
            tapIntervals.add (now - lastTapMs);
            while (tapIntervals.size() > 4)
                tapIntervals.remove (0);

            double avg = 0.0;
            for (auto iv : tapIntervals) avg += iv;
            avg /= tapIntervals.size();

            const float bpm = juce::jlimit (30.0f, 300.0f, (float) (60000.0 / avg));
            if (auto* param = processorRef.apvts.getParameter ("metBpm"))
                param->setValueNotifyingHost (param->convertTo0to1 (bpm));
        }
        else
        {
            tapIntervals.clear();
        }
        lastTapMs = now;
    }

    void updatePlayButton()
    {
        playButton.setButtonText (processorRef.isMetronomePlaying() ? "STOP" : "START");
    }

    void timerCallback() override
    {
        auto& met = processorRef.getMetronome();

        // Keep the button in sync (e.g. after a mode switch) and repaint the
        // dynamic parts only when something actually changed.
        if (playButton.getToggleState() != met.isPlaying())
        {
            playButton.setToggleState (met.isPlaying(), juce::dontSendNotification);
            updatePlayButton();
        }
        if (playButton.getButtonText() != (met.isPlaying() ? juce::String ("STOP") : juce::String ("START")))
            updatePlayButton();

        const int   beat = met.getDisplayBeat();
        const float bpm  = met.getDisplayBpm();
        const bool  mute = met.isBarMuted();
        if (beat != lastBeat || bpm != lastBpm || mute != lastMuted)
        {
            lastBeat = beat; lastBpm = bpm; lastMuted = mute;
            repaint();
        }
    }

    //==========================================================================
    LiveDspProcessor& processorRef;

    juce::TextButton playButton { "START" };
    juce::TextButton tapButton  { "TAP" };

    juce::Slider bpmSlider;
    std::unique_ptr<KnobControl> volKnob;

    std::unique_ptr<Stepper> beatsStep, trIncStep, trBarsStep, gapPlayStep, gapMuteStep;
    juce::ComboBox subdivBox, soundBox;
    juce::TextButton accentButton  { "ACCENT" };
    juce::TextButton trainerButton { "TRAINER" };
    juce::TextButton gapButton     { "GAP" };

    juce::Label lTempo, lBeats, lSubdiv, lSound, lTrInc, lTrBars, lGapPlay, lGapMute;

    std::unique_ptr<APVTS::SliderAttachment>   bpmAttach;
    std::unique_ptr<APVTS::ComboBoxAttachment> subdivAttach, soundAttach;
    std::unique_ptr<APVTS::ButtonAttachment>   accentAttach, trainerAttach, gapAttach;

    juce::Rectangle<int> bpmRect, dotsRect, mainSep;
    std::vector<juce::Rectangle<int>> groupSeps;

    juce::Array<double> tapIntervals;
    double lastTapMs { 0.0 };

    int   lastBeat  { -1 };
    float lastBpm   { 0.0f };
    bool  lastMuted { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetronomeComponent)
};
