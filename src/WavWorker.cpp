#include "../include/WavWorker.hpp"
#include "sndfile.h"
#include "../include/debug_log.hpp"

bool WavWorker::Save() {
    if (!_setter_called) {
        throw SavingWorkerException("Sample rate must be specified");
    }
    if (_audioData.size() == 0) {
        DEBUG_LOG("No audio data to save!" << DEBUG_LOG_ENDL);
        return false;
    }

    SF_INFO sfinfo;
    sfinfo.samplerate = _sampleRate;
    sfinfo.channels = 1;
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* outfile = sf_open(_filename.c_str(), SFM_WRITE, &sfinfo);
    if (!outfile) {
        DEBUG_LOG("Error: could not open output file: " << _filename << DEBUG_LOG_ENDL);
        return false;
    }

    sf_count_t framesWritten = sf_write_short(outfile, _audioData.data(), _audioData.size());
    sf_close(outfile);

    if (framesWritten != static_cast<sf_count_t>(_audioData.size())) {
        DEBUG_LOG("Error: wrote " << framesWritten << " samples, expected " << _audioData.size() << DEBUG_LOG_ENDL);
        return false;
    }

    DEBUG_LOG("Successfully saved " << _audioData.size() << " samples to " << _filename << DEBUG_LOG_ENDL);
    return true;
}


