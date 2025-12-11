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
#include <cmath>
#include <deque>

using namespace std::chrono_literals;

class TestAudioGenerator {
public:
    TestAudioGenerator(unsigned int sampleRate, double frequency = 440.0)
        : _sampleRate(sampleRate)
        , _frequency(frequency)
        , _phase(0.0)
        , _amplitude(0.3)
    {}

    void GenerateSamples(int16_t* buffer, size_t numSamples) {
        const double phaseIncrement = 2.0 * M_PI * _frequency / _sampleRate;
        
        for (size_t i = 0; i < numSamples; ++i) {
            double sample = std::sin(_phase) * _amplitude;
            buffer[i] = static_cast<int16_t>(sample * 32767.0);
            _phase += phaseIncrement;
            
            if (_phase > 2.0 * M_PI) {
                _phase -= 2.0 * M_PI;
            }
        }
    }

    void SetFrequency(double frequency) { _frequency = frequency; }
    void SetAmplitude(double amplitude) { _amplitude = std::max(0.0, std::min(1.0, amplitude)); }

private:
    unsigned int _sampleRate;
    double _frequency;
    double _phase;
    double _amplitude;
};

struct CallStatistics {
    std::atomic<size_t> packetsSent{0};
    std::atomic<size_t> packetsReceived{0};
    std::atomic<size_t> samplesSent{0};
    std::atomic<size_t> samplesReceived{0};
    std::atomic<size_t> packetsLost{0};
    
    std::deque<std::chrono::steady_clock::time_point> sendTimes;
    std::mutex sendTimesMutex;
    
    void RecordSend(size_t samples) {
        packetsSent++;
        samplesSent += samples;
        std::lock_guard<std::mutex> lock(sendTimesMutex);
        sendTimes.push_back(std::chrono::steady_clock::now());
        if (sendTimes.size() > 1000) {
            sendTimes.pop_front();
        }
    }
    
    void RecordReceive(size_t samples) {
        packetsReceived++;
        samplesReceived += samples;
    }
    
    double GetPacketLossRate() const {
        size_t sent = packetsSent.load();
        size_t received = packetsReceived.load();
        if (sent == 0) return 0.0;
        if (received > sent) return 0.0;
        return static_cast<double>(sent - received) / static_cast<double>(sent) * 100.0;
    }
    
    double GetThroughput() const {
        auto duration = std::chrono::steady_clock::now() - (sendTimes.empty() ? 
            std::chrono::steady_clock::now() : sendTimes.front());
        auto seconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0;
        if (seconds <= 0) return 0.0;
        return samplesReceived / seconds;
    }
};

TEST(PeerConnectionCallTest, RealTimeCallSimulation) {
    using namespace std::chrono;
    
    std::cout << "\n=== Real-time Call Simulation Test ===" << std::endl;
    std::cout << "Simulating a 5-second bidirectional call..." << std::endl;
    
    auto pair = test_utils::CreateLoopbackPair();
    auto& caller = pair.caller;
    auto& callee = pair.callee;

    std::promise<void> connectedPromise;
    auto connectedFuture = connectedPromise.get_future();

    auto onConnected = [&connectedPromise](rtc::PeerConnection::State s) {
        if (s == rtc::PeerConnection::State::Connected) {
            connectedPromise.set_value();
        }
    };
    caller->onStateChange(onConnected);
    callee->onStateChange(onConnected);

    auto callerDC = caller->createDataChannel("audio");
    auto calleeDC = callee->createDataChannel("audio");

    std::promise<void> callerDCOpen, calleeDCOpen;
    auto callerDCOpenFuture = callerDCOpen.get_future();
    auto calleeDCOpenFuture = calleeDCOpen.get_future();

    callerDC->onOpen([&callerDCOpen]() { callerDCOpen.set_value(); });
    calleeDC->onOpen([&calleeDCOpen]() { calleeDCOpen.set_value(); });

    caller->setLocalDescription(rtc::Description::Type::Offer);

    ASSERT_EQ(connectedFuture.wait_for(5s), std::future_status::ready)
        << "Peer connection did not reach Connected state";

    std::atomic<bool> calleeDCReceived{false};
    std::shared_ptr<rtc::DataChannel> receivedCallerDC;
    callee->onDataChannel([&calleeDCReceived, &receivedCallerDC](std::shared_ptr<rtc::DataChannel> dc) {
        if (dc->label() == "audio") {
            calleeDCReceived = true;
            receivedCallerDC = dc;
        }
    });

    auto start = steady_clock::now();
    while (!calleeDCReceived && (steady_clock::now() - start) < 2s) {
        std::this_thread::sleep_for(100ms);
    }
    ASSERT_TRUE(calleeDCReceived) << "Callee DataChannel not received";

    ASSERT_EQ(callerDCOpenFuture.wait_for(2s), std::future_status::ready)
        << "Caller DataChannel did not open";
    
    auto calleeReceivedDCOpenStart = steady_clock::now();
    while (receivedCallerDC && !receivedCallerDC->isOpen() && 
           (steady_clock::now() - calleeReceivedDCOpenStart) < 2s) {
        std::this_thread::sleep_for(50ms);
    }
    ASSERT_TRUE(receivedCallerDC && receivedCallerDC->isOpen()) 
        << "Received DataChannel on callee did not open";

    std::this_thread::sleep_for(200ms);

    auto nullWorkerA = std::make_shared<test_utils::NullSavingWorker>();
    AudioRecorder recorderA(nullWorkerA);
    NoiseSuppressor suppressorA;
    suppressorA.SetEnabled(true);

    unsigned int sampleRate = recorderA.GetSampleRate();
    TestAudioGenerator generatorA(sampleRate, 440.0);

    CallStatistics statsA;
    std::atomic<bool> stopGenerationA{false};

    std::vector<int16_t> sendBufferA(512);
    std::function<void(const int16_t*, size_t, unsigned int)> sendCallbackA = 
        [&](const int16_t* samples, size_t numSamples, unsigned int) {
            if (callerDC->isOpen()) {
                const std::byte* begin = reinterpret_cast<const std::byte*>(samples);
                const std::byte* end = begin + numSamples * sizeof(int16_t);
                rtc::binary frame(begin, end);
                callerDC->send(frame);
                statsA.RecordSend(numSamples);
            }
        };

    std::thread audioGeneratorA([&]() {
        const size_t bufferSize = 512;
        const auto bufferDuration = std::chrono::milliseconds(
            static_cast<long>(bufferSize * 1000.0 / sampleRate));
        
        std::this_thread::sleep_for(100ms);
        
        while (!stopGenerationA.load()) {
            generatorA.GenerateSamples(sendBufferA.data(), bufferSize);
            sendCallbackA(sendBufferA.data(), bufferSize, sampleRate);
            std::this_thread::sleep_for(bufferDuration);
        }
    });

    std::atomic<bool> callerDCReceived{false};
    std::shared_ptr<rtc::DataChannel> receivedCalleeDC;
    std::vector<int16_t> receivedA;
    std::mutex receivedAMutex;
    
    caller->onDataChannel([&callerDCReceived, &receivedCalleeDC, &receivedA, &receivedAMutex, &statsA](
                              std::shared_ptr<rtc::DataChannel> dc) {
        if (dc->label() == "audio") {
            callerDCReceived = true;
            receivedCalleeDC = dc;
            
            dc->onOpen([dc]() {});
            
            dc->onMessage(
                [&receivedA, &receivedAMutex, &statsA](rtc::binary message) {
                    const auto* samples = reinterpret_cast<const int16_t*>(message.data());
                    size_t numSamples = message.size() / sizeof(int16_t);
                    
                    std::lock_guard<std::mutex> lock(receivedAMutex);
                    receivedA.insert(receivedA.end(), samples, samples + numSamples);
                    statsA.RecordReceive(numSamples);
                },
                [](rtc::string) {}
            );
        }
    });
    
    auto callerDCReceivedStart = steady_clock::now();
    while (!callerDCReceived && (steady_clock::now() - callerDCReceivedStart) < 2s) {
        std::this_thread::sleep_for(100ms);
    }
    ASSERT_TRUE(callerDCReceived) << "Caller did not receive DataChannel from callee";
    
    auto callerReceivedDCOpenStart = steady_clock::now();
    while (receivedCalleeDC && !receivedCalleeDC->isOpen() && 
           (steady_clock::now() - callerReceivedDCOpenStart) < 2s) {
        std::this_thread::sleep_for(50ms);
    }
    ASSERT_TRUE(receivedCalleeDC && receivedCalleeDC->isOpen()) 
        << "Received DataChannel on caller did not open";

    auto nullWorkerB = std::make_shared<test_utils::NullSavingWorker>();
    AudioRecorder recorderB(nullWorkerB);
    NoiseSuppressor suppressorB;
    suppressorB.SetEnabled(true);

    // Разные частоты для различения потоков
    TestAudioGenerator generatorB(sampleRate, 523.25);

    CallStatistics statsB;
    std::atomic<bool> stopGenerationB{false};

    std::vector<int16_t> sendBufferB(512);
    std::function<void(const int16_t*, size_t, unsigned int)> sendCallbackB = 
        [&](const int16_t* samples, size_t numSamples, unsigned int) {
            if (calleeDC->isOpen()) {
                const std::byte* begin = reinterpret_cast<const std::byte*>(samples);
                const std::byte* end = begin + numSamples * sizeof(int16_t);
                rtc::binary frame(begin, end);
                calleeDC->send(frame);
                statsB.RecordSend(numSamples);
            }
        };

    std::thread audioGeneratorB([&]() {
        const size_t bufferSize = 512;
        const auto bufferDuration = std::chrono::milliseconds(
            static_cast<long>(bufferSize * 1000.0 / sampleRate));
        
        while (!stopGenerationB.load()) {
            generatorB.GenerateSamples(sendBufferB.data(), bufferSize);
            sendCallbackB(sendBufferB.data(), bufferSize, sampleRate);
            std::this_thread::sleep_for(bufferDuration);
        }
    });

    std::vector<int16_t> receivedB;
    std::mutex receivedBMutex;
    
    if (receivedCallerDC) {
        receivedCallerDC->onOpen([receivedCallerDC]() {});
        
        receivedCallerDC->onMessage(
            [&receivedB, &receivedBMutex, &statsB](rtc::binary message) {
                const auto* samples = reinterpret_cast<const int16_t*>(message.data());
                size_t numSamples = message.size() / sizeof(int16_t);
                
                std::lock_guard<std::mutex> lock(receivedBMutex);
                receivedB.insert(receivedB.end(), samples, samples + numSamples);
                statsB.RecordReceive(numSamples);
            },
            [](rtc::string) {}
        );
    }

    std::cout << "Starting call..." << std::endl;
    
    const auto callDuration = 5s;
    auto callStart = steady_clock::now();
    
    while (steady_clock::now() - callStart < callDuration) {
        std::this_thread::sleep_for(500ms);
        
        auto elapsed = duration_cast<milliseconds>(steady_clock::now() - callStart).count();
        std::cout << "[" << elapsed << "ms] "
                  << "A→B: " << statsA.packetsSent.load() << " pkts, "
                  << "B→A: " << statsB.packetsSent.load() << " pkts" << std::endl;
    }

    stopGenerationA = true;
    stopGenerationB = true;
    
    if (audioGeneratorA.joinable()) {
        audioGeneratorA.join();
    }
    if (audioGeneratorB.joinable()) {
        audioGeneratorB.join();
    }

    // Ждем завершения передачи
    std::this_thread::sleep_for(500ms);

    std::cout << "\n=== Call Statistics ===" << std::endl;
    
    std::lock_guard<std::mutex> lockA(receivedAMutex);
    std::lock_guard<std::mutex> lockB(receivedBMutex);
    
    size_t totalReceivedA = receivedA.size();
    size_t totalReceivedB = receivedB.size();
    
    std::cout << "Participant A (Caller):" << std::endl;
    std::cout << "  Sent: " << statsA.samplesSent.load() << " samples in " 
              << statsA.packetsSent.load() << " packets" << std::endl;
    std::cout << "  Received: " << totalReceivedA << " samples in " 
              << statsA.packetsReceived.load() << " packets" << std::endl;
    std::cout << "  Packet loss: " << statsA.GetPacketLossRate() << "%" << std::endl;
    std::cout << "  Throughput: " << statsA.GetThroughput() << " samples/sec" << std::endl;
    
    std::cout << "Participant B (Callee):" << std::endl;
    std::cout << "  Sent: " << statsB.samplesSent.load() << " samples in " 
              << statsB.packetsSent.load() << " packets" << std::endl;
    std::cout << "  Received: " << totalReceivedB << " samples in " 
              << statsB.packetsReceived.load() << " packets" << std::endl;
    std::cout << "  Packet loss: " << statsB.GetPacketLossRate() << "%" << std::endl;
    std::cout << "  Throughput: " << statsB.GetThroughput() << " samples/sec" << std::endl;

    EXPECT_GT(statsA.packetsSent.load(), 0u) << "Caller did not send any packets";
    EXPECT_GT(statsB.packetsSent.load(), 0u) << "Callee did not send any packets";

    EXPECT_GT(totalReceivedA, 0u) << "Caller did not receive any audio";
    EXPECT_GT(totalReceivedB, 0u) << "Callee did not receive any audio";

    size_t expectedMinA = sampleRate * 4;
    size_t expectedMinB = sampleRate * 4;
    EXPECT_GE(totalReceivedA, expectedMinA)
        << "Caller received too few samples: " << totalReceivedA 
        << " < " << expectedMinA;
    EXPECT_GE(totalReceivedB, expectedMinB)
        << "Callee received too few samples: " << totalReceivedB 
        << " < " << expectedMinB;

    EXPECT_LT(statsA.GetPacketLossRate(), 5.0)
        << "Caller packet loss too high: " << statsA.GetPacketLossRate() << "%";
    EXPECT_LT(statsB.GetPacketLossRate(), 5.0)
        << "Callee packet loss too high: " << statsB.GetPacketLossRate() << "%";

    double ratioA = static_cast<double>(totalReceivedA) / static_cast<double>(statsB.samplesSent.load());
    double ratioB = static_cast<double>(totalReceivedB) / static_cast<double>(statsA.samplesSent.load());
    
    EXPECT_GE(ratioA, 0.7) << "Caller received too little data from callee: " << ratioA;
    EXPECT_GE(ratioB, 0.7) << "Callee received too little data from caller: " << ratioB;

    std::cout << "\n=== Call Test PASSED ===" << std::endl;
}
