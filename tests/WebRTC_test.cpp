#include "rtc/rtc.hpp"
#include "AudioSender/AudioSender.hpp"
#include "AudioRecorder/NoiseSuppressor.hpp"
#include "test_utils.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

TEST(PeerConnectionTest, BasicDataChannelTransmission) {
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

    std::promise<std::vector<int16_t>> receivedPromise;
    auto receivedFuture = receivedPromise.get_future();

    std::atomic<bool> dataChannelReceived{false};
    pcB->onDataChannel([&receivedPromise, &dataChannelReceived](std::shared_ptr<rtc::DataChannel> dc) {
        dataChannelReceived = true;
        
        dc->onOpen([dc]() {});
        
        dc->onMessage(
            [&receivedPromise](rtc::binary message) {
                const auto* samples = reinterpret_cast<const int16_t*>(message.data());
                size_t numSamples = message.size() / sizeof(int16_t);
                std::vector<int16_t> buffer(samples, samples + numSamples);
                receivedPromise.set_value(std::move(buffer));
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

    std::vector<int16_t> original(480);
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<int16_t>((i % 200) - 100);
    }
    
    const std::byte* begin = reinterpret_cast<const std::byte*>(original.data());
    const std::byte* end = begin + original.size() * sizeof(int16_t);
    rtc::binary frame(begin, end);
    dc->send(frame);

    ASSERT_EQ(receivedFuture.wait_for(5s), std::future_status::ready)
        << "No audio frame received over WebRTC";

    auto received = receivedFuture.get();
    EXPECT_EQ(received, original) << "Received frame size/data mismatch";
    EXPECT_EQ(received.size(), original.size()) << "Received frame size mismatch";
}