#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <cmath>

/**
    Low-latency granular (two-tap crossfade) pitch shifter — a time-domain shifter
    that adds far less latency than an STFT/phase-vocoder engine (RB Live), at the
    cost of some "granular" colouration that has to be tuned via the grain size.

    How it works (exactly the classic delay-line modulation method):
      - One circular buffer is written at 1 sample/sample (the write pointer).
      - TWO read pointers chase the write pointer. Their read position is
            read = writePos - (delayBase + phase * grain)
        where 'phase' is a normalised ramp in [0,1). The read pointer therefore
        advances at rate r = 2^(semitones/12) per output sample (drp/dn = r), so
        r < 1 pitches DOWN and r > 1 pitches UP — without changing duration.
      - Because a read pointer would otherwise drift away forever, 'phase' wraps
        every grain; at the wrap the pointer jumps by one grain. The two pointers
        are offset by half a grain (phase + 0.5) and each is weighted by a Hann
        window. Two Hann windows offset by 0.5 sum to exactly 1.0, and both reach
        0 at the wrap — so the jump (the "splice") is completely cross-faded out.

    Latency is ~ delayBase + grain/2 (the window energy sits mid-grain), i.e. a
    20 ms grain is ~10 ms of latency — vs RB Live's ~50 ms. Smaller grains = lower
    latency but more warble/comb colouration, so 'grain' is the main tuning knob.
    Reads use 4-point Hermite interpolation for the fractional positions.

    prepare/reset run on the MESSAGE THREAD; the setters/process are real-time safe
    (atomics + a pre-allocated buffer, no locking, no allocation in process()).
*/
class GranularPitchShifter
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate   = spec.sampleRate;
        maxBlockSize = (int) spec.maximumBlockSize;

        buffer.assign ((size_t) bufferSize, 0.0f);
        writePos = 0;
        phase    = 0.0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        phase    = 0.0;
    }

    void setSemitones (float s) noexcept
    {
        ratio.store (std::pow (2.0f, juce::jlimit (-12.0f, 12.0f, s) / 12.0f));
    }

    // Grain size in milliseconds (the main quality/latency lever).
    void setGrainMs (float ms) noexcept
    {
        const int g = juce::jlimit (64, maxGrain,
                                    (int) std::lround (ms * sampleRate / 1000.0));
        grainSamples.store (g);
        latency.store (delayBase + g / 2);
    }

    int getLatencySamples() const noexcept { return latency.load(); }

    // Mono, in-place processing.
    void process (float* x, int numSamples) noexcept
    {
        const double r   = (double) ratio.load();
        const double G   = (double) grainSamples.load();
        const double inc = (1.0 - r) / G;                 // phase step per sample
        const double twoPi = juce::MathConstants<double>::twoPi;

        for (int i = 0; i < numSamples; ++i)
        {
            buffer[(size_t) (writePos & mask)] = x[i];

            double ph1 = phase;
            double ph2 = phase + 0.5;
            if (ph2 >= 1.0) ph2 -= 1.0;

            const double d1 = (double) delayBase + ph1 * G;
            const double d2 = (double) delayBase + ph2 * G;

            const float s1 = readHermite ((double) writePos - d1);
            const float s2 = readHermite ((double) writePos - d2);

            // Hann windows offset by half a grain -> sum to 1, zero at the splice.
            const float w1 = 0.5f * (1.0f - (float) std::cos (twoPi * ph1));
            const float w2 = 0.5f * (1.0f - (float) std::cos (twoPi * ph2));

            x[i] = w1 * s1 + w2 * s2;

            ++writePos;
            phase += inc;
            if      (phase >= 1.0) phase -= 1.0;
            else if (phase <  0.0) phase += 1.0;
        }
    }

private:
    // 4-point (cubic) Hermite interpolation at a fractional read position.
    float readHermite (double rp) const noexcept
    {
        const long long i0 = (long long) std::floor (rp);
        const float f = (float) (rp - (double) i0);
        const float xm1 = buffer[(size_t) ((i0 - 1) & mask)];
        const float x0  = buffer[(size_t) ( i0      & mask)];
        const float x1  = buffer[(size_t) ((i0 + 1) & mask)];
        const float x2  = buffer[(size_t) ((i0 + 2) & mask)];

        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float w = c + v;
        const float a = w + v + (x2 - x0) * 0.5f;
        const float b = w + a;
        return ((a * f - b) * f + c) * f + x0;
    }

    static constexpr int       bufferSize = 1 << 15;       // 32768 samples (~0.68 s)
    static constexpr long long mask       = bufferSize - 1;
    static constexpr int       maxGrain   = 8192;          // upper bound on grain
    static constexpr int       delayBase  = 4;             // min read delay (Hermite needs +2)

    double sampleRate   { 48000.0 };
    int    maxBlockSize { 512 };

    std::vector<float> buffer;
    long long writePos { 0 };
    double    phase    { 0.0 };

    std::atomic<float> ratio        { 1.0f };
    std::atomic<int>   grainSamples { 1152 };   // ~24 ms @ 48 kHz
    std::atomic<int>   latency      { 0 };
};
