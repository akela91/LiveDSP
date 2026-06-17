#pragma once

#include <JuceHeader.h>
#include <cmath>

/**
    Tube Screamer stílusú overdrive/booster.

    Jelút a stompbox-on belül:
        [highpass ~720 Hz] -> [drive gain] -> [aszimmetrikus soft-clip (tanh)]
        -> [lowpass / tone] -> [level]

    A highpass adja a TS jellegzetes "mid-hump" alapját (a basszust levágja a
    klippelés előtt), a lowpass a tone-t. Mono, in-place.

    RT-safe: nincs allokáció a process()-ben.
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

    // 0..1: a tone-szűrő levágási frekvenciáját skálázza (sötét..fényes).
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

            x = preFilter.processSample (x);          // basszus levágás klippelés előtt
            x *= driveGain.getNextValue();

            // Aszimmetrikus soft clipping (páros felharmonikusokért).
            x = std::tanh (x) + 0.05f * std::tanh (x * 2.0f);

            x = toneFilter.processSample (x);          // tone
            x *= level.getNextValue() * 0.5f;          // kimeneti szint kompenzáció

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
