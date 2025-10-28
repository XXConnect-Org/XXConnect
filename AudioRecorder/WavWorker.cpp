#include "WavWorker.hpp"

WavWorker::WavWorker(const std::vector<int16_t> &audioData, const std::string &filename, unsigned int sampleRate) {
    _sampleRate = sampleRate;
    _audioData = audioData.data();
    _filename = filename;
}

bool saveToWavFile() {
    if (audioData.empty()) {
        std::cout << "No audio data to save!" << std::endl;
        return false;
    }

    SF_INFO sfinfo;
    sfinfo.samplerate = sampleRate;
    sfinfo.channels = 1;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* outfile = sf_open(filename.c_str(), SFM_WRITE, &sfinfo);
    if (!outfile) {
        std::cout << "Error: could not open output file: " << filename << std::endl;
        return false;
    }

    // Записываем данные в файл
    sf_count_t framesWritten = sf_write_short(outfile, audioData.data(), audioData.size());
    sf_close(outfile);

    if (framesWritten != static_cast<sf_count_t>(audioData.size())) {
        std::cout << "Error: wrote " << framesWritten << " samples, expected " << audioData.size() << std::endl;
        return false;
    }
    for (int i = 0; i < audioData.size(); i++) {
        std::cout << audioData[i] << " ";
    }

    std::cout << "Successfully saved " << audioData.size() << " samples to " << filename << std::endl;
    return true;
}
