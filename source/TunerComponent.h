#pragma once

#include <JuceHeader.h>
#include <climits>
#include "PluginProcessor.h"
#include "dsp/PitchDetector.h"

/**
    Vizuális gitárhangoló. A processzor nyers bemenetét pollozza (Timer), és
    autokorrelációval detektálja a hangmagasságot. Kijelzi a legközelebbi hangot
    és a cent-eltérést egy mutatóval (zöld = hangban).

    A detektálás az üzenetszálon fut (a Timer callbackben), így nem terheli a
    hangszálat.
*/
class TunerComponent : public juce::Component,
                       private juce::Timer
{
public:
    explicit TunerComponent (LiveDspProcessor& p) : processorRef (p)
    {
        snapshot.resize ((size_t) snapshotSize, 0.0f);
        detector.setRange (40.0f, 1000.0f);
        startTimerHz (20);
    }

    ~TunerComponent() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced (10);

        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillRoundedRectangle (area.toFloat(), 6.0f);

        const bool inTune = hasPitch && std::abs (cents) < 5.0f;

        // Hang neve
        auto top = area.removeFromTop (area.getHeight() / 2);
        g.setColour (hasPitch ? (inTune ? juce::Colours::limegreen : juce::Colours::white)
                              : juce::Colours::grey);
        g.setFont (juce::Font (juce::FontOptions (40.0f, juce::Font::bold)));
        g.drawText (hasPitch ? noteName : juce::String ("--"),
                    top, juce::Justification::centred);

        // Frekvencia + cent szöveg
        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        if (hasPitch)
            g.drawText (juce::String (frequency, 1) + " Hz   "
                          + (cents >= 0 ? "+" : "") + juce::String (cents, 0) + " cent",
                        top.removeFromBottom (16), juce::Justification::centred);

        // Cent-mutató sáv
        auto bar = area.reduced (4);
        const float midX = bar.getCentreX();

        g.setColour (juce::Colour (0xff333333));
        g.fillRect (bar.withSizeKeepingCentre (bar.getWidth(), 6));

        // Középvonal (hangban)
        g.setColour (juce::Colours::limegreen.withAlpha (0.6f));
        g.fillRect ((int) midX - 1, bar.getY(), 2, bar.getHeight());

        // Skála ±50 cent
        if (hasPitch)
        {
            const float norm = juce::jlimit (-1.0f, 1.0f, cents / 50.0f);
            const float needleX = midX + norm * (bar.getWidth() * 0.5f);
            g.setColour (inTune ? juce::Colours::limegreen : juce::Colours::orange);
            g.fillRoundedRectangle (needleX - 4.0f, (float) bar.getY(), 8.0f,
                                    (float) bar.getHeight(), 3.0f);
        }
    }

private:
    void timerCallback() override
    {
        processorRef.copyRecentInput (snapshot.data(), snapshotSize);
        detector.setSampleRate (processorRef.getSampleRate());

        const auto r = detector.detect (snapshot.data(), snapshotSize);

        if (r.frequency > 0.0f && r.clarity >= minClarity)
        {
            silenceCount = 0;

            // Sima frekvencia-követés (octave-ugrás után gyorsabb újrakövetés).
            const bool jumped = smoothedFreq > 0.0f
                && (r.frequency > 1.6f * smoothedFreq || r.frequency < 0.6f * smoothedFreq);
            if (smoothedFreq <= 0.0f || jumped)
                smoothedFreq = r.frequency;
            else
                smoothedFreq = 0.85f * smoothedFreq + 0.15f * r.frequency;
            frequency = smoothedFreq;

            const float midiF   = 69.0f + 12.0f * std::log2 (frequency / 440.0f);
            const int   nearest = (int) std::lround (midiF);

            // HANG-STABILIZÁLÁS: csak több egymást követő stabil keret után váltunk
            // hangot (megszünteti a magas hangok ugrálását).
            if (nearest == pendingNote)
            {
                if (++pendingCount >= stableFramesNeeded || displayedNote == INT_MIN)
                    displayedNote = nearest;
            }
            else
            {
                pendingNote  = nearest;
                pendingCount = 1;
                if (displayedNote == INT_MIN)
                    displayedNote = nearest;
            }

            // A cent-eltérés a STABIL hanghoz képest.
            cents    = (midiF - (float) displayedNote) * 100.0f;
            noteName = midiNoteName (displayedNote);
            hasPitch = true;
        }
        else if (++silenceCount > 10)   // pár keret tartás, majd törlés
        {
            hasPitch     = false;
            smoothedFreq = 0.0f;
            displayedNote = INT_MIN;
            pendingNote   = INT_MIN;
            pendingCount  = 0;
        }

        repaint();
    }

    static juce::String midiNoteName (int midiNote)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F",
                                       "F#", "G", "G#", "A", "A#", "B" };
        int idx = midiNote % 12;
        if (idx < 0) idx += 12;
        const int octave = midiNote / 12 - 1;
        return juce::String (names[idx]) + juce::String (octave);
    }

    LiveDspProcessor& processorRef;
    PitchDetector detector;

    static constexpr int snapshotSize = 4096;
    std::vector<float> snapshot;

    bool   hasPitch     { false };
    float  frequency    { 0.0f };
    float  smoothedFreq { 0.0f };
    float  cents        { 0.0f };
    juce::String noteName { "--" };

    // Hang-stabilizálás
    static constexpr int stableFramesNeeded = 2;
    static constexpr float minClarity = 0.8f;
    int displayedNote { INT_MIN };
    int pendingNote   { INT_MIN };
    int pendingCount  { 0 };
    int silenceCount  { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerComponent)
};
