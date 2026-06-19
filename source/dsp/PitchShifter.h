#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <cmath>
#include <memory>

// Rubber Band Library (GPL/commercial). The single-file build's static lib
// (single/RubberBandSingle.cpp) provides RubberBandLiveShifter (v4): the
// lowest-latency live pitch shifter — for live monitoring.
#include <rubberband/RubberBandLiveShifter.h>

/**
    Real-time, low-latency Transpose (pitch shifter) built on the Rubber Band
    LiveShifter (v4): fixed block size (getBlockSize) shift(), designed for the
    lowest latency — for live monitoring.

    The engine is selectable in the UI for FUTURE alternatives, but currently only
    "RB Live" exists. Its quality/latency profile has two presets:
      - Fast: short window for the lowest latency (the best overall).
      - Fine: medium window + formant preservation (better detuned timbre).

    For drop tuning: -12 .. +12 semitones (without time stretching, at a 1:1 ratio).
    Operates on a mono signal (the guitar signal chain is mono up to the NAM).

    reconfigureLive/reset/prepare must be called from the MESSAGE THREAD (may allocate);
    process() uses a tryLock on the audio thread (skips 1 block at the moment of the swap).
    getLatencySamples() returns the engine's atomically cached latency.
*/
class PitchShifter
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate   = spec.sampleRate;
        maxBlockSize = (int) spec.maximumBlockSize;

        const juce::SpinLock::ScopedLockType sl (lock);
        configureLiveShifter();
    }

    void reset()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        if (liveShifter != nullptr)
        {
            liveShifter->reset();
            lsIn.clear();
            lsOut.clear();
        }
    }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

    // RB Live quality/latency profile: 0 = Fast (short window, lowest latency),
    // 1 = Fine (medium window + formant preservation, better timbre, slightly more latency).
    void setLiveQuality (int q) noexcept { liveQuality.store (q); }

    // RB Live profile switch — call from the MESSAGE THREAD (rebuilds the shifter + FIFO).
    void reconfigureLive()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        configureLiveShifter();
    }

    // Only stores; the transpose is applied by process() under the lock (thread-safe).
    void setSemitones (float semitones) noexcept
    {
        currentSemitones.store (juce::jlimit (-12.0f, 12.0f, semitones));
    }

    int getLatencySamples() const noexcept { return lsLatency.load(); }

    // Mono, in-place processing (numSamples <= maxBlockSize).
    void process (float* samples, int numSamples) noexcept
    {
        if (! enabled || currentSemitones.load() == 0.0f)
            return;

        // tryLock: if a reconfigure/reset is in progress (message thread), skip the block.
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || liveShifter == nullptr)
            return;

        jassert (numSamples <= maxBlockSize);

        processLiveShifter (samples, numSamples);
    }

private:
    //==========================================================================
    // Mono ring FIFO (power-of-two capacity), allocation-free push/pop.
    //==========================================================================
    struct MonoRing
    {
        std::vector<float> buf;
        int cap { 0 }, mask { 0 }, read { 0 }, write { 0 }, count { 0 };

        void setCapacityAtLeast (int n)
        {
            int c = 1;
            while (c < n) c <<= 1;
            cap = c; mask = c - 1;
            buf.assign ((size_t) cap, 0.0f);
            read = write = count = 0;
        }

        void clear() noexcept
        {
            read = write = count = 0;
            std::fill (buf.begin(), buf.end(), 0.0f);
        }

        void push (const float* s, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
            {
                buf[(size_t) write] = s[i];
                write = (write + 1) & mask;
                if (count < cap) ++count;
                else read = (read + 1) & mask;   // overflow: drop the oldest
            }
        }

        // Returns n samples; if fewer are available, zeroes the remainder.
        void popOrZero (float* d, int n) noexcept
        {
            const int have = juce::jmin (count, n);
            for (int i = 0; i < have; ++i)
            {
                d[i] = buf[(size_t) read];
                read = (read + 1) & mask;
                --count;
            }
            for (int i = have; i < n; ++i)
                d[i] = 0.0f;
        }

        // Returns exactly n samples (the caller guarantees that count >= n).
        void pop (float* d, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
            {
                d[i] = buf[(size_t) read];
                read = (read + 1) & mask;
                --count;
            }
        }
    };

    //==========================================================================
    // Rubber Band LiveShifter (v4) — fixed block size, lowest latency
    //==========================================================================
    void processLiveShifter (float* samples, int numSamples) noexcept
    {
        const double scale = std::pow (2.0, (double) currentSemitones.load() / 12.0);
        if (scale != lsPitchScale)
        {
            liveShifter->setPitchScale (scale);
            lsPitchScale = scale;
        }

        // Accumulate the input; shift() per full block.
        lsIn.push (samples, numSamples);
        while (lsIn.count >= lsBlock)
        {
            lsIn.pop (lsBlockIn.data(), lsBlock);
            const float* in[1]  { lsBlockIn.data() };
            float*       out[1] { lsBlockOut.data() };
            liveShifter->shift (in, out);
            lsOut.push (lsBlockOut.data(), lsBlock);
        }

        lsOut.popOrZero (samples, numSamples);
    }

    void configureLiveShifter()
    {
        using LS = RubberBand::RubberBandLiveShifter;

        // Fast: OptionWindowShort (= DefaultOptions) for the lowest latency.
        // Fine: medium window + formant preservation (better detuned timbre).
        const int opts = liveQuality.load() == 1
                           ? (LS::OptionWindowMedium | LS::OptionFormantPreserved)
                           : (int) LS::DefaultOptions;

        liveShifter = std::make_unique<LS> ((size_t) sampleRate, 1, (LS::Options) opts);

        lsPitchScale = std::pow (2.0, (double) currentSemitones.load() / 12.0);
        liveShifter->setPitchScale (lsPitchScale);

        lsBlock = (int) liveShifter->getBlockSize();
        lsBlockIn.assign  ((size_t) lsBlock, 0.0f);
        lsBlockOut.assign ((size_t) lsBlock, 0.0f);

        const int delay = (int) liveShifter->getStartDelay();
        lsIn.setCapacityAtLeast  (lsBlock + 4 * maxBlockSize + 16);
        lsOut.setCapacityAtLeast (delay + lsBlock + 4 * maxBlockSize + 16);

        // Perceptual latency ~= internal startDelay + the block accumulation (lsBlock).
        lsLatency.store (delay + lsBlock);
    }

    //==========================================================================
    std::unique_ptr<RubberBand::RubberBandLiveShifter> liveShifter;
    juce::SpinLock lock;

    double sampleRate   { 48000.0 };
    int    maxBlockSize { 512 };
    bool   enabled      { false };

    std::atomic<int>   liveQuality      { 0 };   // 0 = Fast, 1 = Fine
    std::atomic<float> currentSemitones { 0.0f };
    std::atomic<int>   lsLatency        { 0 };

    // LiveShifter input/output FIFO + fixed block scratch.
    MonoRing lsIn, lsOut;
    std::vector<float> lsBlockIn, lsBlockOut;
    int      lsBlock      { 0 };
    double   lsPitchScale { 1.0 };
};
