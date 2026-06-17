#pragma once

#include <JuceHeader.h>

#include "dsp/NoiseGate.h"
#include "dsp/Overdrive.h"
#include "dsp/PitchShifter.h"
#include "dsp/NamProcessor.h"
#include "dsp/CabConvolver.h"
#include "dsp/Equalizer.h"

/**
    guitarDSP — standalone gitár amp-szimulátor processzor.

    Jelút:
      In -> [InputGain] -> [Gate] -> [Pitch] -> [Overdrive] -> [NAM]
         -> (mono->stereo) -> [Cab IR] -> [Delay] -> [Reverb] -> [OutputGain] -> Out
*/
class GuitarDspProcessor  : public juce::AudioProcessor
{
public:
    GuitarDspProcessor();
    ~GuitarDspProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "guitarDSP"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; } // reverb/delay farok

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int sizeInBytes) override;

    //==============================================================================
    // Modell-/IR-betöltés (üzenetszálról hívandó).
    bool loadNamModel (const juce::File& namFile) { return nam.loadModel (namFile); }
    bool loadCabIr    (const juce::File& irFile)  { return cab.loadIR (irFile); }
    NamProcessor& getNam()       { return nam; }
    CabConvolver& getCab()       { return cab; }

    int getCurrentBlockSize() const noexcept { return currentBlockSize; }

    // A hangolóhoz: a legfrissebb 'numToCopy' nyers bemeneti mintát másolja
    // időrendben a dest-be (üzenetszálról hívandó). A hangszállal való enyhe
    // versengés a hangolónál elfogadható.
    void copyRecentInput (float* dest, int numToCopy) const noexcept;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateParametersFromApvts() noexcept;

    //==============================================================================
    // DSP modulok
    NoiseGate     gate;
    Overdrive     overdrive;
    PitchShifter  pitchShifter;
    NamProcessor  nam;
    CabConvolver  cab;
    Equalizer     eq;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 96000 };
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;

    juce::SmoothedValue<float> inputGain  { 1.0f };
    juce::SmoothedValue<float> outputGain { 1.0f };

    juce::SmoothedValue<float> delayMixSmoothed      { 0.0f };
    juce::SmoothedValue<float> delayFeedbackSmoothed { 0.0f };

    // Mono munkapuffer (a gitár jelút mono a NAM-ig).
    juce::AudioBuffer<float> monoBuffer;

    double currentSampleRate { 48000.0 };
    int    currentBlockSize  { 0 };

    // Tuner: körkörös puffer a nyers (pre-gain) monó bemenetről.
    static constexpr int tunerRingSize = 16384;
    std::vector<float> tunerRing;
    std::atomic<int>   tunerWrite { 0 };

    // Gyorsított paraméter-pointerek
    std::atomic<float>* pInputGain   { nullptr };
    std::atomic<float>* pOutputGain  { nullptr };
    std::atomic<float>* pGateOn      { nullptr };
    std::atomic<float>* pGateThresh  { nullptr };
    std::atomic<float>* pPitchOn     { nullptr };
    std::atomic<float>* pPitchSemis  { nullptr };
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDspProcessor)
};
