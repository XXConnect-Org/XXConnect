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

TEST(PeerConnectionFullTest, FullIntegrationTest) {
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

    AudioReceiver receiver(pcB, sampleRate);
    receiver.SetupDataChannel("audio");

    ASSERT_EQ(dcOpenFuture.wait_for(2s), std::future_status::ready)
        << "DataChannel did not open";

    std::vector<int16_t> receivedSamples;
    std::mutex receivedMutex;
    std::atomic<size_t> receivedChunks{0};

    receiver.SetOnAudioDataCallback([&receivedSamples, &receivedMutex, &receivedChunks](
                                        const int16_t* samples, size_t numSamples, unsigned int) {
        std::lock_guard<std::mutex> lock(receivedMutex);
        receivedSamples.insert(receivedSamples.end(), samples, samples + numSamples);
        receivedChunks++;
    });

    std::this_thread::sleep_for(200ms);

    recorder.Record(2000);

    std::this_thread::sleep_for(500ms);

    const auto& recordedData = recorder.GetAudioData();
    size_t recordedSamples = recordedData.size();

    std::lock_guard<std::mutex> lock(receivedMutex);
    size_t receivedSamplesCount = receivedSamples.size();
    size_t chunksReceived = receivedChunks.load();
    size_t chunksSent = sentChunks.load();

    const auto& receiverData = receiver.GetReceivedAudioData();
    size_t receiverSamplesCount = receiverData.size();

    ASSERT_GT(recordedSamples, 0u) << "No samples were recorded";

    ASSERT_GT(chunksSent, 0u) << "No data chunks were sent";

    size_t totalReceived = std::max(receivedSamplesCount, receiverSamplesCount);
    ASSERT_GT(totalReceived, 0u) << "No audio data received over WebRTC";

    size_t expectedMin = sampleRate * 15 / 10;
    size_t expectedMax = sampleRate * 25 / 10;

    EXPECT_GE(totalReceived, expectedMin)
        << "Received only " << totalReceived << " samples, expected at least " << expectedMin;
    
    if (totalReceived > expectedMax) {
        GTEST_LOG_(WARNING) << "Received " << totalReceived 
                            << " samples, expected at most " << expectedMax;
    }

    if (receivedSamplesCount > 0 && receiverSamplesCount > 0) {
        EXPECT_EQ(receivedSamplesCount, receiverSamplesCount)
            << "Mismatch between callback data (" << receivedSamplesCount
            << ") and receiver data (" << receiverSamplesCount << ")";
    }

    if (recordedSamples > 0 && totalReceived > 0) {
        double ratio = static_cast<double>(totalReceived) / static_cast<double>(recordedSamples);
        EXPECT_GE(ratio, 0.5) << "Received data ratio too low: " << ratio;
        EXPECT_LE(ratio, 2.0) << "Received data ratio too high: " << ratio;
    }
}

