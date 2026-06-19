#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <cmath>
#include <memory>

#include "PitchDetector.h"

// Rubber Band LiveShifter (v4): the lowest-latency live pitch shifter — the same
// engine the guitar Transpose uses. Pulled in by the 'rubberband' CMake target.
#include <rubberband/RubberBandLiveShifter.h>

/**
    Low-latency vocal Autotune.

    Pipeline (all mono, runs before the VoiceChain on the audio thread):
      1. Detect the current pitch from a sliding history window (McLeod / NSDF,
         reusing the tuner's PitchDetector). Detection is throttled to a fixed
         sample HOP so the CPU cost is independent of the host block size.
      2. Quantize the detected note to the NEAREST semitone -> a target correction.
      3. Glide the applied correction toward that target. A single AMOUNT controls
         how aggressively it intervenes: it scales both how far the note is pulled
         (0 % = off .. 100 % = full snap) AND how fast it snaps (the host maps it to
         the RETUNE time: low = slow/natural, high = instant/"robotic").
      4. Apply the (fractional) correction with the Rubber Band LiveShifter, with
         formant preservation so the timbre stays natural (no "chipmunk").

    prepare/reset must be called from the MESSAGE THREAD (may allocate); process()
    uses a tryLock on the audio thread (skips a block at the moment of a reconfigure).
    getLatencySamples() returns the shifter's latency while enabled, else 0.
*/
class Autotune
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate   = spec.sampleRate;
        maxBlockSize = (int) spec.maximumBlockSize;

        detector.setSampleRate (sampleRate);
        detector.setRange (70.0f, 1100.0f);   // typical sung-voice range

        analysis.assign     ((size_t) analysisSize, 0.0f);
        analysisWork.assign ((size_t) analysisSize, 0.0f);
        analysisWrite = 0;
        sinceDetect   = 0;
        pendingTarget = 0.0f;

        // Warm the detector up off the audio thread (allocates its work buffers once).
        detector.detect (analysisWork.data(), analysisSize);

        const juce::SpinLock::ScopedLockType sl (lock);
        configureShifter();
        currentSemis = 0.0f;
    }

    void reset()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        if (shifter != nullptr) { shifter->reset(); lsIn.clear(); lsOut.clear(); }
        std::fill (analysis.begin(), analysis.end(), 0.0f);
        analysisWrite = 0;
        sinceDetect   = 0;
        pendingTarget = 0.0f;
        currentSemis  = 0.0f;
    }

    void  setEnabled  (bool b)     noexcept { enabled.store (b); }
    void  setAmount   (float pct)  noexcept { amount.store (juce::jlimit (0.0f, 1.0f, pct * 0.01f)); }
    void  setRetuneMs (float ms)   noexcept { retuneMs.store (juce::jlimit (0.0f, 250.0f, ms)); }

    int getLatencySamples() const noexcept { return enabled.load() ? lsLatency.load() : 0; }

    // Mono, in-place processing (numSamples <= maxBlockSize).
    void process (float* samples, int numSamples) noexcept
    {
        // Always feed the analysis ring (cheap) so detection has history the moment
        // the effect is switched on.
        pushAnalysis (samples, numSamples);

        if (! enabled.load())
            return;

        // tryLock: if a reconfigure/reset is in progress (message thread), skip the block.
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || shifter == nullptr)
            return;

        jassert (numSamples <= maxBlockSize);

        // --- 1) Detect the current pitch (throttled to a fixed hop) ----------
        sinceDetect += numSamples;
        if (sinceDetect >= detectHop)
        {
            sinceDetect = 0;
            copyAnalysisOrdered();
            const auto det = detector.detect (analysisWork.data(), analysisSize);

            // --- 2) Quantize to the nearest semitone -> target correction ----
            float target = 0.0f;
            if (det.frequency > 0.0f && det.clarity > 0.0f)
            {
                const float midi = 69.0f + 12.0f * std::log2 (det.frequency / 440.0f);
                const float nearest = std::round (midi);   // nearest note (chromatic)
                target = juce::jlimit (-2.0f, 2.0f, nearest - midi) * amount.load();
            }
            pendingTarget = target;
        }

        // --- 3) Glide toward the target (RETUNE = how fast/aggressive) --------
        const float blockMs = (float) ((double) numSamples / sampleRate * 1000.0);
        const float rt      = retuneMs.load();
        const float a       = rt <= 0.0f ? 0.0f : std::exp (-blockMs / rt);
        currentSemis = a * currentSemis + (1.0f - a) * pendingTarget;

        // --- 4) Apply the shift with the low-latency live shifter ------------
        const double sc = std::pow (2.0, (double) currentSemis / 12.0);
        if (sc != lsPitchScale) { shifter->setPitchScale (sc); lsPitchScale = sc; }

        lsIn.push (samples, numSamples);
        while (lsIn.count >= lsBlock)
        {
            lsIn.pop (lsBlockIn.data(), lsBlock);
            const float* in[1]  { lsBlockIn.data() };
            float*       out[1] { lsBlockOut.data() };
            shifter->shift (in, out);
            lsOut.push (lsBlockOut.data(), lsBlock);
        }
        lsOut.popOrZero (samples, numSamples);
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
    void pushAnalysis (const float* s, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            analysis[(size_t) analysisWrite] = s[i];
            if (++analysisWrite >= analysisSize) analysisWrite = 0;
        }
    }

    // Copy the ring into analysisWork in chronological order (oldest first).
    void copyAnalysisOrdered() noexcept
    {
        int r = analysisWrite;   // the oldest sample sits at the write cursor
        for (int i = 0; i < analysisSize; ++i)
        {
            analysisWork[(size_t) i] = analysis[(size_t) r];
            if (++r >= analysisSize) r = 0;
        }
    }

    void configureShifter()
    {
        using LS = RubberBand::RubberBandLiveShifter;

        // Short window = lowest latency; formant preservation keeps the corrected
        // voice natural instead of "chipmunk"-y when shifting.
        const int opts = (int) LS::OptionWindowShort | (int) LS::OptionFormantPreserved;

        shifter = std::make_unique<LS> ((size_t) sampleRate, 1, (LS::Options) opts);
        shifter->setPitchScale (1.0);
        lsPitchScale = 1.0;

        lsBlock = (int) shifter->getBlockSize();
        lsBlockIn.assign  ((size_t) lsBlock, 0.0f);
        lsBlockOut.assign ((size_t) lsBlock, 0.0f);

        const int delay = (int) shifter->getStartDelay();
        lsIn.setCapacityAtLeast  (lsBlock + 4 * maxBlockSize + 16);
        lsOut.setCapacityAtLeast (delay + lsBlock + 4 * maxBlockSize + 16);

        // Perceptual latency ~= internal startDelay + the block accumulation (lsBlock).
        lsLatency.store (delay + lsBlock);
    }

    //==========================================================================
    PitchDetector detector;
    std::unique_ptr<RubberBand::RubberBandLiveShifter> shifter;
    juce::SpinLock lock;

    double sampleRate   { 48000.0 };
    int    maxBlockSize { 512 };

    std::atomic<bool>  enabled  { false };
    std::atomic<float> amount   { 1.0f };    // 0..1 (UI is 0..100 %)
    std::atomic<float> retuneMs { 20.0f };   // glide time toward the target note
    std::atomic<int>   lsLatency { 0 };

    // Pitch-detection sliding window + throttle.
    static constexpr int analysisSize = 1024;   // ~21 ms @ 48 kHz
    static constexpr int detectHop    = 256;    // re-detect at most every ~5 ms
    std::vector<float> analysis, analysisWork;
    int   analysisWrite { 0 };
    int   sinceDetect   { 0 };
    float pendingTarget { 0.0f };   // latest computed target correction (semitones)
    float currentSemis  { 0.0f };   // smoothed, actually-applied correction

    // LiveShifter input/output FIFO + fixed block scratch.
    MonoRing lsIn, lsOut;
    std::vector<float> lsBlockIn, lsBlockOut;
    int      lsBlock      { 0 };
    double   lsPitchScale { 1.0 };
};
