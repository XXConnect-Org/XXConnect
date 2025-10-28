#include "WavWorker.hpp"
#include "sndfile.h"
#include <iostream>

WavWorker::WavWorker(const std::vector<int16_t> &audioData, const std::string &filename, unsigned int sampleRate) {
    _sampleRate = sampleRate;
    _audioData = audioData.data();
    _filename = filename;
    _audioDataSize = audioData.size();
}

bool WavWorker::Save() {
    if (_audioDataSize == 0) {
        std::cout << "No audio data to save!" << std::endl;
        return false;
    }

    SF_INFO sfinfo;
    sfinfo.samplerate = _sampleRate;
    sfinfo.channels = 1;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* outfile = sf_open(_filename.c_str(), SFM_WRITE, &sfinfo);
    if (!outfile) {
        std::cout << "Error: could not open output file: " << _filename << std::endl;
        return false;
    }

    // Записываем данные в файл
    sf_count_t framesWritten = sf_write_short(outfile, _audioData, _audioDataSize);
    sf_close(outfile);

    if (framesWritten != static_cast<sf_count_t>(_audioDataSize)) {
        std::cout << "Error: wrote " << framesWritten << " samples, expected " << _audioDataSize << std::endl;
        return false;
    }
    for (int i = 0; i < _audioDataSize; i++) {
        std::cout << _audioData[i] << " ";
    }

    std::cout << "Successfully saved " << _audioDataSize << " samples to " << _filename << std::endl;
    return true;
}
