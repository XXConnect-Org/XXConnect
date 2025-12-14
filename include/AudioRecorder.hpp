#pragma once

#include <RtAudio.h>
#include <vector>
#include <string>
#include <atomic>
#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <cstring>

#include "RecordData.hpp"
#include "ISavingWorker.hpp"

class AudioRecorder {
public:
    AudioRecorder(std::shared_ptr<ISavingWorker> saving_worker);
    ~AudioRecorder();

    void Record(unsigned int milliseconds);

    bool StartRecording();
    void StopRecording();
    bool IsRecording() const { return _is_recording.load(); }

    bool SaveData();

    const std::vector<int16_t>& GetAudioData() const { return _record_data.audioData; }
    unsigned int GetSampleRate() const { return _record_data.sampleRate; }

    // Вызывается для каждого буфера аудио: (samples, numSamples, sampleRate)
    void SetOnBufferCallback(std::function<void(const int16_t*, size_t, unsigned int)> cb) {
        _record_data.onBuffer = std::move(cb);
    }

private:
    RecordData _record_data;
    RtAudio _audio;
    RtAudio::StreamParameters _parameters;
    std::shared_ptr<ISavingWorker> _saving_worker;
    std::atomic<bool> _is_recording;
    unsigned int _buffer_frames;
    mutable std::mutex _stream_mutex;

};


