#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GuitarDspProcessor::GuitarDspProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    pInputGain  = apvts.getRawParameterValue ("inputGain");
    pOutputGain = apvts.getRawParameterValue ("outputGain");
    pGateOn     = apvts.getRawParameterValue ("gateOn");
    pGateThresh = apvts.getRawParameterValue ("gateThreshold");
    pPitchOn    = apvts.getRawParameterValue ("pitchOn");
    pPitchSemis = apvts.getRawParameterValue ("pitchSemitones");
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

    // Fejlesztői kényelem: az alapértelmezett models/ mappából betöltjük az
    // első NAM modellt és IR-t MÁR a konstruktorban (az editor előtt), hogy a
    // státusz helyesen jelenjen meg és azonnal szóljon. A loadModel itt a
    // tag-alapértelmezett SR-rel Reset-el; a prepareToPlay később újra Reset-el
    // a valódi mintavételi frekvenciával.
   #if defined (GUITARDSP_DEFAULT_MODELS_DIR)
    if (juce::File modelsDir { GUITARDSP_DEFAULT_MODELS_DIR }; modelsDir.isDirectory())
    {
        auto namFiles = modelsDir.findChildFiles (juce::File::findFiles, true, "*.nam");
        if (! namFiles.isEmpty())
            nam.loadModel (namFiles.getFirst());

        auto irFiles = modelsDir.findChildFiles (juce::File::findFiles, true, "*.wav");
        if (! irFiles.isEmpty())
            cab.loadIR (irFiles.getFirst());   // betöltve, de a Cab alapból OFF
    }
   #endif
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout GuitarDspProcessor::createParameterLayout()
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

    // Cab IR (alapból OFF a Full Rig modellek miatt)
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

    return layout;
}

//==============================================================================
void GuitarDspProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec monoSpec { sampleRate, (juce::uint32) samplesPerBlock, 1 };
    juce::dsp::ProcessSpec stereoSpec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    gate.prepare (monoSpec);
    overdrive.prepare (monoSpec);
    pitchShifter.prepare (monoSpec);
    nam.prepare (monoSpec);

    cab.prepare (stereoSpec);

    // A delay max hossza a tényleges SR-hez igazítva (max paraméter 1500 ms + tartalék).
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

    // Pitch + (Cab IR zero-latency = 0) latencia jelzése a hostnak.
    setLatencySamples (pitchShifter.getLatencySamples());
}

bool GuitarDspProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono vagy sztereó bemenet, sztereó kimenet.
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::stereo())
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void GuitarDspProcessor::updateParametersFromApvts() noexcept
{
    inputGain.setTargetValue  (juce::Decibels::decibelsToGain (pInputGain->load()));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (pOutputGain->load()));

    gate.setEnabled   (pGateOn->load() > 0.5f);
    gate.setThreshold (pGateThresh->load());

    pitchShifter.setEnabled   (pPitchOn->load() > 0.5f);
    pitchShifter.setSemitones (pPitchSemis->load());

    overdrive.setEnabled (pDriveOn->load() > 0.5f);
    overdrive.setDrive   (pDriveAmt->load());
    overdrive.setTone    (pDriveTone->load());
    overdrive.setLevel   (pDriveLevel->load());

    nam.setEnabled (pNamOn->load() > 0.5f);
    cab.setEnabled (pCabOn->load() > 0.5f);

    delayMixSmoothed.setTargetValue      (pDelayOn->load() > 0.5f ? pDelayMix->load() : 0.0f);
    delayFeedbackSmoothed.setTargetValue (pDelayFb->load());

    const float rv = pReverbOn->load() > 0.5f ? pReverbAmt->load() : 0.0f;
    reverbParams.wetLevel = rv;
    reverbParams.dryLevel = 1.0f - 0.5f * rv;
    reverbParams.roomSize = juce::jmap (rv, 0.0f, 1.0f, 0.3f, 0.9f);
    reverbParams.width    = 1.0f;
    reverb.setParameters (reverbParams);
}

void GuitarDspProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numOut     = buffer.getNumChannels();

    updateParametersFromApvts();

    // --- Mono jelút felépítése (a 0. csatorna a gitár) -------------------
    monoBuffer.setSize (1, numSamples, false, false, true);
    float* mono = monoBuffer.getWritePointer (0);
    monoBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);

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

    // --- Mono -> Stereo ---------------------------------------------------
    for (int ch = 0; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, mono, numSamples);

    juce::dsp::AudioBlock<float> block (buffer);

    // 6) Cab IR (zero-latency, bypassolható)
    cab.process (block);

    // 7) Delay (sztereó, visszacsatolással)
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

    // 8) Reverb (sztereó)
    {
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

    // 9) Output Gain
    for (int ch = 0; ch < numOut; ++ch)
        outputGain.applyGain (buffer.getWritePointer (ch), numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* GuitarDspProcessor::createEditor()
{
    return new GuitarDspEditor (*this);
}

void GuitarDspProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void GuitarDspProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// A standalone/plugin belépési pontja.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarDspProcessor();
}
