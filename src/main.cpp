#include "RtAudio.h"
#include "sndfile.h"
#include "../AudioRecorder/RecordData.hpp"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <atomic>
#include <thread>



int record(void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
           double streamTime, RtAudioStreamStatus status, void *userData)
{
    RecordData* data = static_cast<RecordData*>(userData);

    if (status) {
        std::cout << "Stream overflow detected!" << std::endl;
    }

    if (data->isRecording && inputBuffer) {
        // Копируем данные из inputBuffer в наш вектор
        int16_t* inputSamples = static_cast<int16_t*>(inputBuffer);

        // Защищаем от одновременного доступа (хотя в данном случае это не критично)
        data->audioData.insert(data->audioData.end(), inputSamples, inputSamples + nBufferFrames);

        // Выводим прогресс каждые 44100 samples (1 секунда)
        static int counter = 0;
        if (++counter % (44100 / nBufferFrames) == 0) {
            std::cout << "." << std::flush;
        }
    }

    return 0;
}

bool saveToWavFile(const std::vector<int16_t>& audioData, const std::string& filename, unsigned int sampleRate) {
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

int main()
{
    RtAudio adc;
    bool streamOpened = false;

    // Проверяем доступные устройства
    std::vector<unsigned int> deviceIds = adc.getDeviceIds();
    if (deviceIds.size() < 1) {
        std::cout << "\nNo audio devices found!\n";
        return 0;
    }

    // Выводим информацию о устройствах
    std::cout << "Available audio devices:" << std::endl;
    for (unsigned int i = 0; i < deviceIds.size(); i++) {
        RtAudio::DeviceInfo info = adc.getDeviceInfo(deviceIds[i]);
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

    // Получаем информацию о дефолтном устройстве
    unsigned int defaultDevice = adc.getDefaultInputDevice();
    RtAudio::DeviceInfo defaultInfo = adc.getDeviceInfo(defaultDevice);

    std::cout << "\nUsing default input device: " << defaultInfo.name << std::endl;
    std::cout << "Input channels: " << defaultInfo.inputChannels << std::endl;

    // Проверяем, есть ли входные каналы
    if (defaultInfo.inputChannels < 1) {
        std::cout << "Default device has no input channels! Searching for alternative..." << std::endl;
        for (unsigned int i = 0; i < deviceIds.size(); i++) {
            RtAudio::DeviceInfo info = adc.getDeviceInfo(deviceIds[i]);
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
        return 0;
    }

    std::cout << "Preferred sample rate: " << defaultInfo.preferredSampleRate << std::endl;

    // Используем поддерживаемую частоту дискретизации
    unsigned int sampleRate = 44100;

    // Проверяем поддерживается ли 44100
    bool sampleRateSupported = false;
    for (unsigned int sr : defaultInfo.sampleRates) {
        if (sr == sampleRate) {
            sampleRateSupported = true;
            break;
        }
    }

    // Если 44100 не поддерживается, используем предпочтительную частоту
    if (!sampleRateSupported) {
        sampleRate = defaultInfo.preferredSampleRate;
        std::cout << "44100 not supported, using preferred rate: " << sampleRate << std::endl;
    }

    // Создаем структуру для передачи данных
    RecordData recordData;
    recordData.sampleRate = sampleRate;
    recordData.isRecording = false;

    RtAudio::StreamParameters parameters;
    parameters.deviceId = defaultDevice;
    parameters.nChannels = 1;
    parameters.firstChannel = 0;

    unsigned int bufferFrames = 256;

    std::cout << "\nTrying to open stream with:" << std::endl;
    std::cout << "  Sample rate: " << sampleRate << std::endl;
    std::cout << "  Buffer frames: " << bufferFrames << std::endl;
    std::cout << "  Format: SINT16" << std::endl;

    // Пробуем открыть поток с передачей userData
    if (adc.openStream(NULL, &parameters, RTAUDIO_SINT16,
                       sampleRate, &bufferFrames, &record, &recordData)) {
        std::cout << "Error opening stream: " << adc.getErrorText() << std::endl;

        // Пробуем другие форматы если SINT16 не работает
        std::cout << "Trying FLOAT32 format..." << std::endl;
        if (adc.openStream(NULL, &parameters, RTAUDIO_FLOAT32,
                           sampleRate, &bufferFrames, &record, &recordData)) {
            std::cout << "Error with FLOAT32: " << adc.getErrorText() << std::endl;

            // Пробуем другую частоту дискретизации
            std::cout << "Trying sample rate 48000..." << std::endl;
            if (adc.openStream(NULL, &parameters, RTAUDIO_SINT16,
                               48000, &bufferFrames, &record, &recordData)) {
                std::cout << "All attempts failed: " << adc.getErrorText() << std::endl;
                return 0;
            } else {
                sampleRate = 48000;
                recordData.sampleRate = sampleRate;
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
        return 0;
    }

    // Очищаем данные перед записью
    recordData.audioData.clear();
    recordData.isRecording = true;

    std::cout << "\n=== Starting recording ===" << std::endl;
    std::cout << "Recording";

    // Запускаем поток
    if (adc.startStream()) {
        std::cout << "Error starting stream: " << adc.getErrorText() << std::endl;
        if (adc.isStreamOpen()) {
            adc.closeStream();
        }
        return 0;
    }

    // Записываем 5 секунд
    for (int i = 0; i < 5; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Останавливаем запись
    recordData.isRecording = false;

    // Останавливаем поток
    if (adc.isStreamRunning()) {
        adc.stopStream();
    }

    std::cout << std::endl;
    std::cout << "Recording stopped." << std::endl;
    std::cout << "Recorded " << recordData.audioData.size() << " samples ("
              << (double)recordData.audioData.size() / sampleRate << " seconds)" << std::endl;

    // Проверяем, есть ли данные
    if (recordData.audioData.empty()) {
        std::cout << "WARNING: No audio data was recorded!" << std::endl;
        std::cout << "Possible issues:" << std::endl;
        std::cout << "1. Microphone permissions not granted" << std::endl;
        std::cout << "2. Microphone is muted" << std::endl;
        std::cout << "3. No audio input signal" << std::endl;
    } else {
        // Сохраняем в файл
        std::string filename = "recorded_audio.wav";
        std::cout << "Saving to file: " << filename << "..." << std::endl;

        if (saveToWavFile(recordData.audioData, filename, sampleRate)) {
            std::cout << "=== File saved successfully! ===" << std::endl;
        } else {
            std::cout << "=== Error saving file! ===" << std::endl;
        }
    }

    // Закрываем поток
    if (adc.isStreamOpen()) {
        adc.closeStream();
    }

    return 0;
}