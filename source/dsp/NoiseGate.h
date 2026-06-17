#pragma once

#include <JuceHeader.h>
#include <cmath>

/**
    Gyors, válaszkész zajkapu (envelope follower + küszöb).

    Mono jelre dolgozik. Az attack/release ms-ben állítható, a küszöb dB-ben.
    A hysteresis (open/close küszöb különbség) megakadályozza a "chattering"-et.

    RT-safe: nincs allokáció a process()-ben.
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

    // dB-ben adott küszöb (pl. -60 .. -20)
    void setThreshold (float thresholdDb) noexcept
    {
        openThreshold  = juce::Decibels::decibelsToGain (thresholdDb);
        closeThreshold = juce::Decibels::decibelsToGain (thresholdDb - hysteresisDb);
    }

    void setAttack (float attackMs) noexcept   { attackTimeMs  = attackMs;  updateCoefficients(); }
    void setRelease (float releaseMs) noexcept { releaseTimeMs = releaseMs; updateCoefficients(); }
    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

    // A kapu aktuális gain-je (0 = teljesen zárva/némít, 1 = nyitva) — a UI
    // LED-jelzéshez (üzenetszálról olvasandó, atomikusan). Ha a kapu ki van
    // kapcsolva, 1.0-t ad (nincs némítás).
    float getCurrentGain() const noexcept { return enabled ? currentGain.load() : 1.0f; }

    // Mono, in-place feldolgozás.
    void process (float* samples, int numSamples) noexcept
    {
        if (! enabled)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const float in = samples[i];

            // Egyszerű envelope follower (peak detektor, gyors fel / lassú le).
            const float rectified = std::abs (in);
            if (rectified > envelope)
                envelope = rectified;                              // azonnal követi a csúcsot
            else
                envelope = envelope * envelopeDecay;               // lassú lecsengés

            // Cél-gain meghatározása hysteresissel.
            float targetGain;
            if (envelope > openThreshold)        targetGain = 1.0f;
            else if (envelope < closeThreshold)  targetGain = 0.0f;
            else                                 targetGain = gain; // tartás a sávban

            // Sima átmenet (attack a nyitásnál, release a zárásnál).
            const float coeff = (targetGain > gain) ? attackCoeff : releaseCoeff;
            gain = targetGain + (gain - targetGain) * coeff;

            samples[i] = in * gain;
        }

        currentGain.store (gain);   // blokk végi állapot a UI LED-hez
    }

private:
    void updateCoefficients() noexcept
    {
        attackCoeff   = std::exp (-1.0f / (0.001f * attackTimeMs  * (float) sampleRate));
        releaseCoeff  = std::exp (-1.0f / (0.001f * releaseTimeMs * (float) sampleRate));
        // Envelope lecsengés ~ 50 ms ablak
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

    std::atomic<float> currentGain { 1.0f };   // UI LED-hez (blokk végi gain)
};
