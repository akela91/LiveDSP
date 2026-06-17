#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>
#include <atomic>
#include <filesystem>

// Neural Amp Modeler Core. The include path is provided by the CMake nam_core target.
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

/**
    NAM (Neural Amp Modeler) wrapper.

    Loads a .nam model and processes the mono signal block by block.
    NAM_SAMPLE is double by default, so we perform float<->double conversion
    using buffers allocated in prepare() (no allocation on the audio thread).

    Model loading: must NOT be called from the audio thread (file I/O). The
    processor loads it in prepareToPlay or from a message-thread action, then
    atomically swaps the active model.
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

    /** Loads a .nam model. Returns true on success.
        Call from the message thread (file I/O!). */
    bool loadModel (const juce::File& namFile)
    {
        if (! namFile.existsAsFile())
        {
            lastStatus = "file does not exist: " + namFile.getFullPathName();
            return false;
        }

        try
        {
            // Explicit std::filesystem::path: a plain string would be ambiguous
            // between the path and the nlohmann::json overload.
            const std::filesystem::path path { namFile.getFullPathName().toStdString() };
            auto newModel = nam::get_dsp (path);
            if (newModel == nullptr)
            {
                lastStatus = "get_dsp returned nullptr: " + namFile.getFileName();
                return false;
            }

            newModel->Reset (sampleRate, maxBlockSize);

            // LOUDNESS NORMALIZATION: bring the output to a common target level
            // based on the loudness metadata embedded in NAM models. Without this,
            // some models (e.g. 6505+) are extremely loud and feed back, while
            // others are quiet.
            float newGain = 1.0f;
            if (newModel->HasLoudness())
            {
                constexpr double targetLoudnessDb = -18.0;
                newGain = (float) juce::Decibels::decibelsToGain (
                              targetLoudnessDb - newModel->GetLoudness());
            }

            // Check the model's expected sample rate.
            const double expected = newModel->GetExpectedSampleRate();
            if (expected > 0.0 && std::abs (expected - sampleRate) > 1.0)
                lastStatus = "loaded (" + juce::String (expected, 0) + " Hz model)";
            else
                lastStatus = "loaded";

            // Thread-safe swap: get_dsp/Reset (slow) ran WITHOUT the lock;
            // we only guard the pointer swap. The old model is destroyed here,
            // on the message thread (not on the audio thread).
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
            lastStatus = juce::String ("error: ") + e.what();
            return false;
        }
        catch (...)
        {
            lastStatus = "unknown exception while loading";
            return false;
        }
    }

    bool isLoaded() const noexcept { return model != nullptr; }
    juce::String getLoadedName() const { return loadedName; }
    juce::String getLastStatus() const { return lastStatus; }

    // Mono, in-place processing.
    void process (float* samples, int numSamples) noexcept
    {
        if (! enabled)
            return;

        // tryLock: if a model swap is in progress (message thread), we skip this
        // block (short, inaudible) rather than blocking the audio thread.
        const juce::SpinLock::ScopedTryLockType sl (modelLock);
        if (! sl.isLocked() || model == nullptr)
            return;

        jassert (numSamples <= maxBlockSize);

        for (int i = 0; i < numSamples; ++i)
            inBuffer[(size_t) i] = (NAM_SAMPLE) samples[i];

        // The NAM API is channel-major: input[channel][frame]. The model is mono (1 ch.).
        NAM_SAMPLE* inChannels[1]  { inBuffer.data() };
        NAM_SAMPLE* outChannels[1] { outBuffer.data() };
        model->process (inChannels, outChannels, numSamples);

        for (int i = 0; i < numSamples; ++i)
            samples[i] = (float) outBuffer[(size_t) i] * normalizationGain;
    }

private:
    std::unique_ptr<nam::DSP> model;
    juce::SpinLock modelLock;   // guards swapping the model pointer + normalizationGain
    float normalizationGain { 1.0f };
    juce::String loadedName;
    juce::String lastStatus { "no load attempt yet" };

    double sampleRate   { 48000.0 };
    int    maxBlockSize { 512 };
    bool   enabled      { true };

    std::vector<NAM_SAMPLE> inBuffer;
    std::vector<NAM_SAMPLE> outBuffer;
};
