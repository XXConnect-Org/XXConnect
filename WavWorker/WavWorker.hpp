#pragma once
#include <cstdint>
#include <vector>

class WavWorker {
public:
    WavWorker(const std::vector<int16_t>& audioData, const std::string& filename, unsigned int sampleRate);
    bool Save();
private:
    const int16_t* _audioData;
    size_t _audioDataSize;
    std::string _filename;
    unsigned int _sampleRate;
};
