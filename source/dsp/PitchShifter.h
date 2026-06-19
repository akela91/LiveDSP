#pragma once

#include <JuceHeader.h>
#include <atomic>

// Our own granular (two-tap crossfade) time-domain shifter — the GuitarDSP
// Transpose runs on this for its very low latency.
#include "GranularPitchShifter.h"

/**
    Real-time, low-latency Transpose (pitch shifter) for the guitar chain, built on
    the custom GranularPitchShifter — a two-tap crossfade time-domain shifter. It
    adds only ~grain/2 of latency (a 24 ms grain ≈ ~12 ms), a fraction of what an
    STFT/phase-vocoder shifter needs, and is tuned via the live GRAIN knob.

    For drop tuning: -12 .. +12 semitones (without time stretching, at a 1:1 ratio).
    Operates on a mono signal (the guitar signal chain is mono up to the NAM).

    prepare/reset run on the MESSAGE THREAD (audio stopped); the setters and
    process() are real-time safe. getLatencySamples() returns the cached latency.
*/
class PitchShifter
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec) { granular.prepare (spec); }
    void reset()                                       { granular.reset(); }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled.store (shouldBeEnabled); }

    // Stores the shift; process() bypasses entirely at 0 semitones (zero latency).
    void setSemitones (float semitones) noexcept
    {
        const float s = juce::jlimit (-12.0f, 12.0f, semitones);
        semitonesValue.store (s);
        granular.setSemitones (s);
    }

    // Grain size in ms — the quality/latency lever (smaller = lower latency).
    void setGrainMs (float ms) noexcept { granular.setGrainMs (ms); }

    int getLatencySamples() const noexcept { return granular.getLatencySamples(); }

    // Mono, in-place processing.
    void process (float* samples, int numSamples) noexcept
    {
        // Bypass when off or at unity shift (the granular path is not a clean
        // pass-through at ratio 1.0, so we skip it to keep zero added latency).
        if (! enabled.load() || semitonesValue.load() == 0.0f)
            return;

        granular.process (samples, numSamples);
    }

private:
    GranularPitchShifter granular;
    std::atomic<bool>  enabled        { false };
    std::atomic<float> semitonesValue { 0.0f };
};
