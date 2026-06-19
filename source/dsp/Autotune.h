#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <cmath>

#include "PitchDetector.h"

// Same low-latency granular shifter the guitar Transpose uses — here calibrated
// for a single voice (small grain) and driven by the autotune correction.
#include "GranularPitchShifter.h"

/**
    Low-latency vocal Autotune (built on the custom GranularPitchShifter).

    Pipeline (all mono, runs before the VoiceChain on the audio thread):
      1. Detect the current pitch from a sliding history window (McLeod / NSDF,
         reusing the tuner's PitchDetector). Detection is throttled to a fixed
         sample HOP so the CPU cost is independent of the host block size.
      2. Quantize the detected note to the NEAREST semitone -> a target correction.
      3. Glide the applied correction toward that target. A single AMOUNT controls
         how aggressively it intervenes: it scales both how far the note is pulled
         (0 % = off .. 100 % = full snap) AND how fast it snaps (the host maps it to
         the RETUNE time: low = slow/natural, high = instant/"robotic").
      4. Apply the (fractional) correction with the granular shifter — same engine
         as the guitar Transpose, with a small voice-calibrated grain for very low
         latency (~grain/2, ~8 ms).

    prepare/reset run on the MESSAGE THREAD; the setters/process are real-time safe
    (atomics + a pre-allocated buffer, no locking, no allocation in process()).
    getLatencySamples() returns the shifter's latency while actively correcting.
*/
class Autotune
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        detector.setSampleRate (sampleRate);
        detector.setRange (70.0f, 1100.0f);   // typical sung-voice range

        analysis.assign     ((size_t) analysisSize, 0.0f);
        analysisWork.assign ((size_t) analysisSize, 0.0f);
        analysisWrite = 0;
        sinceDetect   = 0;
        pendingTarget = 0.0f;
        currentSemis  = 0.0f;
        hasPitch      = false;

        // Warm the detector up off the audio thread (allocates its work buffers once).
        detector.detect (analysisWork.data(), analysisSize);

        granular.prepare (spec);
        granular.setGrainMs (grainMs);   // voice-calibrated: small grain, low latency
    }

    void reset()
    {
        granular.reset();
        std::fill (analysis.begin(), analysis.end(), 0.0f);
        analysisWrite = 0;
        sinceDetect   = 0;
        pendingTarget = 0.0f;
        currentSemis  = 0.0f;
        hasPitch      = false;
    }

    void  setEnabled  (bool b)    noexcept { enabled.store (b); }
    void  setAmount   (float pct) noexcept { amount.store (juce::jlimit (0.0f, 1.0f, pct * 0.01f)); }
    void  setRetuneMs (float ms)  noexcept { retuneMs.store (juce::jlimit (0.0f, 250.0f, ms)); }

    int getLatencySamples() const noexcept
    {
        return (enabled.load() && amount.load() > 0.001f) ? granular.getLatencySamples() : 0;
    }

    // Mono, in-place processing.
    void process (float* samples, int numSamples) noexcept
    {
        // Always feed the analysis ring (cheap) so detection has history the moment
        // the effect is switched on.
        pushAnalysis (samples, numSamples);

        if (! enabled.load())
            return;

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

                // Smooth the detected pitch to reject the jitter of an unsteady
                // voice, then pick the target note with HYSTERESIS: once locked to a
                // note we only switch when the (smoothed) pitch moves clearly past
                // the boundary, so it stops flip-flopping between adjacent notes.
                if (! hasPitch) { smoothedMidi = midi; heldNote = (int) std::lround (midi); hasPitch = true; }
                else            { smoothedMidi += pitchSmoothing * (midi - smoothedMidi); }

                if      (smoothedMidi > (float) heldNote + 0.5f + noteHysteresis) heldNote = (int) std::lround (smoothedMidi);
                else if (smoothedMidi < (float) heldNote - 0.5f - noteHysteresis) heldNote = (int) std::lround (smoothedMidi);

                target = juce::jlimit (-2.0f, 2.0f, (float) heldNote - smoothedMidi) * amount.load();
            }
            else
            {
                hasPitch = false;   // unvoiced -> re-lock onto the next sung note
            }
            pendingTarget = target;
        }

        // --- 3) Glide toward the target (AMOUNT -> how fast/aggressive) -------
        const float blockMs = (float) ((double) numSamples / sampleRate * 1000.0);
        const float rt      = retuneMs.load();
        const float a       = rt <= 0.0f ? 0.0f : std::exp (-blockMs / rt);
        currentSemis = a * currentSemis + (1.0f - a) * pendingTarget;

        // --- 4) Apply the shift with the granular engine ---------------------
        // At AMOUNT 0 the effect is off, so bypass entirely (keeps the voice clean
        // and zero-latency); otherwise correct continuously.
        if (amount.load() <= 0.001f)
            return;

        granular.setSemitones (currentSemis);
        granular.process (samples, numSamples);
    }

private:
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

    //==========================================================================
    PitchDetector detector;
    GranularPitchShifter granular;

    double sampleRate { 48000.0 };

    std::atomic<bool>  enabled  { false };
    std::atomic<float> amount   { 1.0f };    // 0..1 (UI is 0..100 %)
    std::atomic<float> retuneMs { 20.0f };   // glide time toward the target note

    static constexpr float grainMs = 16.0f;  // voice-calibrated grain (~8 ms latency)

    // Pitch-detection sliding window + throttle.
    static constexpr int analysisSize = 1024;   // ~21 ms @ 48 kHz
    static constexpr int detectHop    = 256;    // re-detect at most every ~5 ms
    std::vector<float> analysis, analysisWork;
    int   analysisWrite { 0 };
    int   sinceDetect   { 0 };
    float pendingTarget { 0.0f };   // latest computed target correction (semitones)
    float currentSemis  { 0.0f };   // smoothed, actually-applied correction

    // Note-tracking state (stability for an unsteady voice).
    float smoothedMidi  { 0.0f };
    int   heldNote      { 0 };
    bool  hasPitch      { false };
    static constexpr float pitchSmoothing = 0.4f;   // one-pole on detected pitch
    static constexpr float noteHysteresis = 0.15f;  // dead-zone past the note boundary
};
