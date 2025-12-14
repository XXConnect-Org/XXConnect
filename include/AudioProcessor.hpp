#pragma once

#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include "AudioRecorder.hpp"
#include "NoiseSuppressor.hpp"
#include "ISavingWorker.hpp"

class AudioProcessor {
public:
    using AudioCallback = std::function<void(const int16_t*, size_t)>;

    AudioProcessor();
    ~AudioProcessor();

    bool Initialize();
    void StartRecording(AudioCallback callback);
    void StopRecording();
    void PlayAudio(const int16_t* samples, size_t numSamples);

    std::vector<int16_t> ProcessAudio(const int16_t* samples, size_t numSamples);

private:
    void RecordingThread();

    std::unique_ptr<AudioRecorder> _recorder;
    std::unique_ptr<NoiseSuppressor> _suppressor;
    std::unique_ptr<std::thread> _recordingThread;
    std::atomic<bool> _recording;
    AudioCallback _audioCallback;

    static const unsigned int SAMPLE_RATE = 48000;
    static const unsigned int BUFFER_FRAMES = 512;
};
