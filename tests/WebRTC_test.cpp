#include "rtc/rtc.hpp"
#include "AudioSender/AudioSender.hpp"
#include "AudioRecorder/NoiseSuppressor.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <iostream>
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

} // namespace

int main() {
    using namespace std::chrono_literals;

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

    // Подготовка приема на стороне callee через DataChannel
    std::promise<std::vector<int16_t>> receivedPromise;
    auto receivedFuture = receivedPromise.get_future();

    std::atomic<bool> dataChannelReceived{false};
    pcB->onDataChannel([&receivedPromise, &dataChannelReceived](std::shared_ptr<rtc::DataChannel> dc) {
        std::cout << "DataChannel received on callee side" << std::endl;
        dataChannelReceived = true;
        
        dc->onOpen([dc]() {
            std::cout << "DataChannel opened on callee side" << std::endl;
        });
        
        dc->onMessage(
            [&receivedPromise](rtc::binary message) {
                std::cout << "Received message: " << message.size() << " bytes" << std::endl;
                const auto* samples = reinterpret_cast<const int16_t*>(message.data());
                size_t numSamples = message.size() / sizeof(int16_t);
                std::vector<int16_t> buffer(samples, samples + numSamples);
                receivedPromise.set_value(std::move(buffer));
            },
            [](rtc::string) {
                // Ignore string messages
            }
        );
    });

    // Создаем DataChannel для отправки сырых PCM данных
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

    // Ждем, пока DataChannel будет получен на принимающей стороне
    auto start = std::chrono::steady_clock::now();
    while (!dataChannelReceived && (std::chrono::steady_clock::now() - start) < 2s) {
        std::this_thread::sleep_for(100ms);
    }
    if (!dataChannelReceived) {
        std::cerr << "DataChannel was not received on callee side" << std::endl;
        return 1;
    }
    
    // Ждем открытия DataChannel
    if (dcOpenFuture.wait_for(2s) != std::future_status::ready) {
        std::cerr << "DataChannel did not open" << std::endl;
        return 1;
    }
    
    std::this_thread::sleep_for(200ms);

    // Генерируем тестовый буфер и отправляем через DataChannel
    std::vector<int16_t> original(480);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<int16_t>((i % 200) - 100);
    }
    std::cout << "Sending test buffer: " << original.size() << " samples" << std::endl;
    
    const std::byte* begin = reinterpret_cast<const std::byte*>(original.data());
    const std::byte* end = begin + original.size() * sizeof(int16_t);
    rtc::binary frame(begin, end);
    dc->send(frame);

    // Ждем получение на принимающей стороне
    if (receivedFuture.wait_for(5s) != std::future_status::ready) {
        std::cerr << "No audio frame received over WebRTC" << std::endl;
        return 1;
    }

    auto received = receivedFuture.get();
    if (received != original) {
        std::cerr << "Received frame size/data mismatch" << std::endl;
        return 1;
    }

    std::cout << "PeerConnection audio path OK (" << received.size() << " samples)" << std::endl;
    return 0;
}