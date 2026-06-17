#pragma once

#include <JuceHeader.h>

/**
    IR (impulse response) cabinet simulator built around juce::dsp::Convolution,
    in ZERO-LATENCY mode (Latency{0}) — for live playing.

    IMPORTANT: "Full Rig" NAM models ALREADY include the cabinet, so in that case
    this module must be bypassed (see setEnabled). A separate IR is only worth
    loading for an "amp-only" (preamp/DI) NAM model.

    Operates on a stereo signal (the processor widens the mono post-NAM signal
    to stereo before this module).
*/
class CabConvolver
{
public:
    CabConvolver()
        : convolution (juce::dsp::Convolution::Latency { 0 })
    {
    }

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        convolution.prepare (spec);
    }

    void reset() noexcept
    {
        convolution.reset();
    }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }
    bool isLoaded() const noexcept { return loaded; }
    juce::String getLoadedName() const { return loadedName; }

    /** Unloads the current IR (the module stops processing until a new IR is
        loaded). Used when restoring a state whose referenced IR is missing. */
    void unload() noexcept { loaded = false; loadedName = {}; }

    /** Load a .wav IR. Call from the message thread (file I/O!).
        The Convolution processes it internally on a background thread and
        swaps the active IR in a thread-safe manner. */
    bool loadIR (const juce::File& irFile)
    {
        if (! irFile.existsAsFile())
            return false;

        convolution.loadImpulseResponse (
            irFile,
            juce::dsp::Convolution::Stereo::yes,      // spreads to stereo if the IR is mono
            juce::dsp::Convolution::Trim::yes,        // trims the silent tail
            0,                                        // 0 = full IR length
            juce::dsp::Convolution::Normalise::yes);  // level normalization

        loadedName = irFile.getFileNameWithoutExtension();
        loaded = true;
        return true;
    }

    // Stereo, in-place processing on an AudioBlock.
    void process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        if (! enabled || ! loaded)
            return;

        juce::dsp::ProcessContextReplacing<float> ctx (block);
        convolution.process (ctx);
    }

private:
    juce::dsp::Convolution convolution;
    bool enabled { false };          // OFF by default (because of Full Rig NAM)
    bool loaded  { false };
    juce::String loadedName;
};
