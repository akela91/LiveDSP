#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GuitarDspProcessor::GuitarDspProcessor()
    : juce::AudioProcessor (BusesProperties()
        // Sztereó bemenet: a standalone az interfész 2 csatornáját adja
        // (Input 1+2); a processBlock keveri monóba a gitár jelutat.
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
    pVocCompOn     = apvts.getRawParameterValue ("vocCompOn");
    pVocCompThresh = apvts.getRawParameterValue ("vocCompThresh");
    pVocCompRatio  = apvts.getRawParameterValue ("vocCompRatio");
    pVocAirOn      = apvts.getRawParameterValue ("vocAirOn");
    pVocAir        = apvts.getRawParameterValue ("vocAir");
    pVocReverbOn   = apvts.getRawParameterValue ("vocReverbOn");
    pVocReverbMix  = apvts.getRawParameterValue ("vocReverbMix");

    // A pitch-latencia élő újrakonfigurálása (üzenetszálon).
    apvts.addParameterListener ("pitchLatency", this);
    // Motorváltáskor a pitch shifter állapotát üzenetszálon resetelni kell.
    apvts.addParameterListener ("pitchEngine", this);
    // Az RB Live minőségprofil élő újrakonfigurálása (üzenetszálon).
    apvts.addParameterListener ("pitchLiveQuality", this);

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
        {
            // Preferált alapértelmezett modell; ha nincs meg, az első.
            const juce::String preferred = "FR OD808 MBDR MW Red Mdn - 1 - 4FB LR SM57a";
            juce::File toLoad = namFiles.getFirst();
            for (const auto& f : namFiles)
                if (f.getFileNameWithoutExtension() == preferred) { toLoad = f; break; }
            nam.loadModel (toLoad);
        }

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
    // Pitch latencia/minőség: kisebb = kisebb latencia (több műtermék),
    // nagyobb = jobb minőség (nagyobb latencia). Élőben átkonfigurálja a motort.
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "pitchLatency", 1 },
        "Pitch Latency", NormalisableRange<float> { 8.0f, 80.0f, 1.0f }, 40.0f));
    // Pitch motor (alapértelmezés: RubberBand): 0 = Signalsmith (állítható
    // latencia), 1 = Rubber Band Stretcher (R3 'Finer', jó polifonikus minőség),
    // 2 = Rubber Band LiveShifter (v4, legkisebb latencia). A 'pitchLatency'
    // knob csak a Signalsmith motorra hat.
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "pitchEngine", 1 },
        "Pitch Engine", StringArray { "Signalsmith", "RubberBand", "RB Live" }, 2));
    // RB Live minőségprofil (csak az RB Live motorra hat): Fast = legkisebb
    // latencia (rövid ablak), Fine = jobb minőség (közepes ablak + formant).
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

    // EQ (alacsony latenciás, 9-sávos grafikus IIR) — Cab után
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

    //==========================================================================
    // ÉNEK (Vocal) mód paraméterei
    //==========================================================================
    // Input Gain: digitális előerősítő a halk dinamikus mikrofonhoz (0..+24 dB).
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocGain", 1 },
        "Vocal Gain", NormalisableRange<float> { 0.0f, 24.0f, 0.1f }, 6.0f));

    // Compressor (Attack 5 ms / Release 100 ms FIX a motorban).
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "vocCompOn", 1 }, "Vocal Comp On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocCompThresh", 1 },
        "Vocal Comp Threshold", NormalisableRange<float> { -40.0f, 0.0f, 0.1f }, -18.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocCompRatio", 1 },
        "Vocal Comp Ratio", NormalisableRange<float> { 1.0f, 10.0f, 0.1f }, 3.0f));

    // High-Shelf "air/presence" (6 kHz FIX, 0..+12 dB emelés).
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "vocAirOn", 1 }, "Vocal Air On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocAir", 1 },
        "Vocal Air", NormalisableRange<float> { 0.0f, 12.0f, 0.1f }, 3.0f));

    // Reverb Wet/Dry mix (0..100%); room/damp FIX a motorban.
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "vocReverbOn", 1 }, "Vocal Reverb On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "vocReverbMix", 1 },
        "Vocal Reverb Mix", NormalisableRange<float> { 0.0f, 100.0f, 1.0f }, 20.0f));

    return layout;
}

//==============================================================================
void GuitarDspProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
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

    // Ének lánc sztereó blokkon dolgozik (mono mikrofon mindkét csatornán).
    vocal.prepare (stereoSpec);

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

    tunerRing.assign ((size_t) tunerRingSize, 0.0f);
    tunerWrite.store (0, std::memory_order_relaxed);

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

void GuitarDspProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Standalone UI-ból üzenetszálon hívódik -> biztonságos újrakonfigurálni.
    if (parameterID == "pitchLatency")
    {
        pitchShifter.setBlockMs ((double) newValue);
        pitchShifter.reconfigure();
    }
    else if (parameterID == "pitchEngine")
    {
        // Tiszta váltás: a kiválasztott motor + FIFO állapotát kiürítjük.
        pitchShifter.setEngine ((int) newValue);
        pitchShifter.reset();
    }
    else if (parameterID == "pitchLiveQuality")
    {
        // RB Live profilváltás: a shiftert újra kell építeni (ablakopció a
        // konstruktorban dől el), ezért üzenetszálon reconfiguráljuk.
        pitchShifter.setLiveQuality ((int) newValue);
        pitchShifter.reconfigureLive();
    }
}

void GuitarDspProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    switch ((AppMode) appMode.load())
    {
        case AppMode::guitar: processGuitar (buffer); return;
        case AppMode::vocal:  processVocal  (buffer); return;
        case AppMode::none:
        default:
            // Landing képernyő: nincs feldolgozás, néma kimenet.
            buffer.clear();
            return;
    }
}

void GuitarDspProcessor::processGuitar (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numOut     = buffer.getNumChannels();
    const int numIn      = getTotalNumInputChannels();

    updateParametersFromApvts();

    // --- Mono jelút felépítése --------------------------------------------
    // CSAK a kiválasztott bemeneti csatornát vesszük (nem keverünk össze minden
    // bemenetet) — így a mikrofon nem szivárog be a gitár láncba.
    const int gCh = juce::jlimit (0, juce::jmax (0, numIn - 1), guitarInputCh.load());
    monoBuffer.setSize (1, numSamples, false, false, true);
    monoBuffer.clear();
    float* mono = monoBuffer.getWritePointer (0);
    if (numIn > 0)
        monoBuffer.copyFrom (0, 0, buffer, gCh, 0, numSamples);

    // Nyers (pre-gain) bemenet rögzítése a hangolóhoz.
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

    // Ha az Amp ki van kapcsolva, NEM engedjük át a nyers (clean) jelet:
    // az amp a hangforrás, így amp nélkül néma a kimenet (nem "direct monitor").
    if (pNamOn->load() <= 0.5f)
        monoBuffer.clear();

    // --- Mono -> Stereo ---------------------------------------------------
    for (int ch = 0; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, mono, numSamples);

    juce::dsp::AudioBlock<float> block (buffer);

    // 6) Cab IR (zero-latency, bypassolható)
    cab.process (block);

    // 6b) EQ (alacsony latenciás IIR, Cab után)
    eq.process (block);

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
void GuitarDspProcessor::updateVocalFromApvts() noexcept
{
    vocal.setInputGainDb  (pVocGain->load());
    vocal.setCompThreshold (pVocCompThresh->load());
    vocal.setCompRatio     (pVocCompRatio->load());
    vocal.setAirDb         (pVocAir->load());
    vocal.setReverbMix     (pVocReverbMix->load() * 0.01f);   // % -> 0..1

    vocal.setCompEnabled   (pVocCompOn->load()   > 0.5f);
    vocal.setAirEnabled    (pVocAirOn->load()    > 0.5f);
    vocal.setReverbEnabled (pVocReverbOn->load() > 0.5f);
}

void GuitarDspProcessor::processVocal (juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numOut     = buffer.getNumChannels();
    const int numIn      = getTotalNumInputChannels();

    updateVocalFromApvts();

    // Mono mikrofonjel: CSAK a kiválasztott bemeneti csatorna (a mic melyik
    // Scarlett bemeneten van), majd minden kimenetre másoljuk.
    const int vCh = juce::jlimit (0, juce::jmax (0, numIn - 1), vocalInputCh.load());
    monoBuffer.setSize (1, numSamples, false, false, true);
    monoBuffer.clear();
    float* mono = monoBuffer.getWritePointer (0);
    if (numIn > 0)
        monoBuffer.copyFrom (0, 0, buffer, vCh, 0, numSamples);

    for (int ch = 0; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, mono, numSamples);

    // Teljes ének-lánc sztereó blokkon (Gain -> LowCut -> Comp -> Air -> Reverb -> Limiter).
    juce::dsp::AudioBlock<float> block (buffer);
    vocal.process (block);
}

//==============================================================================
int GuitarDspProcessor::getEffectiveLatencySamples() const noexcept
{
    // Az ének lánc végig nulla-latenciás (IIR/comp/reverb/limiter).
    if ((AppMode) appMode.load() != AppMode::guitar)
        return 0;

    int s = 0;

    // A pitch shifter csak akkor ad latenciát, ha be van kapcsolva ÉS van eltolás
    // (semitones != 0) — a process() ilyenkor engedi át nyersen, latencia nélkül.
    if (pPitchOn != nullptr && pPitchOn->load() > 0.5f
        && pPitchSemis != nullptr && pPitchSemis->load() != 0.0f)
        s += pitchShifter.getLatencySamples();

    // A Cab IR zero-latency módban fut (0), a többi modul szintén 0 latencia.
    return s;
}

//==============================================================================
void GuitarDspProcessor::copyRecentInput (float* dest, int numToCopy) const noexcept
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
    // Ha kértek többet, mint amennyi van, a maradékot nullázzuk.
    for (int i = n; i < numToCopy; ++i)
        dest[i] = 0.0f;
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
        // Bemeneti csatorna-választás módonként (nem APVTS-param, de perzisztens).
        xml->setAttribute ("guitarInputCh", guitarInputCh.load());
        xml->setAttribute ("vocalInputCh",  vocalInputCh.load());
        copyXmlToBinary (*xml, destData);
    }
}

void GuitarDspProcessor::setStateInformation (const void* data, int sizeInBytes)
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
// A standalone/plugin belépési pontja.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarDspProcessor();
}
