#include "AudioRecorder.hpp"
#include <cstring>

AudioRecorder::AudioRecorder()
    : m_isRecording(false)
    , m_sampleRate(44100)
    , m_bufferFrames(512) {
}

AudioRecorder::~AudioRecorder() {
    if (m_audio.isStreamOpen()) {
        m_audio.closeStream();
    }
}

bool AudioRecorder::initialize() {
    // Проверяем доступные API
    RtAudio::Api api = RtAudio::MACOSX_CORE;

    try {
        m_audio = RtAudio(api);
    } catch (RtAudioError& e) {
        std::cerr << "RtAudio error: " << e.getMessage() << std::endl;
        return false;
    }

    // Проверяем доступные устройства
    if (m_audio.getDeviceCount() < 1) {
        std::cerr << "No audio devices found!" << std::endl;
        return false;
    }

    // Настройка параметров потока
    RtAudio::StreamParameters parameters;
    parameters.deviceId = m_audio.getDefaultInputDevice();
    parameters.nChannels = 1;
    parameters.firstChannel = 0;

    RtAudio::StreamOptions options;
    options.flags = RTAUDIO_SCHEDULE_REALTIME;
    options.streamName = "AudioRecorder";

    try {
        m_audio.openStream(nullptr, &parameters, RTAUDIO_SINT16,
                          m_sampleRate, &m_bufferFrames,
                          &AudioRecorder::audioCallback, this, &options);
    } catch (RtAudioError& e) {
        std::cerr << "RtAudio error: " << e.getMessage() << std::endl;
        return false;
    }

    return true;
}

bool AudioRecorder::startRecording() {
    m_audioData.clear();
    m_isRecording = false;

    try {
        m_audio.startStream();
        m_isRecording = true;
        std::cout << "Recording started..." << std::endl;
        return true;
    } catch (RtAudioError& e) {
        std::cerr << "RtAudio error: " << e.getMessage() << std::endl;
        return false;
    }
}

bool AudioRecorder::stopRecording() {
    m_isRecording = false;

    try {
        if (m_audio.isStreamRunning()) {
            m_audio.stopStream();
        }
        std::cout << "Recording stopped. Recorded " << m_audioData.size() << " samples." << std::endl;
        return true;
    } catch (RtAudioError& e) {
        std::cerr << "RtAudio error: " << e.getMessage() << std::endl;
        return false;
    }
}

bool AudioRecorder::saveToFile(const std::string& filename) {
    return writeWavFile(filename);
}

int AudioRecorder::audioCallback(void* outputBuffer, void* inputBuffer,
                               unsigned int nFrames, double streamTime,
                               RtAudioStreamStatus status, void* userData) {
    AudioRecorder* recorder = static_cast<AudioRecorder*>(userData);

    if (status) {
        std::cout << "Stream overflow detected!" << std::endl;
    }

    if (recorder->m_isRecording && inputBuffer) {
        int16_t* samples = static_cast<int16_t*>(inputBuffer);
        recorder->m_audioData.insert(recorder->m_audioData.end(),
                                   samples, samples + nFrames);
    }

    return 0;
}

bool AudioRecorder::writeWavFile(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return false;
    }

    // WAV header structure
    struct WavHeader {
        char riff[4] = {'R','I','F','F'};
        uint32_t chunkSize;
        char wave[4] = {'W','A','V','E'};
        char fmt[4] = {'f','m','t',' '};
        uint32_t subchunk1Size = 16;
        uint16_t audioFormat = 1; // PCM
        uint16_t numChannels = 1;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample = 16;
        char data[4] = {'d','a','t','a'};
        uint32_t subchunk2Size;
    };

    WavHeader header;
    header.sampleRate = m_sampleRate;
    header.byteRate = m_sampleRate * 1 * 16 / 8;
    header.blockAlign = 1 * 16 / 8;
    header.subchunk2Size = static_cast<uint32_t>(m_audioData.size()) * sizeof(int16_t);
    header.chunkSize = 36 + header.subchunk2Size;

    // Write header
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write audio data
    file.write(reinterpret_cast<const char*>(m_audioData.data()),
               m_audioData.size() * sizeof(int16_t));

    if (!file) {
        std::cerr << "Error writing to file: " << filename << std::endl;
        return false;
    }

    std::cout << "Successfully saved " << m_audioData.size() << " samples to " << filename << std::endl;
    return true;
}