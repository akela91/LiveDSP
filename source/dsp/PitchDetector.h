#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

/**
    Hangmagasság-detektor a hangolóhoz — McLeod Pitch Method (MPM) alapú.

    Normalizált négyzetes differencia függvény (NSDF) + "key maxima"
    csúcsválasztás: a globális csúcs k-szorosát (0.9) elérő ELSŐ csúcsot
    választja. Ez stabil, és elkerüli az oktáv-tévesztést (magas hangoknál is).

    NEM a hangszálon fut — az üzenetszálról (Timer) hívjuk egy másolt
    bemeneti pufferre.
*/
class PitchDetector
{
public:
    struct Result
    {
        float frequency { 0.0f };  // Hz, 0 ha nincs megbízható detektálás
        float clarity   { 0.0f };  // 0..1 megbízhatóság
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

        // DC eltávolítás + energia (csend-gate).
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
        if (energy / numSamples < 1.0e-5)   // ~ -50 dBFS alatt nincs detektálás
            return result;

        // NSDF kiszámítása 0..maxLag-ig.
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

        // "Key maxima": minden pozitív-meredekségű nullátmenet utáni első
        // lokális maximumot összegyűjtjük (a minLag-tól).
        float globalMax = 0.0f;
        int   chosenLag = -1;

        // 1) globális csúcs a key maximák között
        std::vector<int> keyLags;
        {
            int lag = minLag;
            // előrelépés az első pozitív nullátmenetig
            while (lag < maxLag && nsdf[(size_t) lag] > 0.0f) ++lag;
            while (lag < maxLag)
            {
                if (nsdf[(size_t) lag] > 0.0f && nsdf[(size_t) (lag - 1)] <= 0.0f)
                {
                    // pozitív zónába léptünk -> keressük a lokális maximumot
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

        // 2) az első key maximum, amely eléri a globális csúcs k-szorosát.
        const float threshold = 0.9f * globalMax;
        for (int kl : keyLags)
            if (nsdf[(size_t) kl] >= threshold) { chosenLag = kl; break; }

        if (chosenLag < minLag)
            return result;

        // 3) parabolikus interpoláció a választott csúcs körül.
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
    float  maxFreq    { 1200.0f };   // magas fekvésig
    float  clarityThreshold { 0.7f };

    std::vector<float> work;
    std::vector<float> nsdf;
};
