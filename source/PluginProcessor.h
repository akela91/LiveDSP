#pragma once

#include <JuceHeader.h>

#include "dsp/NoiseGate.h"
#include "dsp/Overdrive.h"
#include "dsp/PitchShifter.h"
#include "dsp/NamProcessor.h"
#include "dsp/CabConvolver.h"
#include "dsp/Equalizer.h"
#include "dsp/VoiceChain.h"
#include "dsp/Autotune.h"

/**
    GuitarDSP — dual-mode standalone audio app: GUITAR amp simulator + VOCALS
    channel. The runtime-switchable 'appMode' decides which signal chain runs.
    The startup screen (Landing) lets the user pick the mode (0 = none selected).

    Guitar signal chain:
      In -> [InputGain] -> [Gate] -> [Pitch] -> [Overdrive] -> [NAM]
         -> (mono->stereo) -> [Cab IR] -> [EQ] -> [Delay] -> [Reverb] -> [OutputGain] -> Out

    Vocals signal chain (mono microphone -> stereo, VoiceChain ProcessorChain):
      In -> [Gain] -> [LowCut 90Hz] -> [Comp] -> [Air 6kHz] -> [Reverb] -> [Limiter] -> Out
*/
class LiveDspProcessor  : public juce::AudioProcessor
{
public:
    LiveDspProcessor();
    ~LiveDspProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LiveDSP"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; } // reverb/delay tail

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int sizeInBytes) override;

    //==============================================================================
    // Model/IR loading (call from the message thread).
    bool loadNamModel (const juce::File& namFile) { return nam.loadModel (namFile); }
    bool loadCabIr    (const juce::File& irFile)  { return cab.loadIR (irFile); }
    NamProcessor& getNam()       { return nam; }
    CabConvolver& getCab()       { return cab; }
    NoiseGate&    getGate()      { return gate; }

    // Vocals display meters for the UI LEDs (0..1).
    float getVocalGateGain()      const noexcept { return vocal.getGateGain(); }
    float getVocalCompReduction() const noexcept { return vocal.getCompReduction(); }

    // Input signal level for the INPUT panel's LED meter: the peak (linear, post
    // input-gain) of the signal entering the active chain, with meter ballistics
    // (fast attack / slow release). Read from the message thread.
    float getInputLevel() const noexcept { return inputLevel.load(); }

    int getCurrentBlockSize() const noexcept { return currentBlockSize; }

    //==============================================================================
    // App mode: 0 = none selected (Landing), 1 = Guitar, 2 = Vocals. Switchable
    // at runtime (from the Landing screen and the views' "menu" button). Not
    // persisted in the state — the Landing screen comes up on every startup.
    enum class AppMode { none = 0, guitar = 1, vocal = 2 };
    int  getAppMode() const noexcept { return appMode.load(); }
    void setAppMode (int m) noexcept { appMode.store (m); }

    //==============================================================================
    // Input channel selection PER MODE separately (which of the Scarlett's
    // several inputs the given chain should listen to). 0-based index; processBlock
    // takes ONLY this single channel (it does not mix the inputs together), so the
    // microphone does not leak into the guitar chain and vice versa. Persisted in the state.
    int  getGuitarInputCh() const noexcept { return guitarInputCh.load(); }
    int  getVocalInputCh()  const noexcept { return vocalInputCh.load(); }
    void setGuitarInputCh (int ch) noexcept { guitarInputCh.store (juce::jmax (0, ch)); }
    void setVocalInputCh  (int ch) noexcept { vocalInputCh.store  (juce::jmax (0, ch)); }

    // Latency (in samples) added by the currently ACTIVE modules — dynamic,
    // follows the switch states (e.g. pitch on/off). For the live display.
    int getEffectiveLatencySamples() const noexcept;

    // For the tuner: copies the most recent 'numToCopy' raw input samples
    // in chronological order into dest (call from the message thread). The slight
    // contention with the audio thread is acceptable for the tuner.
    void copyRecentInput (float* dest, int numToCopy) const noexcept;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateParametersFromApvts() noexcept;
    void updateVocalFromApvts() noexcept;
    void processGuitar (juce::AudioBuffer<float>& buffer) noexcept;
    void processVocal  (juce::AudioBuffer<float>& buffer) noexcept;

    std::atomic<int> appMode       { (int) AppMode::none };
    std::atomic<int> guitarInputCh { 0 };
    std::atomic<int> vocalInputCh  { 0 };

    //==============================================================================
    // DSP modules — guitar signal chain
    NoiseGate     gate;
    Overdrive     overdrive;
    PitchShifter  pitchShifter;
    NamProcessor  nam;
    CabConvolver  cab;
    Equalizer     eq;

    // DSP modules — vocals signal chain
    Autotune      autotune;   // runs (mono) before the VoiceChain
    VoiceChain    vocal;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 96000 };
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;

    juce::SmoothedValue<float> inputGain  { 1.0f };
    juce::SmoothedValue<float> outputGain { 1.0f };

    juce::SmoothedValue<float> delayMixSmoothed      { 0.0f };
    juce::SmoothedValue<float> delayFeedbackSmoothed { 0.0f };

    // Mono work buffer (the guitar signal chain is mono up to the NAM).
    juce::AudioBuffer<float> monoBuffer;

    double currentSampleRate { 48000.0 };
    int    currentBlockSize  { 0 };

    // INPUT meter: smoothed peak of the signal entering the active chain (0..1+).
    std::atomic<float> inputLevel { 0.0f };
    float inputLevelEnv { 0.0f };   // audio-thread envelope (fast rise, slow fall)
    void  updateInputMeter (const float* samples, int numSamples, float gainLin) noexcept;

    // Tuner: circular buffer for the raw (pre-gain) mono input.
    static constexpr int tunerRingSize = 16384;
    std::vector<float> tunerRing;
    std::atomic<int>   tunerWrite { 0 };

    // Cached parameter pointers
    std::atomic<float>* pInputGain   { nullptr };
    std::atomic<float>* pOutputGain  { nullptr };
    std::atomic<float>* pGateOn      { nullptr };
    std::atomic<float>* pGateThresh  { nullptr };
    std::atomic<float>* pPitchOn     { nullptr };
    std::atomic<float>* pPitchSemis  { nullptr };
    std::atomic<float>* pPitchGrain  { nullptr };
    std::atomic<float>* pDriveOn     { nullptr };
    std::atomic<float>* pDriveAmt    { nullptr };
    std::atomic<float>* pDriveTone   { nullptr };
    std::atomic<float>* pDriveLevel  { nullptr };
    std::atomic<float>* pNamOn       { nullptr };
    std::atomic<float>* pCabOn       { nullptr };
    std::atomic<float>* pDelayOn     { nullptr };
    std::atomic<float>* pDelayTime   { nullptr };
    std::atomic<float>* pDelayFb     { nullptr };
    std::atomic<float>* pDelayMix    { nullptr };
    std::atomic<float>* pReverbOn    { nullptr };
    std::atomic<float>* pReverbAmt   { nullptr };
    std::atomic<float>* pEqOn        { nullptr };
    std::array<std::atomic<float>*, Equalizer::numBands> pEqBands { };

    // Vocals parameter pointers
    std::atomic<float>* pVocGain       { nullptr };
    std::atomic<float>* pVocGateOn     { nullptr };
    std::atomic<float>* pVocGateThresh { nullptr };
    std::atomic<float>* pVocWarmthOn   { nullptr };
    std::atomic<float>* pVocWarmth     { nullptr };
    std::atomic<float>* pVocCompOn     { nullptr };
    std::atomic<float>* pVocCompThresh { nullptr };
    std::atomic<float>* pVocCompRatio  { nullptr };
    std::atomic<float>* pVocAirOn      { nullptr };
    std::atomic<float>* pVocAir        { nullptr };
    std::atomic<float>* pVocDelayOn    { nullptr };
    std::atomic<float>* pVocDelayTime  { nullptr };
    std::atomic<float>* pVocDelayMix   { nullptr };
    std::atomic<float>* pVocReverbOn   { nullptr };
    std::atomic<float>* pVocReverbMix  { nullptr };
    std::atomic<float>* pVocAutoOn     { nullptr };
    std::atomic<float>* pVocAutoAmount { nullptr };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiveDspProcessor)
};
