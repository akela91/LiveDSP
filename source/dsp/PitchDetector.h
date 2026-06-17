#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>

/**
    Egyszerű, robusztus hangmagasság-detektor a hangolóhoz (autokorreláció +
    "clarity" küszöb + parabolikus interpoláció).

    NEM a hangszálon fut — az üzenetszálról (Timer) hívjuk egy másolt
    bemeneti pufferre. Egyszálú használatra tervezve.

    Tartomány alapból ~40–600 Hz (lehangolt gitár mély húrjaitól a magas fekvésig).
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

    // A samples a legfrissebb monó bemeneti ablak (időrendben).
    Result detect (const float* samples, int numSamples)
    {
        Result result;
        if (numSamples < 256 || sampleRate <= 0.0)
            return result;

        const int minLag = juce::jmax (2, (int) std::floor (sampleRate / maxFreq));
        const int maxLag = juce::jmin (numSamples - 1, (int) std::ceil (sampleRate / minFreq));
        if (maxLag <= minLag)
            return result;

        // DC eltávolítás + jelenergia (gate a csendre).
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

        // Túl halk -> nincs detektálás (kb. -50 dBFS RMS alatt).
        if (energy / numSamples < 1.0e-5)
            return result;

        // Normalizált autokorreláció (NSDF-szerű) a lyukak/oktávtévesztés ellen.
        // r(lag) = 2*sum(x[i]*x[i+lag]) / (sum(x[i]^2)+sum(x[i+lag]^2))
        float bestValue = 0.0f;
        int   bestLag   = -1;

        float prev = 0.0f;
        for (int lag = minLag; lag <= maxLag; ++lag)
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
            const float nsdf  = denom > 0.0f ? (float) (2.0 * ac) / denom : 0.0f;

            // Lokális csúcs megkeresése a küszöb felett (a legerősebbet tartjuk meg).
            if (nsdf < prev && prev > bestValue && prev > clarityThreshold)
            {
                bestValue = prev;
                bestLag   = lag - 1;
            }
            prev = nsdf;
        }

        if (bestLag < minLag)
            return result;

        // Parabolikus interpoláció a csúcs körül (finomabb frekvencia).
        const float y0 = nsdfAt (bestLag - 1, numSamples);
        const float y1 = nsdfAt (bestLag,     numSamples);
        const float y2 = nsdfAt (bestLag + 1, numSamples);
        float refinedLag = (float) bestLag;
        const float denom2 = (y0 - 2.0f * y1 + y2);
        if (std::abs (denom2) > 1.0e-9f)
            refinedLag += 0.5f * (y0 - y2) / denom2;

        result.frequency = (float) (sampleRate / refinedLag);
        result.clarity   = juce::jlimit (0.0f, 1.0f, bestValue);
        return result;
    }

private:
    // NSDF egy adott lag-re (a parabolikus finomításhoz; work[] már elő van készítve).
    float nsdfAt (int lag, int numSamples)
    {
        if (lag < 1 || lag >= numSamples)
            return 0.0f;
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
        return denom > 0.0f ? (float) (2.0 * ac) / denom : 0.0f;
    }

    double sampleRate { 48000.0 };
    float  minFreq    { 40.0f };
    float  maxFreq    { 600.0f };
    float  clarityThreshold { 0.6f };

    std::vector<float> work;
};
