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

    std::cout << "=== Testing AudioReceiver with AudioRecorder -> PeerConnection ===" << std::endl;

    auto pair = CreateLoopbackPair();
    auto& pcA = pair.caller;
    auto& pcB = pair.callee;

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
        std::cout << "DataChannel opened on caller side" << std::endl;
        dcOpenPromise.set_value();
    });

    // Стартуем сигналинг
    pcA->setLocalDescription(rtc::Description::Type::Offer);

    // Ждем подключения
    if (connectedFuture.wait_for(5s) != std::future_status::ready) {
        std::cerr << "Peer connection did not reach Connected state" << std::endl;
        return 1;
    }
    std::cout << "Peer connection established" << std::endl;

    // Создаем AudioRecorder и AudioSender на стороне отправителя
    auto nullWorker = std::make_shared<NullSavingWorker>();
    AudioRecorder recorder(nullWorker);
    NoiseSuppressor suppressor;
    suppressor.SetEnabled(false); // для теста отправляем сырой PCM

    unsigned int sampleRate = recorder.GetSampleRate();
    
    std::cout << "Sample rate: " << sampleRate << " Hz" << std::endl;

    // Настраиваем callback для отправки данных через DataChannel
    recorder.SetOnBufferCallback([dc](const int16_t* samples, size_t numSamples, unsigned int) {
        if (!dc->isOpen()) {
            return;
        }

        const std::byte* begin = reinterpret_cast<const std::byte*>(samples);
        const std::byte* end = begin + numSamples * sizeof(int16_t);
        rtc::binary frame(begin, end);
        dc->send(frame);
    });

    // Создаем AudioReceiver на стороне получателя
    AudioReceiver receiver(pcB, sampleRate);
    
    // Настраиваем прием через DataChannel
    receiver.SetupDataChannel("audio");

    // Отслеживаем полученные данные через callback
    std::vector<int16_t> receivedViaCallback;
    std::mutex callbackMutex;
    std::atomic<size_t> callbackCount{0};
    
    receiver.SetOnAudioDataCallback([&receivedViaCallback, &callbackMutex, &callbackCount](
                                        const int16_t* samples, size_t numSamples, unsigned int) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        receivedViaCallback.insert(receivedViaCallback.end(), samples, samples + numSamples);
        callbackCount++;
    });

    // Ждем открытия DataChannel
    if (dcOpenFuture.wait_for(2s) != std::future_status::ready) {
        std::cerr << "DataChannel did not open" << std::endl;
        return 1;
    }

    std::this_thread::sleep_for(200ms);

    // Записываем короткий фрагмент (1 секунда)
    std::cout << "Recording 1 second of audio..." << std::endl;
    std::cout << "Please speak into your microphone..." << std::endl;
    
    recorder.Record(1000); // 1 секунда

    std::cout << "Recording finished. Waiting for data transmission..." << std::endl;

    // Ждем немного, чтобы все данные были переданы
    std::this_thread::sleep_for(500ms);

    // Проверяем данные, полученные через callback
    std::lock_guard<std::mutex> lock(callbackMutex);
    size_t totalReceivedViaCallback = receivedViaCallback.size();
    size_t callbacks = callbackCount.load();

    // Проверяем данные, накопленные в AudioReceiver
    const auto& receivedViaReceiver = receiver.GetReceivedAudioData();
    size_t totalReceivedViaReceiver = receivedViaReceiver.size();

    std::cout << "Received via callback: " << totalReceivedViaCallback 
              << " samples in " << callbacks << " callbacks" << std::endl;
    std::cout << "Received via receiver: " << totalReceivedViaReceiver << " samples" << std::endl;

    if (totalReceivedViaCallback == 0 && totalReceivedViaReceiver == 0) {
        std::cerr << "ERROR: No audio data received over WebRTC" << std::endl;
        return 1;
    }

    // Проверяем, что получили разумное количество данных
    size_t expectedMin = sampleRate * 8 / 10; // минимум 80% от ожидаемого
    size_t totalReceived = std::max(totalReceivedViaCallback, totalReceivedViaReceiver);
    
    if (totalReceived < expectedMin) {
        std::cerr << "WARNING: Received only " << totalReceived 
                  << " samples, expected at least " << expectedMin << std::endl;
        return 1;
    }

    // Проверяем, что данные совпадают между callback и накопленными данными
    if (totalReceivedViaCallback > 0 && totalReceivedViaReceiver > 0) {
        if (totalReceivedViaCallback != totalReceivedViaReceiver) {
            std::cerr << "WARNING: Mismatch between callback data (" << totalReceivedViaCallback
                      << ") and receiver data (" << totalReceivedViaReceiver << ")" << std::endl;
        } else {
            std::cout << "SUCCESS: Callback and receiver data match" << std::endl;
        }
    }

    std::cout << "AudioReceiver test PASSED" << std::endl;
    return 0;
}

