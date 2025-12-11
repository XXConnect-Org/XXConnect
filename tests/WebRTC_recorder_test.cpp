#include "rtc/rtc.hpp"
#include "AudioRecorder/AudioRecorder.hpp"
#include "AudioSender/AudioSender.hpp"
#include "AudioRecorder/NoiseSuppressor.hpp"
#include "test_utils.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

TEST(PeerConnectionRecorderTest, AudioRecorderToPeerConnectionIntegration) {
    auto pair = test_utils::CreateLoopbackPair();
    auto& pcA = pair.caller;
    auto& pcB = pair.callee;

    std::promise<void> connectedPromise;
    auto connectedFuture = connectedPromise.get_future();

    auto onConnected = [&connectedPromise](rtc::PeerConnection::State s) {
        if (s == rtc::PeerConnection::State::Connected) {
            connectedPromise.set_value();
        }
    };
    pcA->onStateChange(onConnected);
    pcB->onStateChange(onConnected);

    // Используем DataChannel для тестов, так как Track требует кодирование
    std::vector<int16_t> receivedSamples;
    std::mutex receivedMutex;
    std::atomic<bool> dataChannelReceived{false};
    std::atomic<size_t> receivedChunks{0};

    pcB->onDataChannel([&receivedSamples, &receivedMutex, &dataChannelReceived, &receivedChunks](
                           std::shared_ptr<rtc::DataChannel> dc) {
        dataChannelReceived = true;

        dc->onOpen([dc]() {});

        dc->onMessage(
            [&receivedSamples, &receivedMutex, &receivedChunks](rtc::binary message) {
                const auto* samples = reinterpret_cast<const int16_t*>(message.data());
                size_t numSamples = message.size() / sizeof(int16_t);
                
                std::lock_guard<std::mutex> lock(receivedMutex);
                receivedSamples.insert(receivedSamples.end(), samples, samples + numSamples);
                receivedChunks++;
            },
            [](rtc::string) {}
        );
    });

    auto dc = pcA->createDataChannel("audio");

    std::promise<void> dcOpenPromise;
    auto dcOpenFuture = dcOpenPromise.get_future();
    dc->onOpen([&dcOpenPromise]() {
        dcOpenPromise.set_value();
    });

    pcA->setLocalDescription(rtc::Description::Type::Offer);

    ASSERT_EQ(connectedFuture.wait_for(5s), std::future_status::ready)
        << "Peer connection did not reach Connected state";

    auto start = std::chrono::steady_clock::now();
    while (!dataChannelReceived && (std::chrono::steady_clock::now() - start) < 2s) {
        std::this_thread::sleep_for(100ms);
    }
    ASSERT_TRUE(dataChannelReceived) << "DataChannel was not received on callee side";

    ASSERT_EQ(dcOpenFuture.wait_for(2s), std::future_status::ready)
        << "DataChannel did not open";

    std::this_thread::sleep_for(200ms);

    auto nullWorker = std::make_shared<test_utils::NullSavingWorker>();
    AudioRecorder recorder(nullWorker);
    NoiseSuppressor suppressor;
    suppressor.SetEnabled(false);

    unsigned int sampleRate = recorder.GetSampleRate();

    recorder.SetOnBufferCallback([dc](const int16_t* samples, size_t numSamples, unsigned int) {
        if (!dc->isOpen()) {
            return;
        }

        const std::byte* begin = reinterpret_cast<const std::byte*>(samples);
        const std::byte* end = begin + numSamples * sizeof(int16_t);
        rtc::binary frame(begin, end);
        dc->send(frame);
    });

    recorder.Record(1000);

    std::this_thread::sleep_for(500ms);

    std::lock_guard<std::mutex> lock(receivedMutex);
    size_t totalReceived = receivedSamples.size();
    size_t chunks = receivedChunks.load();

    ASSERT_GT(totalReceived, 0u) << "No audio data received over WebRTC";
    
    size_t expectedMin = sampleRate * 8 / 10;
    EXPECT_GE(totalReceived, expectedMin)
        << "Received only " << totalReceived << " samples, expected at least " << expectedMin;
}

