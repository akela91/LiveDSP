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

        // ALACSONY LATENCIÁS konfiguráció: a presetDefault ~120 ms-os STFT-blokkot
        // használ (érzékelhetően ~száz ms latencia). Egy kisebb blokk (~30 ms)
        // drámaian csökkenti a latenciát, cserébe kicsit gyengébb a mély hangok
        // felbontása — drop-hangoláshoz jó kompromisszum. blockMs állítható.
        const int block    = juce::jmax (256, juce::roundToInt (sampleRate * blockMs * 0.001));
        const int interval = juce::jmax (1, block / 4);
        stretch.configure (1, block, interval);

        // Belső I/O pufferek (pointer-tömb a Stretch API-hoz).
        scratchIn.assign  ((size_t) maxBlockSize, 0.0f);
        scratchOut.assign ((size_t) maxBlockSize, 0.0f);

        setSemitones (currentSemitones);
        reset();
    }

    // STFT-blokk hossza ms-ben (kisebb = kisebb latencia). prepare() előtt hívd.
    void setBlockMs (double ms) noexcept { blockMs = juce::jlimit (10.0, 120.0, ms); }

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
    double blockMs      { 30.0 };   // alacsony latencia

    std::vector<float> scratchIn;
    std::vector<float> scratchOut;
};
