#pragma once

#include <JuceHeader.h>
#include <vector>

// Signalsmith Stretch (header-only, MIT). Az include útvonalat a
// 'signalsmith-stretch' CMake cél adja (include/ mappa), ami transzitívan
// behúzza a signalsmith-linear függőséget is.
#include "signalsmith-stretch/signalsmith-stretch.h"

/**
    Valós idejű, polifonikus pitch shifter a Signalsmith Stretch köré.

    Drop-hangoláshoz: -12 .. 0 félhang (időnyújtás nélkül, 1:1 arányban).
    Mono jelre dolgozik (a gitár jelút mono a NAM-ig).

    A latencia a Stretch belső blokkméretétől függ; a prepare() után az
    getLatencySamples() adja a teljes késleltetést, amit a processzor a
    setLatencySamples()-szel jelez a hostnak.

    FONTOS: a process() belső pufferekkel dolgozik (a prepare()-ben foglalva),
    így nincs allokáció a hangszálon.
*/
class PitchShifter
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate    = spec.sampleRate;
        maxBlockSize  = (int) spec.maximumBlockSize;

        // Alacsony latenciás preset 1 csatornára.
        stretch.presetDefault (1, (float) sampleRate);

        // Belső I/O pufferek (pointer-tömb a Stretch API-hoz).
        scratchIn.assign  ((size_t) maxBlockSize, 0.0f);
        scratchOut.assign ((size_t) maxBlockSize, 0.0f);

        setSemitones (currentSemitones);
        reset();
    }

    void reset() noexcept
    {
        stretch.reset();
    }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

    // -12 .. +12 félhang. Drop-hangoláshoz negatív értékek.
    void setSemitones (float semitones) noexcept
    {
        currentSemitones = juce::jlimit (-12.0f, 12.0f, semitones);
        stretch.setTransposeSemitones (currentSemitones);
    }

    int getLatencySamples() const noexcept
    {
        return (int) std::lround (stretch.inputLatency() + stretch.outputLatency());
    }

    // Mono, in-place feldolgozás (numSamples <= maxBlockSize).
    void process (float* samples, int numSamples) noexcept
    {
        if (! enabled || currentSemitones == 0.0f)
            return;

        jassert (numSamples <= maxBlockSize);

        std::copy (samples, samples + numSamples, scratchIn.begin());

        float* inPtrs[1]  { scratchIn.data() };
        float* outPtrs[1] { scratchOut.data() };

        // Egyenlő be-/kimeneti minta -> tiszta pitch shift (time ratio = 1).
        stretch.process (inPtrs, numSamples, outPtrs, numSamples);

        std::copy (scratchOut.begin(), scratchOut.begin() + numSamples, samples);
    }

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch;

    double sampleRate   { 48000.0 };
    int    maxBlockSize { 512 };
    bool   enabled      { false };
    float  currentSemitones { 0.0f };

    std::vector<float> scratchIn;
    std::vector<float> scratchOut;
};
