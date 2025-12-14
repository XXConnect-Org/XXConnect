#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <cstdint>
#include <deque>
#include <mutex>
#include <chrono>

#include "rtc/rtc.hpp"

class AudioReceiver {
public:
    using PeerConnectionPtr = std::shared_ptr<rtc::PeerConnection>;
    using OnAudioDataCallback = std::function<void(const int16_t*, size_t, unsigned int)>;

    AudioReceiver(PeerConnectionPtr pc, unsigned int sampleRate);
    ~AudioReceiver();

    // Вызывать после установки соединения
    void SetupDataChannel(const std::string& channelLabel = "audio");

    void SetupTrack(std::shared_ptr<rtc::Track> track);

    void SetOnAudioDataCallback(OnAudioDataCallback callback) {
        _onAudioData = std::move(callback);
    }

    const std::vector<int16_t>& GetReceivedAudioData() const { return _receivedData; }
    void ClearReceivedData() { _receivedData.clear(); }

    unsigned int GetSampleRate() const { return _sampleRate; }

    void SetJitterBufferEnabled(bool enabled) { _jitterBufferEnabled = enabled; }
    void SetJitterBufferSize(size_t minPackets, size_t maxPackets);

    bool GetNextAudioPacket(std::vector<int16_t>& output, size_t requestedSamples);

private:
    void ProcessReceivedSamples(const int16_t* samples, size_t numSamples);
    void ProcessReceivedSamplesWithJitter(const int16_t* samples, size_t numSamples, uint32_t timestamp);

    struct AudioPacket {
        std::vector<int16_t> data;
        uint32_t timestamp;
        std::chrono::steady_clock::time_point receivedTime;
    };

    PeerConnectionPtr _pc;
    std::shared_ptr<rtc::DataChannel> _dataChannel;
    std::shared_ptr<rtc::Track> _audioTrack;
    OnAudioDataCallback _onAudioData;
    std::vector<int16_t> _receivedData;
    unsigned int _sampleRate;

    bool _jitterBufferEnabled;
    size_t _minJitterPackets;
    size_t _maxJitterPackets;
    std::deque<AudioPacket> _jitterBuffer;
    std::mutex _jitterBufferMutex;
    uint32_t _expectedTimestamp;
    bool _firstPacket;
    std::chrono::steady_clock::time_point _lastOutputTime;
};

