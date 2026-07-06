#pragma once

#include <JuceHeader.h>

/**
    Records the processed output to a WAV file (JUCE AudioRecordingDemo pattern).

    The audio thread pushes samples into a ThreadedWriter, which flushes them to
    disk on its own background TimeSliceThread — so the audio callback never
    touches the file system. start/stopRecording() must be called from the
    message thread; write() is called from the audio thread.

    The file is written as 32-bit float WAV, so post-gain levels can never clip
    in the file.

    RT-safe: write() uses a ScopedTryLock, so the audio thread never blocks on
    start/stop; it just skips the block while the writer is being (un)installed.
*/
class Recorder
{
public:
    Recorder()
    {
        backgroundThread.startThread();
    }

    ~Recorder()
    {
        stopRecording();
    }

    // Starts recording into 'file' (message thread only). Any existing file is
    // replaced. Returns true if the writer was created successfully.
    bool startRecording (const juce::File& file, double sampleRate, int numChannels)
    {
        stopRecording();

        if (sampleRate <= 0.0 || numChannels <= 0)
            return false;

        file.getParentDirectory().createDirectory();
        file.deleteFile();

        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wavFormat.createWriterFor (stream.get(), sampleRate,
                                       (unsigned int) numChannels, 32, {}, 0));
        if (writer == nullptr)
            return false;

        stream.release();   // the writer now owns the stream

        // The ThreadedWriter buffers the incoming audio and writes it to disk
        // on the background thread.
        threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (
            writer.release(), backgroundThread, 32768);

        recordedFile = file;

        const juce::ScopedLock sl (writerLock);
        activeWriter.store (threadedWriter.get());
        return true;
    }

    // Stops recording, flushes and closes the file (message thread only).
    void stopRecording()
    {
        {
            const juce::ScopedLock sl (writerLock);
            activeWriter.store (nullptr);
        }

        // Deleting the ThreadedWriter flushes the remaining buffered audio and
        // closes the file. Safe now: the audio thread can no longer see it.
        threadedWriter.reset();
    }

    bool isRecording() const noexcept { return activeWriter.load() != nullptr; }

    // Audio thread: appends a block to the file. No-op when not recording.
    // TryLock so the audio thread never blocks on start/stop.
    void write (const float* const* channels, int numSamples) noexcept
    {
        const juce::ScopedTryLock sl (writerLock);
        if (sl.isLocked())
            if (auto* w = activeWriter.load())
                w->write (channels, numSamples);
    }

    // The file of the last (or current) recording — for a status readout.
    juce::File getRecordedFile() const { return recordedFile; }

private:
    juce::TimeSliceThread backgroundThread { "LiveDSP Recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::CriticalSection writerLock;
    std::atomic<juce::AudioFormatWriter::ThreadedWriter*> activeWriter { nullptr };
    juce::File recordedFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Recorder)
};
