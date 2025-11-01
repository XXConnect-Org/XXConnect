#include "RtAudio.h"
#include "AudioRecorder.hpp"

#include <cstring>

// callback для rtaudio
int record(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
           double streamTime, RtAudioStreamStatus status, void *userData)
{
    RecordData* data = static_cast<RecordData*>(userData);

    if (status) {
        std::cout << "Stream overflow detected!" << std::endl;
    }

    if (data->isRecording && inputBuffer) {
        int16_t* inputSamples = static_cast<int16_t*>(inputBuffer);

        data->audioData.insert(data->audioData.end(), inputSamples, inputSamples + nBufferFrames);

        static int counter = 0;
        if (++counter % (44100 / nBufferFrames) == 0) {
            std::cout << "." << std::flush;
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
        std::cout << "\nNo audio devices found!\n";
        throw std::runtime_error("No audio devices found");
    }

    std::cout << "Available audio devices:" << std::endl;
    for (unsigned int i = 0; i < deviceIds.size(); i++) {
        RtAudio::DeviceInfo info = _audio.getDeviceInfo(deviceIds[i]);
        std::cout << "Device " << i << " (ID: " << deviceIds[i] << "): " << info.name << std::endl;
        std::cout << "  Input channels: " << info.inputChannels << std::endl;
        if (info.inputChannels > 0) {
            std::cout << "  Supported sample rates: ";
            for (unsigned int sr : info.sampleRates) {
                std::cout << sr << " ";
            }
            std::cout << std::endl;
        }
    }

    unsigned int defaultDevice = _audio.getDefaultInputDevice();
    RtAudio::DeviceInfo defaultInfo = _audio.getDeviceInfo(defaultDevice);

    std::cout << "\nUsing default input device: " << defaultInfo.name << std::endl;
    std::cout << "Input channels: " << defaultInfo.inputChannels << std::endl;

    if (defaultInfo.inputChannels < 1) {
        std::cout << "Default device has no input channels! Searching for alternative..." << std::endl;
        for (unsigned int i = 0; i < deviceIds.size(); i++) {
            RtAudio::DeviceInfo info = _audio.getDeviceInfo(deviceIds[i]);
            if (info.inputChannels > 0) {
                defaultDevice = deviceIds[i];
                defaultInfo = info;
                std::cout << "Using device: " << info.name << std::endl;
                break;
            }
        }
    }

    if (defaultInfo.inputChannels < 1) {
        std::cout << "No input devices found!" << std::endl;
        throw std::runtime_error("No input devices found!");
    }

    std::cout << "Preferred sample rate: " << defaultInfo.preferredSampleRate << std::endl;

    // самая популярная типа
    unsigned int sampleRate = 44100;

    // Проверяем поддерживается ли 44100
    bool sampleRateSupported = false;
    for (unsigned int sr : defaultInfo.sampleRates) {
        if (sr == sampleRate) {
            sampleRateSupported = true;
            break;
        }
    }

    // Если 44100 не поддерживается, используем то, что хочетустройство
    if (!sampleRateSupported) {
        sampleRate = defaultInfo.preferredSampleRate;
        std::cout << "44100 not supported, using preferred rate: " << sampleRate << std::endl;
    }

    // Создаем структуру для передачи данных
    _record_data.sampleRate = sampleRate;
    _record_data.isRecording = false;

    
    _parameters.deviceId = defaultDevice;
    _parameters.nChannels = 1;
    _parameters.firstChannel = 0;

    unsigned int bufferFrames = 256;

    std::cout << "\nTrying to open stream with:" << std::endl;
    std::cout << "  Sample rate: " << sampleRate << std::endl;
    std::cout << "  Buffer frames: " << bufferFrames << std::endl;
    std::cout << "  Format: SINT16" << std::endl;

    // тут пипец
    if (_audio.openStream(NULL, &_parameters, RTAUDIO_SINT16,
                       sampleRate, &bufferFrames, &record, &_record_data)) {
        std::cout << "Error opening stream: " << _audio.getErrorText() << std::endl;

        // Пробуем другие форматы если SINT16 не работает
        std::cout << "Trying FLOAT32 format..." << std::endl;
        if (_audio.openStream(NULL, &_parameters, RTAUDIO_FLOAT32,
                           sampleRate, &bufferFrames, &record, &_record_data)) {
            std::cout << "Error with FLOAT32: " << _audio.getErrorText() << std::endl;

            // Пробуем другую частоту дискретизации
            std::cout << "Trying sample rate 48000..." << std::endl;
            if (_audio.openStream(NULL, &_parameters, RTAUDIO_SINT16,
                               48000, &bufferFrames, &record, &_record_data)) {
                std::cout << "All attempts failed: " << _audio.getErrorText() << std::endl;
                throw std::runtime_error("Error opening stream, all sample rates failed");
            } else {
                sampleRate = 48000;
                _record_data.sampleRate = sampleRate;
                std::cout << "Success with 48000 SINT16!" << std::endl;
                streamOpened = true;
            }
        } else {
            std::cout << "Success with FLOAT32!" << std::endl;
            streamOpened = true;
        }
    } else {
        std::cout << "Stream opened successfully with SINT16!" << std::endl;
        streamOpened = true;
    }

    if (!streamOpened) {
        std::cout << "Failed to open audio stream!" << std::endl;
        throw std::runtime_error("Failed to open audio stream!");
    }
    _saving_worker->SetSampleRate(sampleRate);
}

AudioRecorder::~AudioRecorder() {
    if (_audio.isStreamOpen()) {
        _audio.closeStream();
    }
}

void AudioRecorder::Record(unsigned int time) {
    _record_data.audioData.clear();
    _record_data.isRecording = true;

    std::cout << "\n=== Starting recording ===" << std::endl;
    std::cout << "Recording";

    if (_audio.startStream()) {
        std::cout << "Error starting stream: " << _audio.getErrorText() << std::endl;
        if (_audio.isStreamOpen()) {
            _audio.closeStream();
        }
        return;
    }

    // Записываем time секунд
    std::this_thread::sleep_for(std::chrono::seconds(time));

    // Останавливаем запись
    _record_data.isRecording = false;

    // Останавливаем поток
    if (_audio.isStreamRunning()) {
        _audio.stopStream();
    }

    std::cout << std::endl;
    std::cout << "Recording stopped." << std::endl;
    std::cout << "Recorded " << _record_data.audioData.size() << " samples ("
              << (double)_record_data.audioData.size() << " seconds)" << std::endl;

    // Проверяем, есть ли данные
    if (_record_data.audioData.empty()) {
        std::cout << "WARNING: No audio data was recorded!" << std::endl;
        std::cout << "Possible issues:" << std::endl;
        std::cout << "1. Microphone permissions not granted" << std::endl;
        std::cout << "2. Microphone is muted" << std::endl;
        std::cout << "3. No audio input signal" << std::endl;
    } else {
        // Сохраняем в файл
        std::cout << "Saving to file: "  << "..." << std::endl;
    }
    // Закрываем поток
    if (_audio.isStreamOpen()) {
        _audio.closeStream();
    }
    std::cout << "setting data..." << _record_data.audioData.size() << std::endl;

    _saving_worker->SetAudioData(_record_data.audioData);

}

bool AudioRecorder::SaveData() {
    if (_saving_worker->Save()) {
        std::cout << "=== File saved successfully! ===" << std::endl;
        return true;
    } else {
        std::cout << "=== Error saving file! ===" << std::endl;
        return false;
    };
}