#pragma once

#include <JuceHeader.h>
#include <cmath>

/**
    Fast, responsive noise gate (envelope follower + threshold).

    Operates on a mono signal. The attack/release are set in ms, the threshold in dB.
    The hysteresis (open/close threshold difference) prevents "chattering".

    RT-safe: no allocation in process().
*/
class NoiseGate
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        updateCoefficients();
        envelope = 0.0f;
        gain = 0.0f;
    }

    void reset() noexcept
    {
        envelope = 0.0f;
        gain = 0.0f;
    }

    // Threshold given in dB (e.g. -60 .. -20)
    void setThreshold (float thresholdDb) noexcept
    {
        openThreshold  = juce::Decibels::decibelsToGain (thresholdDb);
        closeThreshold = juce::Decibels::decibelsToGain (thresholdDb - hysteresisDb);
    }

    void setAttack (float attackMs) noexcept   { attackTimeMs  = attackMs;  updateCoefficients(); }
    void setRelease (float releaseMs) noexcept { releaseTimeMs = releaseMs; updateCoefficients(); }
    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

    // The gate's current gain (0 = fully closed/muting, 1 = open) — for the UI
    // LED indicator (to be read from the message thread, atomically). If the gate
    // is disabled, returns 1.0 (no muting).
    float getCurrentGain() const noexcept { return enabled ? currentGain.load() : 1.0f; }

    // Mono, in-place processing.
    void process (float* samples, int numSamples) noexcept
    {
        if (! enabled)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const float in = samples[i];

            // Simple envelope follower (peak detector, fast attack / slow release).
            const float rectified = std::abs (in);
            if (rectified > envelope)
                envelope = rectified;                              // follows the peak instantly
            else
                envelope = envelope * envelopeDecay;               // slow decay

            // Determine the target gain with hysteresis.
            float targetGain;
            if (envelope > openThreshold)        targetGain = 1.0f;
            else if (envelope < closeThreshold)  targetGain = 0.0f;
            else                                 targetGain = gain; // hold within the band

            // Smooth transition (attack when opening, release when closing).
            const float coeff = (targetGain > gain) ? attackCoeff : releaseCoeff;
            gain = targetGain + (gain - targetGain) * coeff;

            samples[i] = in * gain;
        }

        currentGain.store (gain);   // end-of-block state for the UI LED
    }

private:
    void updateCoefficients() noexcept
    {
        attackCoeff   = std::exp (-1.0f / (0.001f * attackTimeMs  * (float) sampleRate));
        releaseCoeff  = std::exp (-1.0f / (0.001f * releaseTimeMs * (float) sampleRate));
        // Envelope decay ~ 50 ms window
        envelopeDecay = std::exp (-1.0f / (0.050f * (float) sampleRate));
    }

    double sampleRate { 48000.0 };
    bool   enabled    { true };

    float attackTimeMs  { 1.0f };
    float releaseTimeMs { 80.0f };
    float hysteresisDb  { 3.0f };

    float openThreshold  { juce::Decibels::decibelsToGain (-55.0f) };
    float closeThreshold { juce::Decibels::decibelsToGain (-58.0f) };

    float attackCoeff   { 0.0f };
    float releaseCoeff  { 0.0f };
    float envelopeDecay { 0.0f };

    float envelope { 0.0f };
    float gain     { 0.0f };

    std::atomic<float> currentGain { 1.0f };   // for the UI LED (end-of-block gain)
};
