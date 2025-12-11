#include "RtAudio.h"
#include "AudioRecorder.hpp"
#include "../common/debug_log.hpp"

#include <cstring>

int record(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
           double streamTime, RtAudioStreamStatus status, void *userData)
{
    RecordData* data = static_cast<RecordData*>(userData);

    if (status) {
        DEBUG_LOG("Stream overflow detected!" << DEBUG_LOG_ENDL);
    }

    if (data->isRecording && inputBuffer) {
        int16_t* inputSamples = static_cast<int16_t*>(inputBuffer);

        data->audioData.insert(data->audioData.end(),
                               inputSamples,
                               inputSamples + nBufferFrames);

        if (data->onBuffer) {
            data->onBuffer(inputSamples, nBufferFrames, data->sampleRate);
        }

        static int counter = 0;
        if (++counter % (44100 / nBufferFrames) == 0) {
            DEBUG_LOG("." << std::flush);
        }
    }

    return 0;
}

AudioRecorder::AudioRecorder(std::shared_ptr<ISavingWorker> saving_worker)
    : _is_recording(false)
    , _buffer_frames(512)
    , _saving_worker(saving_worker) {
    bool streamOpened = false;

    std::vector<unsigned int> deviceIds = _audio.getDeviceIds();
    if (deviceIds.size() < 1) {
        throw std::runtime_error("No audio devices found");
    }

    DEBUG_LOG("Available audio devices:" << DEBUG_LOG_ENDL);
    for (unsigned int i = 0; i < deviceIds.size(); i++) {
        RtAudio::DeviceInfo info = _audio.getDeviceInfo(deviceIds[i]);
        DEBUG_LOG("Device " << i << " (ID: " << deviceIds[i] << "): " << info.name << DEBUG_LOG_ENDL);
        DEBUG_LOG("  Input channels: " << info.inputChannels << DEBUG_LOG_ENDL);
        if (info.inputChannels > 0) {
            DEBUG_LOG("  Supported sample rates: ");
            for (unsigned int sr : info.sampleRates) {
                DEBUG_LOG(sr << " ");
            }
            DEBUG_LOG(DEBUG_LOG_ENDL);
        }
    }

    unsigned int defaultDevice = _audio.getDefaultInputDevice();
    RtAudio::DeviceInfo defaultInfo = _audio.getDeviceInfo(defaultDevice);

    DEBUG_LOG("\nUsing default input device: " << defaultInfo.name << DEBUG_LOG_ENDL);
    DEBUG_LOG("Input channels: " << defaultInfo.inputChannels << DEBUG_LOG_ENDL);

    if (defaultInfo.inputChannels < 1) {
        DEBUG_LOG("Default device has no input channels! Searching for alternative..." << DEBUG_LOG_ENDL);
        for (unsigned int i = 0; i < deviceIds.size(); i++) {
            RtAudio::DeviceInfo info = _audio.getDeviceInfo(deviceIds[i]);
            if (info.inputChannels > 0) {
                defaultDevice = deviceIds[i];
                defaultInfo = info;
                DEBUG_LOG("Using device: " << info.name << DEBUG_LOG_ENDL);
                break;
            }
        }
    }

    if (defaultInfo.inputChannels < 1) {
        throw std::runtime_error("No input devices found!");
    }

    unsigned int sampleRate = 44100;
    bool sampleRateSupported = false;
    for (unsigned int sr : defaultInfo.sampleRates) {
        if (sr == sampleRate) {
            sampleRateSupported = true;
            break;
        }
    }

    if (!sampleRateSupported) {
        sampleRate = defaultInfo.preferredSampleRate;
        DEBUG_LOG("44100 not supported, using preferred rate: " << sampleRate << DEBUG_LOG_ENDL);
    }

    _record_data.sampleRate = sampleRate;
    _record_data.isRecording = false;

    _parameters.deviceId = defaultDevice;
    _parameters.nChannels = 1;
    _parameters.firstChannel = 0;

    unsigned int bufferFrames = 256;

    DEBUG_LOG("\nTrying to open stream with:" << DEBUG_LOG_ENDL);
    DEBUG_LOG("  Sample rate: " << sampleRate << DEBUG_LOG_ENDL);
    DEBUG_LOG("  Buffer frames: " << bufferFrames << DEBUG_LOG_ENDL);
    DEBUG_LOG("  Format: SINT16" << DEBUG_LOG_ENDL);

    if (_audio.openStream(NULL, &_parameters, RTAUDIO_SINT16,
                       sampleRate, &bufferFrames, &record, &_record_data)) {
        DEBUG_LOG("Error opening stream: " << _audio.getErrorText() << DEBUG_LOG_ENDL);
        DEBUG_LOG("Trying FLOAT32 format..." << DEBUG_LOG_ENDL);
        
        if (_audio.openStream(NULL, &_parameters, RTAUDIO_FLOAT32,
                           sampleRate, &bufferFrames, &record, &_record_data)) {
            DEBUG_LOG("Error with FLOAT32: " << _audio.getErrorText() << DEBUG_LOG_ENDL);
            DEBUG_LOG("Trying sample rate 48000..." << DEBUG_LOG_ENDL);
            
            if (_audio.openStream(NULL, &_parameters, RTAUDIO_SINT16,
                               48000, &bufferFrames, &record, &_record_data)) {
                throw std::runtime_error("Error opening stream, all sample rates failed");
            } else {
                sampleRate = 48000;
                _record_data.sampleRate = sampleRate;
                DEBUG_LOG("Success with 48000 SINT16!" << DEBUG_LOG_ENDL);
                streamOpened = true;
            }
        } else {
            DEBUG_LOG("Success with FLOAT32!" << DEBUG_LOG_ENDL);
            streamOpened = true;
        }
    } else {
        DEBUG_LOG("Stream opened successfully with SINT16!" << DEBUG_LOG_ENDL);
        streamOpened = true;
    }

    if (!streamOpened) {
        throw std::runtime_error("Failed to open audio stream!");
    }
    _saving_worker->SetSampleRate(sampleRate);
}

AudioRecorder::~AudioRecorder() {
    if (_audio.isStreamOpen()) {
        _audio.closeStream();
    }
}

void AudioRecorder::Record(unsigned int milliseconds) {
    _record_data.audioData.clear();
    _record_data.isRecording = true;

    DEBUG_LOG("\n=== Starting recording ===" << DEBUG_LOG_ENDL);
    DEBUG_LOG("Recording");

    if (_audio.startStream()) {
        DEBUG_LOG("Error starting stream: " << _audio.getErrorText() << DEBUG_LOG_ENDL);
        if (_audio.isStreamOpen()) {
            _audio.closeStream();
        }
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));

    _record_data.isRecording = false;

    if (_audio.isStreamRunning()) {
        _audio.stopStream();
    }

    DEBUG_LOG(DEBUG_LOG_ENDL);
    DEBUG_LOG("Recording stopped." << DEBUG_LOG_ENDL);
    DEBUG_LOG("Recorded " << _record_data.audioData.size() << " samples" << DEBUG_LOG_ENDL);

    if (_record_data.audioData.empty()) {
        DEBUG_LOG("WARNING: No audio data was recorded!" << DEBUG_LOG_ENDL);
    }

    if (_audio.isStreamOpen()) {
        _audio.closeStream();
    }

    _saving_worker->SetAudioData(_record_data.audioData);
}

bool AudioRecorder::SaveData() {
    if (_saving_worker->Save()) {
        DEBUG_LOG("File saved successfully!" << DEBUG_LOG_ENDL);
        return true;
    } else {
        DEBUG_LOG("Error saving file!" << DEBUG_LOG_ENDL);
        return false;
    }
}


