#pragma once

#include <JuceHeader.h>
#include <array>

/**
    9-band graphic EQ at octave frequencies:
        65, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 Hz

    Low latency: IIR biquad peak filters (ZERO extra latency, unlike a
    linear-phase FFT EQ). Stereo (ProcessorDuplicator per band).
    Runs after NAM/Cab, before the Delay.

    Coefficients are updated only for bands that changed (not all of them
    every block).
*/
class Equalizer
{
public:
    static constexpr int numBands = 9;
    static constexpr std::array<float, numBands> frequencies
        { 65.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        for (auto& b : bands)
            b.prepare (spec);

        for (auto& g : lastGains)
            g = 1.0e9f;     // forced update
        updateCoefficients();
    }

    void reset() noexcept
    {
        for (auto& b : bands)
            b.reset();
    }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

    // numBands gain values in dB (-15..+15).
    void setGains (const float* gainsDb) noexcept
    {
        for (int i = 0; i < numBands; ++i)
            gains[(size_t) i] = gainsDb[i];
        updateCoefficients();
    }

    void process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        if (! enabled)
            return;

        juce::dsp::ProcessContextReplacing<float> ctx (block);
        for (auto& b : bands)
            b.process (ctx);
    }

private:
    void updateCoefficients()
    {
        if (sampleRate <= 0.0)
            return;

        // For an octave band Q ~ 1.41 (about 1 octave bandwidth).
        constexpr float Q = 1.41f;

        for (int i = 0; i < numBands; ++i)
        {
            if (juce::approximatelyEqual (gains[(size_t) i], lastGains[(size_t) i]))
                continue;

            const float freq = juce::jmin (frequencies[(size_t) i], (float) (sampleRate * 0.45));
            *bands[(size_t) i].state =
                *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                    sampleRate, freq, Q, juce::Decibels::decibelsToGain (gains[(size_t) i]));

            lastGains[(size_t) i] = gains[(size_t) i];
        }
    }

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefs  = juce::dsp::IIR::Coefficients<float>;
    using Band   = juce::dsp::ProcessorDuplicator<Filter, Coefs>;

    std::array<Band, numBands>  bands;
    std::array<float, numBands> gains     { };          // 0 dB
    std::array<float, numBands> lastGains { };

    double sampleRate { 48000.0 };
    bool   enabled    { false };
};
