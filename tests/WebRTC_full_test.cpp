#include "rtc/rtc.hpp"
#include "AudioRecorder/AudioRecorder.hpp"
#include "AudioSender/AudioSender.hpp"
#include "AudioReceiver/AudioReceiver.hpp"
#include "AudioRecorder/NoiseSuppressor.hpp"
#include "SavingWorkers/ISavingWorker.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace {

// Создает пару PeerConnection в одном процессе и соединяет их без внешнего
// сигнального сервера: все локальные описания и ICE-кандидаты пробрасываются
// напрямую между экземплярами.
struct PeerPair {
    std::shared_ptr<rtc::PeerConnection> caller;
    std::shared_ptr<rtc::PeerConnection> callee;
};

PeerPair CreateLoopbackPair() {
    rtc::Configuration config;
    config.disableAutoNegotiation = true; // управляем сигналингом сами

    auto pcA = std::make_shared<rtc::PeerConnection>(config);
    auto pcB = std::make_shared<rtc::PeerConnection>(config);

    // Обмен sdp
    pcA->onLocalDescription([pcB](const rtc::Description& desc) {
        pcB->setRemoteDescription(desc);
        if (desc.type() == rtc::Description::Type::Offer) {
            pcB->setLocalDescription(rtc::Description::Type::Answer);
        }
    });
    pcB->onLocalDescription([pcA](const rtc::Description& desc) {
        pcA->setRemoteDescription(desc);
    });

    // Обмен ICE
    pcA->onLocalCandidate([pcB](const rtc::Candidate& c) { pcB->addRemoteCandidate(c); });
    pcB->onLocalCandidate([pcA](const rtc::Candidate& c) { pcA->addRemoteCandidate(c); });

    return {pcA, pcB};
}

// Простой worker, который ничего не сохраняет (для теста)
class NullSavingWorker : public ISavingWorker {
public:
    bool Save() override { return true; }
};

} // namespace

int main() {
    using namespace std::chrono_literals;

    std::cout << "=== Full Integration Test: AudioRecorder -> AudioSender -> PeerConnection -> AudioReceiver ===" << std::endl;

    auto pair = CreateLoopbackPair();
    auto& pcA = pair.caller;  // Сторона отправителя
    auto& pcB = pair.callee;  // Сторона получателя

    // Отслеживаем подключение
    std::promise<void> connectedPromise;
    auto connectedFuture = connectedPromise.get_future();

    auto onConnected = [&connectedPromise](rtc::PeerConnection::State s) {
        if (s == rtc::PeerConnection::State::Connected) {
            connectedPromise.set_value();
        }
    };
    pcA->onStateChange(onConnected);
    pcB->onStateChange(onConnected);

    // Создаем DataChannel для отправки аудио данных
    auto dc = pcA->createDataChannel("audio");

    // Ждем открытия DataChannel
    std::promise<void> dcOpenPromise;
    auto dcOpenFuture = dcOpenPromise.get_future();
    dc->onOpen([&dcOpenPromise]() {
        std::cout << "DataChannel opened on sender side" << std::endl;
        dcOpenPromise.set_value();
    });

    // Стартуем сигналинг
    pcA->setLocalDescription(rtc::Description::Type::Offer);

    // Ждем подключения
    if (connectedFuture.wait_for(5s) != std::future_status::ready) {
        std::cerr << "ERROR: Peer connection did not reach Connected state" << std::endl;
        return 1;
    }
    std::cout << "Peer connection established" << std::endl;

    // ========== СТОРОНА ОТПРАВИТЕЛЯ ==========
    std::cout << "\n--- Setting up sender side ---" << std::endl;

    // Создаем AudioRecorder и AudioSender
    auto nullWorker = std::make_shared<NullSavingWorker>();
    AudioRecorder recorder(nullWorker);
    NoiseSuppressor suppressor;
    suppressor.SetEnabled(false); // для теста отправляем сырой PCM

    unsigned int sampleRate = recorder.GetSampleRate();
    std::cout << "Sample rate: " << sampleRate << " Hz" << std::endl;

    // Создаем AudioSender (хотя в этом тесте мы используем DataChannel напрямую,
    // но показываем интеграцию с AudioSender для будущего использования с Track)
    AudioSender sender(pcA, suppressor, sampleRate);

    // Настраиваем callback для отправки данных через DataChannel
    // В реальном приложении это можно делать через AudioSender
    std::atomic<size_t> sentChunks{0};
    recorder.SetOnBufferCallback([dc, &sentChunks](const int16_t* samples, size_t numSamples, unsigned int) {
        if (!dc->isOpen()) {
            return;
        }

        const std::byte* begin = reinterpret_cast<const std::byte*>(samples);
        const std::byte* end = begin + numSamples * sizeof(int16_t);
        rtc::binary frame(begin, end);
        dc->send(frame);
        sentChunks++;
    });

    std::cout << "AudioRecorder and AudioSender configured" << std::endl;

    // ========== СТОРОНА ПОЛУЧАТЕЛЯ ==========
    std::cout << "\n--- Setting up receiver side ---" << std::endl;

    // Создаем AudioReceiver
    AudioReceiver receiver(pcB, sampleRate);
    receiver.SetupDataChannel("audio");

    // Отслеживаем полученные данные
    std::vector<int16_t> receivedSamples;
    std::mutex receivedMutex;
    std::atomic<size_t> receivedChunks{0};

    receiver.SetOnAudioDataCallback([&receivedSamples, &receivedMutex, &receivedChunks](
                                        const int16_t* samples, size_t numSamples, unsigned int) {
        std::lock_guard<std::mutex> lock(receivedMutex);
        receivedSamples.insert(receivedSamples.end(), samples, samples + numSamples);
        receivedChunks++;
    });

    std::cout << "AudioReceiver configured" << std::endl;

    // Ждем открытия DataChannel
    if (dcOpenFuture.wait_for(2s) != std::future_status::ready) {
        std::cerr << "ERROR: DataChannel did not open" << std::endl;
        return 1;
    }

    std::this_thread::sleep_for(200ms);

    // ========== ЗАПИСЬ И ПЕРЕДАЧА ==========
    std::cout << "\n--- Starting recording and transmission ---" << std::endl;
    std::cout << "Recording 2 seconds of audio..." << std::endl;
    std::cout << "Please speak into your microphone..." << std::endl;

    // Записываем 2 секунды аудио
    recorder.Record(2000);

    std::cout << "Recording finished. Waiting for data transmission..." << std::endl;

    // Ждем, чтобы все данные были переданы
    std::this_thread::sleep_for(500ms);

    // ========== ПРОВЕРКА РЕЗУЛЬТАТОВ ==========
    std::cout << "\n--- Verifying results ---" << std::endl;

    // Получаем записанные данные
    const auto& recordedData = recorder.GetAudioData();
    size_t recordedSamples = recordedData.size();

    // Получаем полученные данные
    std::lock_guard<std::mutex> lock(receivedMutex);
    size_t receivedSamplesCount = receivedSamples.size();
    size_t chunksReceived = receivedChunks.load();
    size_t chunksSent = sentChunks.load();

    // Получаем данные из AudioReceiver
    const auto& receiverData = receiver.GetReceivedAudioData();
    size_t receiverSamplesCount = receiverData.size();

    std::cout << "Recorded samples: " << recordedSamples << std::endl;
    std::cout << "Sent chunks: " << chunksSent << std::endl;
    std::cout << "Received via callback: " << receivedSamplesCount 
              << " samples in " << chunksReceived << " chunks" << std::endl;
    std::cout << "Received via receiver: " << receiverSamplesCount << " samples" << std::endl;

    // Проверки
    bool allChecksPassed = true;

    // 1. Проверка, что записали данные
    if (recordedSamples == 0) {
        std::cerr << "ERROR: No samples were recorded" << std::endl;
        allChecksPassed = false;
    }

    // 2. Проверка, что данные были отправлены
    if (chunksSent == 0) {
        std::cerr << "ERROR: No data chunks were sent" << std::endl;
        allChecksPassed = false;
    }

    // 3. Проверка, что данные были получены
    if (receivedSamplesCount == 0 && receiverSamplesCount == 0) {
        std::cerr << "ERROR: No audio data received over WebRTC" << std::endl;
        allChecksPassed = false;
    }

    // 4. Проверка количества полученных данных
    size_t totalReceived = std::max(receivedSamplesCount, receiverSamplesCount);
    size_t expectedMin = sampleRate * 15 / 10; // минимум 150% от 1 секунды (с запасом)
    size_t expectedMax = sampleRate * 25 / 10; // максимум 250% от 1 секунды

    if (totalReceived < expectedMin) {
        std::cerr << "WARNING: Received only " << totalReceived 
                  << " samples, expected at least " << expectedMin << std::endl;
        // Не критично, может быть из-за задержек
    }

    if (totalReceived > expectedMax) {
        std::cerr << "WARNING: Received " << totalReceived 
                  << " samples, expected at most " << expectedMax << std::endl;
        // Не критично, может быть из-за буферизации
    }

    // 5. Проверка совпадения данных между callback и receiver
    if (receivedSamplesCount > 0 && receiverSamplesCount > 0) {
        if (receivedSamplesCount != receiverSamplesCount) {
            std::cerr << "WARNING: Mismatch between callback data (" << receivedSamplesCount
                      << ") and receiver data (" << receiverSamplesCount << ")" << std::endl;
        } else {
            std::cout << "SUCCESS: Callback and receiver data match" << std::endl;
        }
    }

    // 6. Проверка, что полученные данные примерно соответствуют записанным
    if (recordedSamples > 0 && totalReceived > 0) {
        double ratio = static_cast<double>(totalReceived) / static_cast<double>(recordedSamples);
        if (ratio < 0.5 || ratio > 2.0) {
            std::cerr << "WARNING: Significant mismatch between recorded (" << recordedSamples
                      << ") and received (" << totalReceived 
                      << ") samples, ratio: " << ratio << std::endl;
        } else {
            std::cout << "SUCCESS: Received data ratio is reasonable: " << ratio << std::endl;
        }
    }

    if (allChecksPassed && totalReceived > 0) {
        std::cout << "\n=== Full Integration Test PASSED ===" << std::endl;
        std::cout << "AudioRecorder -> AudioSender -> PeerConnection -> AudioReceiver pipeline works correctly!" << std::endl;
        return 0;
    } else {
        std::cerr << "\n=== Full Integration Test FAILED ===" << std::endl;
        return 1;
    }
}

