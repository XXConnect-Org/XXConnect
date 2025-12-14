#include "../include/ISavingWorker.hpp"

#include <string>
#include <cstdint>

void ISavingWorker::SetAudioData(const std::vector<int16_t>& audioData) {
    _audioData = audioData;
    _setter_called = true;
}
void ISavingWorker::SetSampleRate(uint32_t sampleRate) {
    _sampleRate = sampleRate;
}


