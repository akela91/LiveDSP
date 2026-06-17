#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AppPaths.h"

//==============================================================================
LiveDspProcessor::LiveDspProcessor()
    : juce::AudioProcessor (BusesProperties()
        // Stereo input: the standalone provides the interface's 2 channels
        // (Input 1+2); processBlock mixes the guitar signal chain down to mono.
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pInputGain  = apvts.getRawParameterValue ("inputGain");
    pOutputGain = apvts.getRawParameterValue ("outputGain");
    pGateOn     = apvts.getRawParameterValue ("gateOn");
    pGateThresh = apvts.getRawParameterValue ("gateThreshold");
    pPitchOn    = apvts.getRawParameterValue ("pitchOn");
    pPitchSemis = apvts.getRawParameterValue ("pitchSemitones");
    pPitchLat   = apvts.getRawParameterValue ("pitchLatency");
    pPitchEngine = apvts.getRawParameterValue ("pitchEngine");
    pPitchLiveQ  = apvts.getRawParameterValue ("pitchLiveQuality");
    pDriveOn    = apvts.getRawParameterValue ("driveOn");
    pDriveAmt   = apvts.getRawParameterValue ("driveAmount");
    pDriveTone  = apvts.getRawParameterValue ("driveTone");
    pDriveLevel = apvts.getRawParameterValue ("driveLevel");
    pNamOn      = apvts.getRawParameterValue ("namOn");
    pCabOn      = apvts.getRawParameterValue ("cabOn");
    pDelayOn    = apvts.getRawParameterValue ("delayOn");
    pDelayTime  = apvts.getRawParameterValue ("delayTime");
    pDelayFb    = apvts.getRawParameterValue ("delayFeedback");
    pDelayMix   = apvts.getRawParameterValue ("delayMix");
    pReverbOn   = apvts.getRawParameterValue ("reverbOn");
    pReverbAmt  = apvts.getRawParameterValue ("reverbAmount");
    pEqOn       = apvts.getRawParameterValue ("eqOn");
    for (int i = 0; i < Equalizer::numBands; ++i)
        pEqBands[(size_t) i] = apvts.getRawParameterValue ("eqBand" + juce::String (i));

    pVocGain       = apvts.getRawParameterValue ("vocGain");
    pVocGateOn     = apvts.getRawParameterValue ("vocGateOn");
    pVocGateThresh = apvts.getRawParameterValue ("vocGateThresh");
    pVocWarmthOn   = apvts.getRawParameterValue ("vocWarmthOn");
    pVocWarmth     = apvts.getRawParameterValue ("vocWarmth");
    pVocCompOn     = apvts.getRawParameterValue ("vocCompOn");
    pVocCompThresh = apvts.getRawParameterValue ("vocCompThresh");
    pVocCompRatio  = apvts.getRawParameterValue ("vocCompRatio");
    pVocAirOn      = apvts.getRawParameterValue ("vocAirOn");
    pVocAir        = apvts.getRawParameterValue ("vocAir");
    pVocDelayOn    = apvts.getRawParameterValue ("vocDelayOn");
    pVocDelayTime  = apvts.getRawParameterValue ("vocDelayTime");
    pVocDelayMix   = apvts.getRawParameterValue ("vocDelayMix");
    pVocReverbOn   = apvts.getRawParameterValue ("vocReverbOn");
    pVocReverbMix  = apvts.getRawParameterValue ("vocReverbMix");

    // Live reconfiguration of the pitch latency (on the message thread).
    apvts.addParameterListener ("pitchLatency", this);
    // On an engine switch, the pitch shifter state must be reset on the message thread.
    apvts.addParameterListener ("pitchEngine", this);
    // Live reconfiguration of the RB Live quality profile (on the message thread).
    apvts.addParameterListener ("pitchLiveQuality", this);

    // Development convenience: load the first NAM model and IR from the default
    // models/ folder ALREADY in the constructor (before the editor), so that the
    // status is displayed correctly and it produces sound immediately. loadModel here
    // resets with the member-default sample rate; prepareToPlay later resets again
    // with the real sample rate.
    if (auto modelsDir = livedsp::getModelsDir(); modelsDir.isDirectory())
    {
        auto namFiles = modelsDir.findChildFiles (juce::File::findFiles, true, "*.nam");
        if (! namFiles.isEmpty())
        {
            // Preferred default model; if not found, the first one.
            const juce::String preferred = "FR OD808 MBDR MW Red Mdn - 1 - 4FB LR SM57a";
            juce::File toLoad = namFiles.getFirst();
            for (const auto& f : namFiles)
                if (f.getFileNameWithoutExtension() == preferred) { toLoad = f; break; }
            nam.loadModel (toLoad);
        }

        auto irFiles = modelsDir.findChildFiles (juce::File::findFiles, true, "*.wav");
        if (! irFiles.isEmpty())
            cab.loadIR (irFiles.getFirst());   // loaded, but the Cab is OFF by default
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout LiveDspProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto db = [] (float lo, float hi)
    {
        return NormalisableRange<float> { lo, hi, 0.1f };
    };

    // I/O
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "inputGain", 1 },
        "Input Gain", db (-24.0f, 24.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "outputGain", 1 },
        "Output Gain", db (-24.0f, 24.0f), 0.0f));

    // Noise Gate
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "gateOn", 1 }, "Gate On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "gateThreshold", 1 },
        "Gate Threshold", NormalisableRange<float> { -80.0f, 0.0f, 0.1f }, -55.0f));

    // Pitch (Transpose)
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "pitchOn", 1 }, "Pitch On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "pitchSemitones", 1 },
        "Transpose", NormalisableRange<float> { -12.0f, 12.0f, 1.0f }, 0.0f));
    // Pitch latency/quality: lower = lower latency (more artifacts),
    // higher = better quality (higher latency). Reconfigures the engine live.
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "pitchLatency", 1 },
        "Pitch Latency", NormalisableRange<float> { 8.0f, 80.0f, 1.0f }, 40.0f));
    // Pitch engine (default: RubberBand): 0 = Signalsmith (adjustable
    // latency), 1 = Rubber Band Stretcher (R3 'Finer', good polyphonic quality),
    // 2 = Rubber Band LiveShifter (v4, lowest latency). The 'pitchLatency'
    // knob only affects the Signalsmith engine.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "pitchEngine", 1 },
        "Pitch Engine", StringArray { "Signalsmith", "RubberBand", "RB Live" }, 2));
    // RB Live quality profile (only affects the RB Live engine): Fast = lowest
    // latency (short window), Fine = better quality (medium window + formant).
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "pitchLiveQuality", 1 },
        "RB Live Quality", StringArray { "Fast", "Fine" }, 0));

    // Overdrive
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "driveOn", 1 }, "Drive On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "driveAmount", 1 },
        "Drive", NormalisableRange<float> { 0.0f, 36.0f, 0.1f }, 12.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "driveTone", 1 },
        "Tone", NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "driveLevel", 1 },
        "Drive Level", NormalisableRange<float> { -24.0f, 6.0f, 0.1f }, 0.0f));

    // Amp (NAM)
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "namOn", 1 }, "Amp On", true));

    // EQ (low-latency, 9-band graphic IIR) — after the Cab
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "eqOn", 1 }, "EQ On", false));
    for (int i = 0; i < Equalizer::numBands; ++i)
    {
        const float f = Equalizer::frequencies[(size_t) i];
        const juce::String label = f >= 1000.0f
            ? "EQ " + juce::String ((int) (f / 1000.0f)) + " kHz"
            : "EQ " + juce::String ((int) f) + " Hz";
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "eqBand" + juce::String (i), 1 }, label, db (-15.0f, 15.0f), 0.0f));
    }

    // Cab IR (OFF by default because of the Full Rig models)
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "cabOn", 1 }, "Cab On", false));

    // Delay
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "delayOn", 1 }, "Delay On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "delayTime", 1 },
        "Delay Time", NormalisableRange<float> { 20.0f, 1500.0f, 1.0f }, 350.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "delayFeedback", 1 },
        "Delay Feedback", NormalisableRange<float> { 0.0f, 0.95f, 0.01f }, 0.35f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "delayMix", 1 },
        "Delay Mix", NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.25f));

    // Reverb
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "reverbOn", 1 }, "Reverb On", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "reverbAmount", 1 },
        "Reverb", NormalisableRange<float> { 0.0f, 1.0f, 0.01f }, 0.25f));

    //==========================================================================
    // VOCALS (Vocal) mode parameters
    //==========================================================================
    // Input Gain: digital preamp for the quiet dynamic microphone (0..+24 dB).
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocGain", 1 },
        "Vocal Gain", NormalisableRange<float> { 0.0f, 24.0f, 0.1f }, 6.0f));

    // Noise Gate after the low-cut (ratio 10:1 / attack 2 ms / release 150 ms FIXED).
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "vocGateOn", 1 }, "Vocal Gate On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocGateThresh", 1 },
        "Vocal Gate", NormalisableRange<float> { -80.0f, -20.0f, 0.1f }, -55.0f));

    // Warmth / Saturation (tanh WaveShaper); WARMTH = drive 1.0..3.0 (subtle).
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "vocWarmthOn", 1 }, "Vocal Warmth On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocWarmth", 1 },
        "Vocal Warmth", NormalisableRange<float> { 1.0f, 3.0f, 0.01f }, 1.2f));

    // Compressor (Attack 5 ms / Release 100 ms FIXED in the engine).
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "vocCompOn", 1 }, "Vocal Comp On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocCompThresh", 1 },
        "Vocal Comp Threshold", NormalisableRange<float> { -40.0f, 0.0f, 0.1f }, -18.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocCompRatio", 1 },
        "Vocal Comp Ratio", NormalisableRange<float> { 1.0f, 10.0f, 0.1f }, 3.0f));

    // High-Shelf "air/presence" (6 kHz FIXED, 0..+12 dB boost).
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "vocAirOn", 1 }, "Vocal Air On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocAir", 1 },
        "Vocal Air", NormalisableRange<float> { 0.0f, 12.0f, 0.1f }, 3.0f));

    // Delay (feedback 0.3 FIXED); TIME 50..500 ms, MIX 0..50%.
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "vocDelayOn", 1 }, "Vocal Delay On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocDelayTime", 1 },
        "Vocal Delay Time", NormalisableRange<float> { 50.0f, 500.0f, 1.0f }, 200.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocDelayMix", 1 },
        "Vocal Delay Mix", NormalisableRange<float> { 0.0f, 50.0f, 1.0f }, 12.0f));

    // Reverb Wet/Dry mix (0..100%); room/damp FIXED in the engine.
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "vocReverbOn", 1 }, "Vocal Reverb On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocReverbMix", 1 },
        "Vocal Reverb Mix", NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 20.0f));

    return layout;
}

//==============================================================================
void LiveDspProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    juce::dsp::ProcessSpec monoSpec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    juce::dsp::ProcessSpec stereoSpec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    gate.prepare (monoSpec);
    overdrive.prepare (monoSpec);
    if (pPitchLat != nullptr)
        pitchShifter.setBlockMs (pPitchLat->load());
    pitchShifter.prepare (monoSpec);
    nam.prepare (monoSpec);

    cab.prepare (stereoSpec);
    eq.prepare (stereoSpec);

    // The vocals chain works on a stereo block (mono microphone on both channels).
    vocal.prepare (stereoSpec);

    // Max delay length aligned to the actual sample rate (max parameter 1500 ms + headroom).
    delayLine.setMaximumDelayInSamples ((int) (sampleRate * 2.0) + samplesPerBlock);
    delayLine.prepare (stereoSpec);
    delayLine.reset();

    reverb.prepare (stereoSpec);
    reverb.reset();

    inputGain.reset  (sampleRate, 0.02);
    outputGain.reset (sampleRate, 0.02);
    delayMixSmoothed.reset (sampleRate, 0.02);
    delayFeedbackSmoothed.reset (sampleRate, 0.02);

    monoBuffer.setSize (1, samplesPerBlock, false, false, true);

    tunerRing.assign ((size_t) tunerRingSize, 0.0f);
    tunerWrite.store (0, std::memory_order_relaxed);

    // Report Pitch + (Cab IR zero-latency = 0) latency to the host.
    setLatencySamples (pitchShifter.getLatencySamples());
}

bool LiveDspProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono or stereo input, stereo output.
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::stereo())
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void LiveDspProcessor::updateParametersFromApvts() noexcept
{
    inputGain.setTargetValue  (juce::Decibels::decibelsToGain (pInputGain->load()));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (pOutputGain->load()));

    gate.setEnabled   (pGateOn->load() > 0.5f);
    gate.setThreshold (pGateThresh->load());

    pitchShifter.setEnabled   (pPitchOn->load() > 0.5f);
    pitchShifter.setSemitones (pPitchSemis->load());
    pitchShifter.setEngine      ((int) pPitchEngine->load());
    pitchShifter.setLiveQuality ((int) pPitchLiveQ->load());

    overdrive.setEnabled (pDriveOn->load() > 0.5f);
    overdrive.setDrive   (pDriveAmt->load());
    overdrive.setTone    (pDriveTone->load());
    overdrive.setLevel   (pDriveLevel->load());

    nam.setEnabled (pNamOn->load() > 0.5f);
    cab.setEnabled (pCabOn->load() > 0.5f);

    eq.setEnabled (pEqOn->load() > 0.5f);
    float eqGains[Equalizer::numBands];
    for (int i = 0; i < Equalizer::numBands; ++i)
        eqGains[i] = pEqBands[(size_t) i]->load();
    eq.setGains (eqGains);

    delayMixSmoothed.setTargetValue      (pDelayOn->load() > 0.5f ? pDelayMix->load() : 0.0f);
    delayFeedbackSmoothed.setTargetValue (pDelayFb->load());

    const float rv = pReverbOn->load() > 0.5f ? pReverbAmt->load() : 0.0f;
    reverbParams.wetLevel = rv;
    reverbParams.dryLevel = 1.0f - 0.5f * rv;
    reverbParams.roomSize = juce::jmap (rv, 0.0f, 1.0f, 0.3f, 0.9f);
    reverbParams.width    = 1.0f;
    reverb.setParameters (reverbParams);
}

void LiveDspProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Called from the standalone UI on the message thread -> safe to reconfigure.
    if (parameterID == "pitchLatency")
    {
        pitchShifter.setBlockMs ((double) newValue);
        pitchShifter.reconfigure();
    }
    else if (parameterID == "pitchEngine")
    {
        // Clean switch: clear the selected engine + FIFO state.
        pitchShifter.setEngine ((int) newValue);
        pitchShifter.reset();
    }
    else if (parameterID == "pitchLiveQuality")
    {
        // RB Live profile switch: the shifter must be rebuilt (the window option is
        // decided in the constructor), so we reconfigure it on the message thread.
        pitchShifter.setLiveQuality ((int) newValue);
        pitchShifter.reconfigureLive();
    }
}

void LiveDspProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    switch ((AppMode) appMode.load())
    {
        case AppMode::guitar: processGuitar (buffer); return;
        case AppMode::vocal:  processVocal  (buffer); return;
        case AppMode::none:
        default:
            // Landing screen: no processing, silent output.
            buffer.clear();
            return;
    }
}

void LiveDspProcessor::processGuitar (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numOut     = buffer.getNumChannels();
    const int numIn      = getTotalNumInputChannels();

    updateParametersFromApvts();

    // --- Build the mono signal chain --------------------------------------
    // We take ONLY the selected input channel (we do not mix all the
    // inputs together) — so the microphone does not leak into the guitar chain.
    const int gCh = juce::jlimit (0, juce::jmax (0, numIn - 1), guitarInputCh.load());
    monoBuffer.setSize (1, numSamples, false, false, true);
    monoBuffer.clear();
    float* mono = monoBuffer.getWritePointer (0);
    if (numIn > 0)
        monoBuffer.copyFrom (0, 0, buffer, gCh, 0, numSamples);

    // Capture the raw (pre-gain) input for the tuner.
    if (! tunerRing.empty())
    {
        int w = tunerWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            tunerRing[(size_t) w] = mono[i];
            if (++w >= tunerRingSize) w = 0;
        }
        tunerWrite.store (w, std::memory_order_relaxed);
    }

    // 1) Input Gain
    inputGain.applyGain (mono, numSamples);

    // 2) Noise Gate
    gate.process (mono, numSamples);

    // 3) Transpose / Pitch shift
    pitchShifter.process (mono, numSamples);

    // 4) Overdrive / Tube Screamer
    overdrive.process (mono, numSamples);

    // 5) Amp (NAM)
    nam.process (mono, numSamples);

    // If the Amp is switched off, we do NOT let the raw (clean) signal through:
    // the amp is the sound source, so without the amp the output is silent (not "direct monitor").
    if (pNamOn->load() <= 0.5f)
        monoBuffer.clear();

    // --- Mono -> Stereo ---------------------------------------------------
    for (int ch = 0; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, mono, numSamples);

    juce::dsp::AudioBlock<float> block (buffer);

    // 6) Cab IR (zero-latency, bypassable)
    cab.process (block);

    // 6b) EQ (low-latency IIR, after the Cab)
    eq.process (block);

    // 7) Delay (stereo, with feedback)
    {
        delayLine.setDelay ((float) (pDelayTime->load() * 0.001 * currentSampleRate));
        for (int ch = 0; ch < numOut; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const float mix = delayMixSmoothed.getNextValue();
                const float fb  = delayFeedbackSmoothed.getNextValue();
                const float in  = data[i];
                const float delayed = delayLine.popSample (ch);
                delayLine.pushSample (ch, in + delayed * fb);
                data[i] = in + delayed * mix;
            }
        }
    }

    // 8) Reverb (stereo)
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

    // 9) Output Gain
    for (int ch = 0; ch < numOut; ++ch)
        outputGain.applyGain (buffer.getWritePointer (ch), numSamples);
}

//==============================================================================
void LiveDspProcessor::updateVocalFromApvts() noexcept
{
    vocal.setInputGainDb   (pVocGain->load());
    vocal.setGateThreshold (pVocGateThresh->load());
    vocal.setWarmth        (pVocWarmth->load());
    vocal.setCompThreshold (pVocCompThresh->load());
    vocal.setCompRatio     (pVocCompRatio->load());
    vocal.setAirDb         (pVocAir->load());
    vocal.setDelayTimeMs   (pVocDelayTime->load());
    vocal.setDelayMix      (pVocDelayMix->load() * 0.01f);    // % -> 0..0.5
    vocal.setReverbMix     (pVocReverbMix->load() * 0.01f);   // % -> 0..1

    vocal.setGateEnabled   (pVocGateOn->load()   > 0.5f);
    vocal.setWarmthEnabled (pVocWarmthOn->load() > 0.5f);
    vocal.setCompEnabled   (pVocCompOn->load()   > 0.5f);
    vocal.setAirEnabled    (pVocAirOn->load()    > 0.5f);
    vocal.setDelayEnabled  (pVocDelayOn->load()  > 0.5f);
    vocal.setReverbEnabled (pVocReverbOn->load() > 0.5f);
}

void LiveDspProcessor::processVocal (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numOut     = buffer.getNumChannels();
    const int numIn      = getTotalNumInputChannels();

    updateVocalFromApvts();

    // Mono microphone signal: ONLY the selected input channel (which Scarlett
    // input the mic is on), then we copy it to every output.
    const int vCh = juce::jlimit (0, juce::jmax (0, numIn - 1), vocalInputCh.load());
    monoBuffer.setSize (1, numSamples, false, false, true);
    monoBuffer.clear();
    float* mono = monoBuffer.getWritePointer (0);
    if (numIn > 0)
        monoBuffer.copyFrom (0, 0, buffer, vCh, 0, numSamples);

    for (int ch = 0; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, mono, numSamples);

    // Full vocals chain on a stereo block (Gain -> LowCut -> Comp -> Air -> Reverb -> Limiter).
    juce::dsp::AudioBlock<float> block (buffer);
    vocal.process (block);
}

//==============================================================================
int LiveDspProcessor::getEffectiveLatencySamples() const noexcept
{
    // The vocals chain is zero-latency throughout (IIR/comp/reverb/limiter).
    if ((AppMode) appMode.load() != AppMode::guitar)
        return 0;

    int s = 0;

    // The pitch shifter only adds latency when it is switched on AND there is a shift
    // (semitones != 0) — otherwise process() passes the signal through raw, without latency.
    if (pPitchOn != nullptr && pPitchOn->load() > 0.5f
        && pPitchSemis != nullptr && pPitchSemis->load() != 0.0f)
        s += pitchShifter.getLatencySamples();

    // The Cab IR runs in zero-latency mode (0), and the other modules are also 0 latency.
    return s;
}

//==============================================================================
void LiveDspProcessor::copyRecentInput (float* dest, int numToCopy) const noexcept
{
    if (tunerRing.empty() || numToCopy <= 0)
    {
        juce::FloatVectorOperations::clear (dest, numToCopy);
        return;
    }

    const int n = juce::jmin (numToCopy, tunerRingSize);
    int r = tunerWrite.load (std::memory_order_relaxed) - n;
    while (r < 0) r += tunerRingSize;

    for (int i = 0; i < n; ++i)
    {
        dest[i] = tunerRing[(size_t) r];
        if (++r >= tunerRingSize) r = 0;
    }
    // If more was requested than is available, zero out the remainder.
    for (int i = n; i < numToCopy; ++i)
        dest[i] = 0.0f;
}

//==============================================================================
juce::AudioProcessorEditor* LiveDspProcessor::createEditor()
{
    return new LiveDspEditor (*this);
}

void LiveDspProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        // Input channel selection per mode (not an APVTS param, but persistent).
        xml->setAttribute ("guitarInputCh", guitarInputCh.load());
        xml->setAttribute ("vocalInputCh",  vocalInputCh.load());
        copyXmlToBinary (*xml, destData);
    }
}

void LiveDspProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            guitarInputCh.store (xml->getIntAttribute ("guitarInputCh", 0));
            vocalInputCh.store  (xml->getIntAttribute ("vocalInputCh",  0));
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
}

//==============================================================================
// Entry point of the standalone/plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LiveDspProcessor();
}
