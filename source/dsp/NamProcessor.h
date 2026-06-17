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
            return false;

        try
        {
            // Explicit std::filesystem::path: a string többértelmű lenne a
            // path és a nlohmann::json overload között.
            const std::filesystem::path path { namFile.getFullPathName().toStdString() };
            auto newModel = nam::get_dsp (path);
            if (newModel == nullptr)
                return false;

            newModel->Reset (sampleRate, maxBlockSize);

            // A modell elvárt mintavételi frekvenciájának ellenőrzése.
            const double expected = newModel->GetExpectedSampleRate();
            if (expected > 0.0 && std::abs (expected - sampleRate) > 1.0)
            {
                // TODO (fázis 2): resampling, ha a host SR eltér a modellétől.
                DBG ("NAM: a modell " << expected << " Hz-re készült, a host "
                     << sampleRate << " Hz. Lehetséges hangzásbeli eltérés.");
            }

            model = std::move (newModel);
            loadedName = namFile.getFileNameWithoutExtension();
            return true;
        }
        catch (const std::exception& e)
        {
            DBG ("NAM betöltési hiba: " << e.what());
            return false;
        }
    }

    bool isLoaded() const noexcept { return model != nullptr; }
    juce::String getLoadedName() const { return loadedName; }

    // Mono, in-place feldolgozás.
    void process (float* samples, int numSamples) noexcept
    {
        if (! enabled || model == nullptr)
            return;

        jassert (numSamples <= maxBlockSize);

        for (int i = 0; i < numSamples; ++i)
            inBuffer[(size_t) i] = (NAM_SAMPLE) samples[i];

        // A NAM API channel-major: input[channel][frame]. A modell mono (1 cs.).
        NAM_SAMPLE* inChannels[1]  { inBuffer.data() };
        NAM_SAMPLE* outChannels[1] { outBuffer.data() };
        model->process (inChannels, outChannels, numSamples);

        for (int i = 0; i < numSamples; ++i)
            samples[i] = (float) outBuffer[(size_t) i];
    }

private:
    std::unique_ptr<nam::DSP> model;
    juce::String loadedName;

    double sampleRate   { 48000.0 };
    int    maxBlockSize { 512 };
    bool   enabled      { true };

    std::vector<NAM_SAMPLE> inBuffer;
    std::vector<NAM_SAMPLE> outBuffer;
};
