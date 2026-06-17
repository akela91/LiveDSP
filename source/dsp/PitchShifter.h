#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <cmath>
#include <memory>

// Signalsmith Stretch (header-only, MIT). Az include útvonalat a
// 'signalsmith-stretch' CMake cél adja (include/ mappa), ami transzitívan
// behúzza a signalsmith-linear függőséget is.
#include "signalsmith-stretch/signalsmith-stretch.h"

// Rubber Band Library (GPL/commercial). Az egyfájlos build statikus libje
// (single/RubberBandSingle.cpp) MINDKÉT osztályt lefordítja:
//  - RubberBandStretcher: valós idejű (OptionProcessRealTime) idő/hangmagasság.
//  - RubberBandLiveShifter (v4): kifejezetten a legkisebb latenciás élő pitch.
#include <rubberband/RubberBandStretcher.h>
#include <rubberband/RubberBandLiveShifter.h>

/**
    Valós idejű, polifonikus pitch shifter HÁROM választható motorral.

    - Signalsmith Stretch: élőben állítható blokkmérettel (a 'Pitch Latency'
      knob), 1:1 blokk-feldolgozás belső fix késleltetéssel.
    - Rubber Band Stretcher (R3 'Finer', OptionProcessRealTime): jó polifonikus
      minőség (power chordok), magasabb fix motor-késleltetés.
    - Rubber Band LiveShifter (v4): fix blokkméretű (getBlockSize) shift(),
      a legkisebb latenciára tervezve — élő monitorozáshoz.

    Drop-hangoláshoz: -12 .. +12 félhang (időnyújtás nélkül, 1:1 arányban).
    Mono jelre dolgozik (a gitár jelút mono a NAM-ig).

    A reconfigure/reset/prepare az ÜZENETSZÁLRÓL hívandó (allokálhat); a process()
    a hangszálon tryLock-ot használ (a csere pillanatában 1 blokkot kihagy).
    A getLatencySamples() az AKTÍV motor atomikusan cache-elt latenciáját adja.
*/
class PitchShifter
{
public:
    enum class Engine { signalsmith = 0, rubberband = 1, rubberbandLive = 2 };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate   = spec.sampleRate;
        maxBlockSize = (int) spec.maximumBlockSize;

        scratchIn.assign  ((size_t) maxBlockSize, 0.0f);
        scratchOut.assign ((size_t) maxBlockSize, 0.0f);
        rbScratch.assign  ((size_t) maxBlockSize, 0.0f);

        const juce::SpinLock::ScopedLockType sl (lock);
        configureStretch();
        configureRubberBand();
        configureLiveShifter();
    }

    // STFT-blokk hossza ms-ben a Signalsmith motorhoz (kisebb = kisebb latencia).
    void setBlockMs (double ms) noexcept { blockMs = juce::jlimit (8.0, 120.0, ms); }

    // Élő újrakonfigurálás az új blockMs-szel (Signalsmith) — ÜZENETSZÁLRÓL hívd.
    void reconfigure()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        configureStretch();
    }

    void reset()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        stretch.reset();
        if (rubberBand != nullptr)
        {
            rubberBand->reset();
            rbOut.clear();
        }
        if (liveShifter != nullptr)
        {
            liveShifter->reset();
            lsIn.clear();
            lsOut.clear();
        }
    }

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }

    void setEngine (int e) noexcept { engine.store (e); }

    // RB Live minőség/latencia profil: 0 = Fast (rövid ablak, legkisebb latencia),
    // 1 = Fine (közepes ablak + formant-megőrzés, jobb timbre, kicsit több latencia).
    void setLiveQuality (int q) noexcept { liveQuality.store (q); }

    // RB Live profilváltás — ÜZENETSZÁLRÓL hívd (újraépíti a shiftert + FIFO-t).
    void reconfigureLive()
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        configureLiveShifter();
    }

    // Csak tárol; a transpose-t a process() alkalmazza a zár alatt (szálbiztos).
    void setSemitones (float semitones) noexcept
    {
        currentSemitones.store (juce::jlimit (-12.0f, 12.0f, semitones));
    }

    int getLatencySamples() const noexcept
    {
        switch ((Engine) engine.load())
        {
            case Engine::rubberband:     return rbLatency.load();
            case Engine::rubberbandLive: return lsLatency.load();
            case Engine::signalsmith:
            default:                     return latencyInSamples.load();
        }
    }

    // Mono, in-place feldolgozás (numSamples <= maxBlockSize).
    void process (float* samples, int numSamples) noexcept
    {
        if (! enabled || currentSemitones.load() == 0.0f)
            return;

        // tryLock: ha épp reconfigure/reset zajlik (üzenetszál), kihagyjuk a blokkot.
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked())
            return;

        jassert (numSamples <= maxBlockSize);

        switch ((Engine) engine.load())
        {
            case Engine::rubberband:
                if (rubberBand != nullptr) { processRubberBand (samples, numSamples); return; }
                break;
            case Engine::rubberbandLive:
                if (liveShifter != nullptr) { processLiveShifter (samples, numSamples); return; }
                break;
            default:
                break;
        }
        processSignalsmith (samples, numSamples);
    }

private:
    //==========================================================================
    // Mono kör-FIFO (kettő-hatvány kapacitás), allokáció nélküli push/pop.
    //==========================================================================
    struct MonoRing
    {
        std::vector<float> buf;
        int cap { 0 }, mask { 0 }, read { 0 }, write { 0 }, count { 0 };

        void setCapacityAtLeast (int n)
        {
            int c = 1;
            while (c < n) c <<= 1;
            cap = c; mask = c - 1;
            buf.assign ((size_t) cap, 0.0f);
            read = write = count = 0;
        }

        void clear() noexcept
        {
            read = write = count = 0;
            std::fill (buf.begin(), buf.end(), 0.0f);
        }

        void push (const float* s, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
            {
                buf[(size_t) write] = s[i];
                write = (write + 1) & mask;
                if (count < cap) ++count;
                else read = (read + 1) & mask;   // túlcsordulás: legrégebbit eldobjuk
            }
        }

        // n mintát ad; ha kevesebb van, a maradékot nullázza.
        void popOrZero (float* d, int n) noexcept
        {
            const int have = juce::jmin (count, n);
            for (int i = 0; i < have; ++i)
            {
                d[i] = buf[(size_t) read];
                read = (read + 1) & mask;
                --count;
            }
            for (int i = have; i < n; ++i)
                d[i] = 0.0f;
        }

        // Pontosan n mintát ad (a hívó garantálja, hogy count >= n).
        void pop (float* d, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
            {
                d[i] = buf[(size_t) read];
                read = (read + 1) & mask;
                --count;
            }
        }
    };

    //==========================================================================
    // Signalsmith motor
    //==========================================================================
    void processSignalsmith (float* samples, int numSamples) noexcept
    {
        stretch.setTransposeSemitones (currentSemitones.load());

        std::copy (samples, samples + numSamples, scratchIn.begin());
        float* inPtrs[1]  { scratchIn.data() };
        float* outPtrs[1] { scratchOut.data() };
        stretch.process (inPtrs, numSamples, outPtrs, numSamples);
        std::copy (scratchOut.begin(), scratchOut.begin() + numSamples, samples);
    }

    void configureStretch()
    {
        const int block    = juce::jmax (128, juce::roundToInt (sampleRate * blockMs * 0.001));
        const int interval = juce::jmax (1, block / 4);
        stretch.configure (1, block, interval);
        stretch.setTransposeSemitones (currentSemitones.load());
        stretch.reset();
        latencyInSamples.store ((int) std::lround (stretch.inputLatency() + stretch.outputLatency()));
    }

    //==========================================================================
    // Rubber Band Stretcher (valós idejű, R3 'Finer')
    //==========================================================================
    void processRubberBand (float* samples, int numSamples) noexcept
    {
        const double scale = std::pow (2.0, (double) currentSemitones.load() / 12.0);
        if (scale != rbPitchScale)
        {
            rubberBand->setPitchScale (scale);
            rbPitchScale = scale;
        }

        const float* inPtr[1] { samples };
        rubberBand->process (inPtr, (size_t) numSamples, false);

        for (int avail = rubberBand->available(); avail > 0; avail = rubberBand->available())
        {
            const int chunk = juce::jmin (avail, maxBlockSize);
            float* outPtr[1] { rbScratch.data() };
            const int got = (int) rubberBand->retrieve (outPtr, (size_t) chunk);
            if (got <= 0)
                break;
            rbOut.push (rbScratch.data(), got);
        }

        rbOut.popOrZero (samples, numSamples);
    }

    void configureRubberBand()
    {
        using RB = RubberBand::RubberBandStretcher;

        const int opts = RB::OptionProcessRealTime
                       | RB::OptionEngineFiner
                       | RB::OptionPitchHighQuality;

        rbPitchScale = std::pow (2.0, (double) currentSemitones.load() / 12.0);

        rubberBand = std::make_unique<RB> ((size_t) sampleRate, 1,
                                           (RB::Options) opts, 1.0, rbPitchScale);
        rubberBand->setMaxProcessSize ((size_t) maxBlockSize);

        const int delay = (int) rubberBand->getStartDelay();
        rbOut.setCapacityAtLeast (delay + 8 * maxBlockSize + 16);
        rbLatency.store (delay);
    }

    //==========================================================================
    // Rubber Band LiveShifter (v4) — fix blokkméret, legkisebb latencia
    //==========================================================================
    void processLiveShifter (float* samples, int numSamples) noexcept
    {
        const double scale = std::pow (2.0, (double) currentSemitones.load() / 12.0);
        if (scale != lsPitchScale)
        {
            liveShifter->setPitchScale (scale);
            lsPitchScale = scale;
        }

        // Bemenet akkumulálása; teljes blokkonként shift().
        lsIn.push (samples, numSamples);
        while (lsIn.count >= lsBlock)
        {
            lsIn.pop (lsBlockIn.data(), lsBlock);
            const float* in[1]  { lsBlockIn.data() };
            float*       out[1] { lsBlockOut.data() };
            liveShifter->shift (in, out);
            lsOut.push (lsBlockOut.data(), lsBlock);
        }

        lsOut.popOrZero (samples, numSamples);
    }

    void configureLiveShifter()
    {
        using LS = RubberBand::RubberBandLiveShifter;

        // Fast: OptionWindowShort (= DefaultOptions) a legkisebb latenciához.
        // Fine: közepes ablak + formant-megőrzés (jobb lehangolt timbre).
        const int opts = liveQuality.load() == 1
                           ? (LS::OptionWindowMedium | LS::OptionFormantPreserved)
                           : (int) LS::DefaultOptions;

        liveShifter = std::make_unique<LS> ((size_t) sampleRate, 1, (LS::Options) opts);

        lsPitchScale = std::pow (2.0, (double) currentSemitones.load() / 12.0);
        liveShifter->setPitchScale (lsPitchScale);

        lsBlock = (int) liveShifter->getBlockSize();
        lsBlockIn.assign  ((size_t) lsBlock, 0.0f);
        lsBlockOut.assign ((size_t) lsBlock, 0.0f);

        const int delay = (int) liveShifter->getStartDelay();
        lsIn.setCapacityAtLeast  (lsBlock + 4 * maxBlockSize + 16);
        lsOut.setCapacityAtLeast (delay + lsBlock + 4 * maxBlockSize + 16);

        // Perceptuális latencia ~= belső startDelay + a blokk-akkumuláció (lsBlock).
        lsLatency.store (delay + lsBlock);
    }

    //==========================================================================
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    std::unique_ptr<RubberBand::RubberBandStretcher>  rubberBand;
    std::unique_ptr<RubberBand::RubberBandLiveShifter> liveShifter;
    juce::SpinLock lock;

    double sampleRate   { 48000.0 };
    int    maxBlockSize { 512 };
    bool   enabled      { false };
    double blockMs      { 40.0 };

    std::atomic<int>   engine           { (int) Engine::rubberbandLive };
    std::atomic<int>   liveQuality      { 0 };   // 0 = Fast, 1 = Fine
    std::atomic<float> currentSemitones { 0.0f };
    std::atomic<int>   latencyInSamples { 0 };   // Signalsmith
    std::atomic<int>   rbLatency        { 0 };   // Rubber Band Stretcher
    std::atomic<int>   lsLatency        { 0 };   // Rubber Band LiveShifter

    std::vector<float> scratchIn;
    std::vector<float> scratchOut;
    std::vector<float> rbScratch;

    // Rubber Band Stretcher kimeneti FIFO.
    MonoRing rbOut;
    double   rbPitchScale { 1.0 };

    // LiveShifter be-/kimeneti FIFO + fix blokk-scratch.
    MonoRing lsIn, lsOut;
    std::vector<float> lsBlockIn, lsBlockOut;
    int      lsBlock      { 0 };
    double   lsPitchScale { 1.0 };
};
