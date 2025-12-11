#include "rtc/rtc.hpp"
#include "AudioRecorder/AudioRecorder.hpp"
#include "AudioSender/AudioSender.hpp"
#include "AudioReceiver/AudioReceiver.hpp"
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

TEST(PeerConnectionReceiverTest, AudioReceiverWithPeerConnection) {
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

    auto dc = pcA->createDataChannel("audio");

    std::promise<void> dcOpenPromise;
    auto dcOpenFuture = dcOpenPromise.get_future();
    dc->onOpen([&dcOpenPromise]() {
        dcOpenPromise.set_value();
    });

    pcA->setLocalDescription(rtc::Description::Type::Offer);

    ASSERT_EQ(connectedFuture.wait_for(5s), std::future_status::ready)
        << "Peer connection did not reach Connected state";

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

    AudioReceiver receiver(pcB, sampleRate);
    receiver.SetupDataChannel("audio");

    std::vector<int16_t> receivedViaCallback;
    std::mutex callbackMutex;
    std::atomic<size_t> callbackCount{0};
    
    receiver.SetOnAudioDataCallback([&receivedViaCallback, &callbackMutex, &callbackCount](
                                        const int16_t* samples, size_t numSamples, unsigned int) {
        std::lock_guard<std::mutex> lock(callbackMutex);
        receivedViaCallback.insert(receivedViaCallback.end(), samples, samples + numSamples);
        callbackCount++;
    });

    std::this_thread::sleep_for(200ms);

    recorder.Record(1000);

    std::this_thread::sleep_for(500ms);

    std::lock_guard<std::mutex> lock(callbackMutex);
    size_t totalReceivedViaCallback = receivedViaCallback.size();
    size_t callbacks = callbackCount.load();

    const auto& receivedViaReceiver = receiver.GetReceivedAudioData();
    size_t totalReceivedViaReceiver = receivedViaReceiver.size();

    size_t totalReceived = std::max(totalReceivedViaCallback, totalReceivedViaReceiver);
    ASSERT_GT(totalReceived, 0u) << "No audio data received over WebRTC";

    size_t expectedMin = sampleRate * 8 / 10;
    EXPECT_GE(totalReceived, expectedMin)
        << "Received only " << totalReceived << " samples, expected at least " << expectedMin;

    if (totalReceivedViaCallback > 0 && totalReceivedViaReceiver > 0) {
        EXPECT_EQ(totalReceivedViaCallback, totalReceivedViaReceiver)
            << "Mismatch between callback data (" << totalReceivedViaCallback
            << ") and receiver data (" << totalReceivedViaReceiver << ")";
    }
}

