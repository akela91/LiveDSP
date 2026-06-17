#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>

// Signalsmith Stretch (header-only, MIT). Az include útvonalat a
// 'signalsmith-stretch' CMake cél adja (include/ mappa), ami transzitívan
// behúzza a signalsmith-linear függőséget is.
#include "signalsmith-stretch/signalsmith-stretch.h"

/**
    Valós idejű, polifonikus pitch shifter a Signalsmith Stretch köré.

    Drop-hangoláshoz: -12 .. +12 félhang (időnyújtás nélkül, 1:1 arányban).
    Mono jelre dolgozik (a gitár jelút mono a NAM-ig).

    A latencia a Stretch belső blokkméretétől (blockMs) függ — ez ÉLŐBEN
    állítható (reconfigure), így a felhasználó a latencia/minőség egyensúlyt
    maga választja. A reconfigure az üzenetszálról hívandó; a process() a
    hangszálon tryLock-ot használ (a csere pillanatában 1 blokkot kihagy).

    A getLatencySamples() egy atomikusan cache-elt értéket ad (a kijelzőhöz).
*/
class PitchShifter
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate   = spec.sampleRate;
        maxBlockSize = (int) spec.maximumBlockSize;

        scratchIn.assign  ((size_t) maxBlockSize, 0.0f);
        scratchOut.assign ((size_t) maxBlockSize, 0.0f);

        const juce::SpinLock::ScopedLockType sl (lock);
        configureStretch();
    }

    // STFT-blokk hossza ms-ben (kisebb = kisebb latencia, gyengébb mély-felbontás).
    void setBlockMs (double ms) noexcept { blockMs = juce::jlimit (8.0, 120.0, ms); }

    // Élő újrakonfigurálás az új blockMs-szel — ÜZENETSZÁLRÓL hívd.
    void reconfigure()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        configureStretch();
    }

    void reset()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        stretch.reset();
    }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

    // Csak tárol; a transpose-t a process() alkalmazza a zár alatt (szálbiztos).
    void setSemitones (float semitones) noexcept
    {
        currentSemitones.store (juce::jlimit (-12.0f, 12.0f, semitones));
    }

    int getLatencySamples() const noexcept { return latencyInSamples.load(); }

    // Mono, in-place feldolgozás (numSamples <= maxBlockSize).
    void process (float* samples, int numSamples) noexcept
    {
        if (! enabled || currentSemitones.load() == 0.0f)
            return;

        // tryLock: ha épp reconfigure zajlik (üzenetszál), kihagyjuk a blokkot.
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked())
            return;

        jassert (numSamples <= maxBlockSize);

        stretch.setTransposeSemitones (currentSemitones.load());

        std::copy (samples, samples + numSamples, scratchIn.begin());
        float* inPtrs[1]  { scratchIn.data() };
        float* outPtrs[1] { scratchOut.data() };
        stretch.process (inPtrs, numSamples, outPtrs, numSamples);
        std::copy (scratchOut.begin(), scratchOut.begin() + numSamples, samples);
    }

private:
    // A zár tartása mellett hívandó.
    void configureStretch()
    {
        const int block    = juce::jmax (128, juce::roundToInt (sampleRate * blockMs * 0.001));
        const int interval = juce::jmax (1, block / 4);
        stretch.configure (1, block, interval);
        stretch.setTransposeSemitones (currentSemitones.load());
        stretch.reset();
        latencyInSamples.store ((int) std::lround (stretch.inputLatency() + stretch.outputLatency()));
    }

    signalsmith::stretch::SignalsmithStretch<float> stretch;
    juce::SpinLock lock;

    double sampleRate   { 48000.0 };
    int    maxBlockSize { 512 };
    bool   enabled      { false };
    double blockMs      { 40.0 };
    std::atomic<float> currentSemitones { 0.0f };
    std::atomic<int>   latencyInSamples { 0 };

    std::vector<float> scratchIn;
    std::vector<float> scratchOut;
};
