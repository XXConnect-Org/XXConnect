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
#include <atomic>

#include "RecordData.hpp"
#include "../SavingWorkers/ISavingWorker.hpp"

class AudioRecorder {
public:
    AudioRecorder(std::shared_ptr<ISavingWorker> saving_worker);
    ~AudioRecorder();

    void Record(unsigned int time);
    bool SaveData();
private:
    RecordData _record_data;
    RtAudio _audio;
    RtAudio::StreamParameters _parameters;
    std::shared_ptr<ISavingWorker> _saving_worker;
    std::atomic<bool> _is_recording;
    unsigned int _buffer_frames;

};
