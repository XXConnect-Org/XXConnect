#include "../include/AudioProcessor.hpp"
#include <iostream>
#include <memory>

AudioProcessor::AudioProcessor() : _recording(false) {
    _suppressor = std::make_unique<NoiseSuppressor>();
    _suppressor->SetEnabled(true);
}

AudioProcessor::~AudioProcessor() {
    StopRecording();
}

bool AudioProcessor::Initialize() {
    // Инициализация может потребовать конкретной реализации ISavingWorker
    // Для простоты используем базовую реализацию
    return true;
}

void AudioProcessor::StartRecording(AudioCallback callback) {
    _audioCallback = std::move(callback);
    _recording = true;
    _recordingThread = std::make_unique<std::thread>(&AudioProcessor::RecordingThread, this);
}

void AudioProcessor::StopRecording() {
    _recording = false;
    if (_recordingThread && _recordingThread->joinable()) {
        _recordingThread->join();
    }
}

void AudioProcessor::RecordingThread() {
    // Здесь должна быть реализация записи с микрофона
    // Для примера создадим простой цикл

    std::vector<int16_t> buffer(BUFFER_FRAMES);

    while (_recording) {
        // В реальной реализации здесь будет код записи с микрофона
        // Пока используем заглушку

        // Имитация получения аудио данных
        // В реальности это будет через RtAudio callback

        if (_audioCallback) {
            _audioCallback(buffer.data(), buffer.size());
        }

        // Небольшая задержка для имитации реального времени
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void AudioProcessor::PlayAudio(const int16_t* samples, size_t numSamples) {
    // Здесь должна быть реализация воспроизведения через динамики
    // Пока просто выводим информацию
    std::cout << "Playing " << numSamples << " audio samples" << std::endl;
}

std::vector<int16_t> AudioProcessor::ProcessAudio(const int16_t* samples, size_t numSamples) {
    // Применяем шумоподавление
    return _suppressor->ProcessSamples(samples, numSamples, SAMPLE_RATE, SAMPLE_RATE);
}
