#pragma once

#include <JuceHeader.h>
#include <cmath>

/**
    Tube Screamer style overdrive/booster.

    Signal chain inside the stompbox:
        [highpass ~720 Hz] -> [drive gain] -> [asymmetric soft-clip (tanh)]
        -> [lowpass / tone] -> [level]

    The highpass provides the basis of the characteristic TS "mid-hump" (cutting
    the bass before clipping), the lowpass provides the tone. Mono, in-place.

    RT-safe: no allocation in process().
*/
class Overdrive
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        auto monoSpec = spec;
        monoSpec.numChannels = 1;

        preFilter.prepare (monoSpec);
        toneFilter.prepare (monoSpec);
        updateFilters();

        driveGain.reset (sampleRate, 0.02);
        level.reset (sampleRate, 0.02);
    }

    void reset() noexcept
    {
        preFilter.reset();
        toneFilter.reset();
    }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }
    void setDrive (float driveDb) noexcept   { driveGain.setTargetValue (juce::Decibels::decibelsToGain (driveDb)); }
    void setLevel (float levelDb) noexcept   { level.setTargetValue (juce::Decibels::decibelsToGain (levelDb)); }

    // 0..1: scales the tone filter's cutoff frequency (dark..bright).
    void setTone (float toneNorm) noexcept
    {
        toneCutoff = juce::jmap (juce::jlimit (0.0f, 1.0f, toneNorm), 800.0f, 6000.0f);
        updateFilters();
    }

    void process (float* samples, int numSamples) noexcept
    {
        if (! enabled)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            float x = samples[i];

            x = preFilter.processSample (x);          // bass cut before clipping
            x *= driveGain.getNextValue();

            // Asymmetric soft clipping (for even harmonics).
            x = std::tanh (x) + 0.05f * std::tanh (x * 2.0f);

            x = toneFilter.processSample (x);          // tone
            x *= level.getNextValue() * 0.5f;          // output level compensation

            samples[i] = x;
        }
    }

private:
    void updateFilters()
    {
        *preFilter.coefficients =
            *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 720.0f);
        *toneFilter.coefficients =
            *juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, toneCutoff);
    }

    double sampleRate { 48000.0 };
    bool   enabled    { false };

    float toneCutoff { 3000.0f };

    juce::dsp::IIR::Filter<float> preFilter;
    juce::dsp::IIR::Filter<float> toneFilter;

    juce::SmoothedValue<float> driveGain { 1.0f };
    juce::SmoothedValue<float> level     { 1.0f };
};
