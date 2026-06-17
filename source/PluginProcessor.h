#pragma once

#include <JuceHeader.h>

#include "dsp/NoiseGate.h"
#include "dsp/Overdrive.h"
#include "dsp/PitchShifter.h"
#include "dsp/NamProcessor.h"
#include "dsp/CabConvolver.h"
#include "dsp/Equalizer.h"
#include "dsp/VocalChain.h"

/**
    guitarDSP — kétmódú standalone audio app: GITÁR amp-szimulátor + ÉNEK
    csatorna. A futás közben váltható 'appMode' dönti el, melyik jelút fut.
    Az induló képernyő (Landing) választatja ki a módot (0 = nincs választva).

    Gitár jelút:
      In -> [InputGain] -> [Gate] -> [Pitch] -> [Overdrive] -> [NAM]
         -> (mono->stereo) -> [Cab IR] -> [EQ] -> [Delay] -> [Reverb] -> [OutputGain] -> Out

    Ének jelút (mono mikrofon -> sztereó, VocalChain ProcessorChain):
      In -> [Gain] -> [LowCut 90Hz] -> [Comp] -> [Air 6kHz] -> [Reverb] -> [Limiter] -> Out
*/
class GuitarDspProcessor  : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener
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

    const juce::String getName() const override { return "LiveDSP"; }
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
    NoiseGate&    getGate()      { return gate; }

    // Vokál kijelző-mérők a UI LED-ekhez (0..1).
    float getVocalGateGain()      const noexcept { return vocal.getGateGain(); }
    float getVocalCompReduction() const noexcept { return vocal.getCompReduction(); }

    int getCurrentBlockSize() const noexcept { return currentBlockSize; }

    //==============================================================================
    // App-mód: 0 = nincs választva (Landing), 1 = Gitár, 2 = Ének. Futás közben
    // váltható (a Landing képernyőről és a nézetek "menü" gombjáról). Nem kerül
    // az állapotba — minden indításkor a Landing képernyő jön be.
    enum class AppMode { none = 0, guitar = 1, vocal = 2 };
    int  getAppMode() const noexcept { return appMode.load(); }
    void setAppMode (int m) noexcept { appMode.store (m); }

    //==============================================================================
    // Bemeneti csatorna kiválasztása MÓDONKÉNT külön (a Scarlett több bemenete
    // közül melyiket figyelje az adott lánc). 0-alapú index; a processBlock CSAK
    // ezt az egy csatornát veszi (nem keveri össze a bemeneteket), így a mikrofon
    // nem szivárog be a gitár láncba és fordítva. Perzisztálódik az állapotban.
    int  getGuitarInputCh() const noexcept { return guitarInputCh.load(); }
    int  getVocalInputCh()  const noexcept { return vocalInputCh.load(); }
    void setGuitarInputCh (int ch) noexcept { guitarInputCh.store (juce::jmax (0, ch)); }
    void setVocalInputCh  (int ch) noexcept { vocalInputCh.store  (juce::jmax (0, ch)); }

    // A jelenleg AKTÍV modulok által hozzáadott latencia (mintában) — dinamikus,
    // követi a kapcsolók állását (pl. pitch be/ki). Az élő kijelzőhöz.
    int getEffectiveLatencySamples() const noexcept;

    // A hangolóhoz: a legfrissebb 'numToCopy' nyers bemeneti mintát másolja
    // időrendben a dest-be (üzenetszálról hívandó). A hangszállal való enyhe
    // versengés a hangolónál elfogadható.
    void copyRecentInput (float* dest, int numToCopy) const noexcept;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateParametersFromApvts() noexcept;
    void updateVocalFromApvts() noexcept;
    void processGuitar (juce::AudioBuffer<float>& buffer) noexcept;
    void processVocal  (juce::AudioBuffer<float>& buffer) noexcept;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    std::atomic<int> appMode       { (int) AppMode::none };
    std::atomic<int> guitarInputCh { 0 };
    std::atomic<int> vocalInputCh  { 0 };

    //==============================================================================
    // DSP modulok — gitár jelút
    NoiseGate     gate;
    Overdrive     overdrive;
    PitchShifter  pitchShifter;
    NamProcessor  nam;
    CabConvolver  cab;
    Equalizer     eq;

    // DSP modul — ének jelút
    VocalChain    vocal;

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
    std::atomic<float>* pPitchLat    { nullptr };
    std::atomic<float>* pPitchEngine { nullptr };
    std::atomic<float>* pPitchLiveQ  { nullptr };
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

    // Ének paraméter-pointerek
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarDspProcessor)
};
