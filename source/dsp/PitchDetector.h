#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

/**
    Pitch detector for the tuner — based on the McLeod Pitch Method (MPM).

    Normalized square difference function (NSDF) + "key maxima"
    peak picking: selects the FIRST peak that reaches k times (0.9) the
    global peak. This is stable, and avoids octave errors (even for high notes).

    Does NOT run on the audio thread — it is called from the message thread (Timer)
    on a copied input buffer.
*/
class PitchDetector
{
public:
    struct Result
    {
        float frequency { 0.0f };  // Hz, 0 if there is no reliable detection
        float clarity   { 0.0f };  // 0..1 reliability
    };

    void setSampleRate (double sr) noexcept { sampleRate = sr; }
    void setRange (float minHz, float maxHz) noexcept { minFreq = minHz; maxFreq = maxHz; }

    Result detect (const float* samples, int numSamples)
    {
        Result result;
        if (numSamples < 256 || sampleRate <= 0.0)
            return result;

        const int maxLag = juce::jmin (numSamples - 1, (int) std::ceil (sampleRate / minFreq));
        const int minLag = juce::jmax (2, (int) std::floor (sampleRate / maxFreq));
        if (maxLag <= minLag)
            return result;

        // DC removal + energy (silence gate).
        double mean = 0.0;
        for (int i = 0; i < numSamples; ++i)
            mean += samples[i];
        mean /= numSamples;

        work.resize ((size_t) numSamples);
        double energy = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const float v = (float) (samples[i] - mean);
            work[(size_t) i] = v;
            energy += (double) v * v;
        }
        if (energy / numSamples < 1.0e-5)   // no detection below ~ -50 dBFS
            return result;

        // Compute the NSDF from 0..maxLag.
        nsdf.assign ((size_t) (maxLag + 1), 0.0f);
        for (int lag = 0; lag <= maxLag; ++lag)
        {
            double ac = 0.0, e0 = 0.0, e1 = 0.0;
            const int n = numSamples - lag;
            for (int i = 0; i < n; ++i)
            {
                const float a = work[(size_t) i];
                const float b = work[(size_t) (i + lag)];
                ac += (double) a * b;
                e0 += (double) a * a;
                e1 += (double) b * b;
            }
            const float denom = (float) (e0 + e1);
            nsdf[(size_t) lag] = denom > 0.0f ? (float) (2.0 * ac) / denom : 0.0f;
        }

        // "Key maxima": collect the first local maximum after each
        // positive-slope zero crossing (starting from minLag).
        float globalMax = 0.0f;
        int   chosenLag = -1;

        // 1) global peak among the key maxima
        std::vector<int> keyLags;
        {
            int lag = minLag;
            // advance to the first positive zero crossing
            while (lag < maxLag && nsdf[(size_t) lag] > 0.0f) ++lag;
            while (lag < maxLag)
            {
                if (nsdf[(size_t) lag] > 0.0f && nsdf[(size_t) (lag - 1)] <= 0.0f)
                {
                    // entered a positive zone -> look for the local maximum
                    int   maxPos = lag;
                    float maxVal = nsdf[(size_t) lag];
                    while (lag < maxLag && nsdf[(size_t) lag] > 0.0f)
                    {
                        if (nsdf[(size_t) lag] > maxVal) { maxVal = nsdf[(size_t) lag]; maxPos = lag; }
                        ++lag;
                    }
                    keyLags.push_back (maxPos);
                    globalMax = juce::jmax (globalMax, maxVal);
                }
                else ++lag;
            }
        }

        if (keyLags.empty() || globalMax <= clarityThreshold)
            return result;

        // 2) the first key maximum that reaches k times the global peak.
        const float threshold = 0.9f * globalMax;
        for (int kl : keyLags)
            if (nsdf[(size_t) kl] >= threshold) { chosenLag = kl; break; }

        if (chosenLag < minLag)
            return result;

        // 3) parabolic interpolation around the chosen peak.
        const float y0 = chosenLag > 0 ? nsdf[(size_t) (chosenLag - 1)] : nsdf[(size_t) chosenLag];
        const float y1 = nsdf[(size_t) chosenLag];
        const float y2 = chosenLag + 1 <= maxLag ? nsdf[(size_t) (chosenLag + 1)] : nsdf[(size_t) chosenLag];
        float refinedLag = (float) chosenLag;
        const float denom2 = (y0 - 2.0f * y1 + y2);
        if (std::abs (denom2) > 1.0e-9f)
            refinedLag += 0.5f * (y0 - y2) / denom2;

        result.frequency = (float) (sampleRate / refinedLag);
        result.clarity   = juce::jlimit (0.0f, 1.0f, globalMax);
        return result;
    }

private:
    double sampleRate { 48000.0 };
    float  minFreq    { 40.0f };
    float  maxFreq    { 1200.0f };   // up to high register
    float  clarityThreshold { 0.7f };

    std::vector<float> work;
    std::vector<float> nsdf;
};
