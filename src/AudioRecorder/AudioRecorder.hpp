#pragma once

#include <RtAudio.h>
#include <vector>
#include <string>
#include <atomic>
#include <iostream>
#include <fstream>
#include <memory>
#include <thread>
#include <cstdlib>
#include <cstring>

#include "RecordData.hpp"
#include "../SavingWorkers/ISavingWorker.hpp"

class AudioRecorder {
public:
    AudioRecorder(std::shared_ptr<ISavingWorker> saving_worker);
    ~AudioRecorder();

    void Record(unsigned int milliseconds);
    bool SaveData();

    // Access recorded data for external processing 
    const std::vector<int16_t>& GetAudioData() const { return _record_data.audioData; }
    unsigned int GetSampleRate() const { return _record_data.sampleRate; }

    // Set a per-buffer capture callback (called from the audio callback thread).
    // The callback receives: (samples, numSamples, sampleRate).
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

};


