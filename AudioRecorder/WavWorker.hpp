#pragma once
#include <cstdint>
#include <vector>

class WavWorker {
public:
    WavWorker(const std::vector<int16_t>& audioData, const std::string& filename, unsigned int sampleRate);
private:
    int16_t* _audioData;
    std::string _filename;
    unsigned int _sampleRate;
}
