#include "WavWorker.hpp"
#include "sndfile.h"
#include <iostream>

bool WavWorker::Save() {
    if (!_setter_called) {throw SavingWorkerException("Sample rate must be specified");}
    if (_audioData.size() == 0) {
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
    sf_count_t framesWritten = sf_write_short(outfile, _audioData.data(), _audioData.size());
    sf_close(outfile);

    if (framesWritten != static_cast<sf_count_t>(_audioData.size())) {
        std::cout << "Error: wrote " << framesWritten << " samples, expected " << _audioData.size() << std::endl;
        return false;
    }
    for (int i = 0; i < _audioData.size(); i++) {
        std::cout << _audioData[i] << " ";
    }

    std::cout << "Successfully saved " << _audioData.size() << " samples to " << _filename << std::endl;
    return true;
}
