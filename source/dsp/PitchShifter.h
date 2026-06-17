#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <cmath>
#include <memory>

// Signalsmith Stretch (header-only, MIT). The include path is provided by the
// 'signalsmith-stretch' CMake target (include/ folder), which transitively
// pulls in the signalsmith-linear dependency as well.
#include "signalsmith-stretch/signalsmith-stretch.h"

// Rubber Band Library (GPL/commercial). The single-file build's static lib
// (single/RubberBandSingle.cpp) compiles BOTH classes:
//  - RubberBandStretcher: real-time (OptionProcessRealTime) time/pitch.
//  - RubberBandLiveShifter (v4): specifically the lowest-latency live pitch.
#include <rubberband/RubberBandStretcher.h>
#include <rubberband/RubberBandLiveShifter.h>

/**
    Real-time, polyphonic pitch shifter with THREE selectable engines.

    - Signalsmith Stretch: live-adjustable block size (the 'Pitch Latency'
      knob), 1:1 block processing with internal fixed latency.
    - Rubber Band Stretcher (R3 'Finer', OptionProcessRealTime): good polyphonic
      quality (power chords), higher fixed engine latency.
    - Rubber Band LiveShifter (v4): fixed block size (getBlockSize) shift(),
      designed for the lowest latency — for live monitoring.

    For drop tuning: -12 .. +12 semitones (without time stretching, at a 1:1 ratio).
    Operates on a mono signal (the guitar signal chain is mono up to the NAM).

    reconfigure/reset/prepare must be called from the MESSAGE THREAD (may allocate); process()
    uses a tryLock on the audio thread (skips 1 block at the moment of the swap).
    getLatencySamples() returns the ACTIVE engine's atomically cached latency.
*/
class PitchShifter
{
public:
    enum class Engine { signalsmith = 0, rubberband = 1, rubberbandLive = 2 };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate   = spec.sampleRate;
        maxBlockSize = (int) spec.maximumBlockSize;

        scratchIn.assign  ((size_t) maxBlockSize, 0.0f);
        scratchOut.assign ((size_t) maxBlockSize, 0.0f);
        rbScratch.assign  ((size_t) maxBlockSize, 0.0f);

        const juce::SpinLock::ScopedLockType sl (lock);
        configureStretch();
        configureRubberBand();
        configureLiveShifter();
    }

    // STFT block length in ms for the Signalsmith engine (smaller = lower latency).
    void setBlockMs (double ms) noexcept { blockMs = juce::jlimit (8.0, 120.0, ms); }

    // Live reconfiguration with the new blockMs (Signalsmith) — call from the MESSAGE THREAD.
    void reconfigure()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        configureStretch();
    }

    void reset()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        stretch.reset();
        if (rubberBand != nullptr)
        {
            rubberBand->reset();
            rbOut.clear();
        }
        if (liveShifter != nullptr)
        {
            liveShifter->reset();
            lsIn.clear();
            lsOut.clear();
        }
    }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

    void setEngine (int e) noexcept { engine.store (e); }

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

    int getLatencySamples() const noexcept
    {
        switch ((Engine) engine.load())
        {
            case Engine::rubberband:     return rbLatency.load();
            case Engine::rubberbandLive: return lsLatency.load();
            case Engine::signalsmith:
            default:                     return latencyInSamples.load();
        }
    }

    // Mono, in-place processing (numSamples <= maxBlockSize).
    void process (float* samples, int numSamples) noexcept
    {
        if (! enabled || currentSemitones.load() == 0.0f)
            return;

        // tryLock: if a reconfigure/reset is in progress (message thread), skip the block.
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked())
            return;

        jassert (numSamples <= maxBlockSize);

        switch ((Engine) engine.load())
        {
            case Engine::rubberband:
                if (rubberBand != nullptr) { processRubberBand (samples, numSamples); return; }
                break;
            case Engine::rubberbandLive:
                if (liveShifter != nullptr) { processLiveShifter (samples, numSamples); return; }
                break;
            default:
                break;
        }
        processSignalsmith (samples, numSamples);
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
    // Signalsmith engine
    //==========================================================================
    void processSignalsmith (float* samples, int numSamples) noexcept
    {
        stretch.setTransposeSemitones (currentSemitones.load());

        std::copy (samples, samples + numSamples, scratchIn.begin());
        float* inPtrs[1]  { scratchIn.data() };
        float* outPtrs[1] { scratchOut.data() };
        stretch.process (inPtrs, numSamples, outPtrs, numSamples);
        std::copy (scratchOut.begin(), scratchOut.begin() + numSamples, samples);
    }

    void configureStretch()
    {
        const int block    = juce::jmax (128, juce::roundToInt (sampleRate * blockMs * 0.001));
        const int interval = juce::jmax (1, block / 4);
        stretch.configure (1, block, interval);
        stretch.setTransposeSemitones (currentSemitones.load());
        stretch.reset();
        latencyInSamples.store ((int) std::lround (stretch.inputLatency() + stretch.outputLatency()));
    }

    //==========================================================================
    // Rubber Band Stretcher (real-time, R3 'Finer')
    //==========================================================================
    void processRubberBand (float* samples, int numSamples) noexcept
    {
        const double scale = std::pow (2.0, (double) currentSemitones.load() / 12.0);
        if (scale != rbPitchScale)
        {
            rubberBand->setPitchScale (scale);
            rbPitchScale = scale;
        }

        const float* inPtr[1] { samples };
        rubberBand->process (inPtr, (size_t) numSamples, false);

        for (int avail = rubberBand->available(); avail > 0; avail = rubberBand->available())
        {
            const int chunk = juce::jmin (avail, maxBlockSize);
            float* outPtr[1] { rbScratch.data() };
            const int got = (int) rubberBand->retrieve (outPtr, (size_t) chunk);
            if (got <= 0)
                break;
            rbOut.push (rbScratch.data(), got);
        }

        rbOut.popOrZero (samples, numSamples);
    }

    void configureRubberBand()
    {
        using RB = RubberBand::RubberBandStretcher;

        const int opts = RB::OptionProcessRealTime
                       | RB::OptionEngineFiner
                       | RB::OptionPitchHighQuality;

        rbPitchScale = std::pow (2.0, (double) currentSemitones.load() / 12.0);

        rubberBand = std::make_unique<RB> ((size_t) sampleRate, 1,
                                           (RB::Options) opts, 1.0, rbPitchScale);
        rubberBand->setMaxProcessSize ((size_t) maxBlockSize);

        const int delay = (int) rubberBand->getStartDelay();
        rbOut.setCapacityAtLeast (delay + 8 * maxBlockSize + 16);
        rbLatency.store (delay);
    }

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
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    std::unique_ptr<RubberBand::RubberBandStretcher>  rubberBand;
    std::unique_ptr<RubberBand::RubberBandLiveShifter> liveShifter;
    juce::SpinLock lock;

    double sampleRate   { 48000.0 };
    int    maxBlockSize { 512 };
    bool   enabled      { false };
    double blockMs      { 40.0 };

    std::atomic<int>   engine           { (int) Engine::rubberbandLive };
    std::atomic<int>   liveQuality      { 0 };   // 0 = Fast, 1 = Fine
    std::atomic<float> currentSemitones { 0.0f };
    std::atomic<int>   latencyInSamples { 0 };   // Signalsmith
    std::atomic<int>   rbLatency        { 0 };   // Rubber Band Stretcher
    std::atomic<int>   lsLatency        { 0 };   // Rubber Band LiveShifter

    std::vector<float> scratchIn;
    std::vector<float> scratchOut;
    std::vector<float> rbScratch;

    // Rubber Band Stretcher output FIFO.
    MonoRing rbOut;
    double   rbPitchScale { 1.0 };

    // LiveShifter input/output FIFO + fixed block scratch.
    MonoRing lsIn, lsOut;
    std::vector<float> lsBlockIn, lsBlockOut;
    int      lsBlock      { 0 };
    double   lsPitchScale { 1.0 };
};
