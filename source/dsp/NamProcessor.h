#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>
#include <atomic>
#include <filesystem>

// Neural Amp Modeler Core. Az include útvonalat a CMake nam_core célja adja.
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

/**
    NAM (Neural Amp Modeler) wrapper.

    Betölt egy .nam modellt és blokkonként feldolgozza a mono jelet.
    A NAM_SAMPLE alapból double, ezért float<->double konverziót végzünk
    a prepare()-ben foglalt pufferekkel (nincs allokáció a hangszálon).

    Modellbetöltés: NEM a hangszálról hívandó (fájl-IO). A processzor a
    prepareToPlay-ben vagy egy üzenetszálas akcióból tölt, majd atomikusan
    cseréli az aktív modellt.
*/
class NamProcessor
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate   = spec.sampleRate;
        maxBlockSize = (int) spec.maximumBlockSize;

        inBuffer.assign  ((size_t) maxBlockSize, 0.0);
        outBuffer.assign ((size_t) maxBlockSize, 0.0);

        if (model != nullptr)
            model->Reset (sampleRate, maxBlockSize);
    }

    void reset() noexcept {}

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

    /** Betölt egy .nam modellt. Visszatér true-val siker esetén.
        Hívd üzenetszálról (fájl-IO!). */
    bool loadModel (const juce::File& namFile)
    {
        if (! namFile.existsAsFile())
        {
            lastStatus = "fájl nem létezik: " + namFile.getFullPathName();
            return false;
        }

        try
        {
            // Explicit std::filesystem::path: a string többértelmű lenne a
            // path és a nlohmann::json overload között.
            const std::filesystem::path path { namFile.getFullPathName().toStdString() };
            auto newModel = nam::get_dsp (path);
            if (newModel == nullptr)
            {
                lastStatus = "get_dsp nullptr-t adott: " + namFile.getFileName();
                return false;
            }

            newModel->Reset (sampleRate, maxBlockSize);

            // LOUDNESS-NORMALIZÁLÁS: a NAM modellek beágyazott loudness metaadata
            // alapján közös célszintre hozzuk a kimenetet. Enélkül egyes modellek
            // (pl. 6505+) extrém hangosak és gerjednek, míg mások halkak.
            float newGain = 1.0f;
            if (newModel->HasLoudness())
            {
                constexpr double targetLoudnessDb = -18.0;
                newGain = (float) juce::Decibels::decibelsToGain (
                              targetLoudnessDb - newModel->GetLoudness());
            }

            // A modell elvárt mintavételi frekvenciájának ellenőrzése.
            const double expected = newModel->GetExpectedSampleRate();
            if (expected > 0.0 && std::abs (expected - sampleRate) > 1.0)
                lastStatus = "betöltve (" + juce::String (expected, 0) + " Hz modell)";
            else
                lastStatus = "betöltve";

            // Szálbiztos csere: a get_dsp/Reset (lassú) a zár NÉLKÜL futott;
            // csak a pointer-cserét védjük. A régi modell itt, az üzenetszálon
            // semmisül meg (nem a hangszálon).
            {
                const juce::SpinLock::ScopedLockType sl (modelLock);
                model = std::move (newModel);
                normalizationGain = newGain;
            }
            loadedName = namFile.getFileNameWithoutExtension();
            return true;
        }
        catch (const std::exception& e)
        {
            lastStatus = juce::String ("hiba: ") + e.what();
            return false;
        }
        catch (...)
        {
            lastStatus = "ismeretlen kivétel a betöltéskor";
            return false;
        }
    }

    bool isLoaded() const noexcept { return model != nullptr; }
    juce::String getLoadedName() const { return loadedName; }
    juce::String getLastStatus() const { return lastStatus; }

    // Mono, in-place feldolgozás.
    void process (float* samples, int numSamples) noexcept
    {
        if (! enabled)
            return;

        // tryLock: ha épp modellcsere zajlik (üzenetszál), kihagyjuk ezt a blokkot
        // (rövid, hallhatatlan), nem blokkoljuk a hangszálat.
        const juce::SpinLock::ScopedTryLockType sl (modelLock);
        if (! sl.isLocked() || model == nullptr)
            return;

        jassert (numSamples <= maxBlockSize);

        for (int i = 0; i < numSamples; ++i)
            inBuffer[(size_t) i] = (NAM_SAMPLE) samples[i];

        // A NAM API channel-major: input[channel][frame]. A modell mono (1 cs.).
        NAM_SAMPLE* inChannels[1]  { inBuffer.data() };
        NAM_SAMPLE* outChannels[1] { outBuffer.data() };
        model->process (inChannels, outChannels, numSamples);

        for (int i = 0; i < numSamples; ++i)
            samples[i] = (float) outBuffer[(size_t) i] * normalizationGain;
    }

private:
    std::unique_ptr<nam::DSP> model;
    juce::SpinLock modelLock;   // a model pointer + normalizationGain cseréjét védi
    float normalizationGain { 1.0f };
    juce::String loadedName;
    juce::String lastStatus { "nincs betöltési kísérlet" };

    double sampleRate   { 48000.0 };
    int    maxBlockSize { 512 };
    bool   enabled      { true };

    std::vector<NAM_SAMPLE> inBuffer;
    std::vector<NAM_SAMPLE> outBuffer;
};
