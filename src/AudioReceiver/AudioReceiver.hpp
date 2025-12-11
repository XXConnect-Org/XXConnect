#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <cstdint>

#include "rtc/rtc.hpp"

// Прием аудио из peer-connection
class AudioReceiver {
public:
    using PeerConnectionPtr = std::shared_ptr<rtc::PeerConnection>;
    using OnAudioDataCallback = std::function<void(const int16_t*, size_t, unsigned int)>;

    AudioReceiver(PeerConnectionPtr pc, unsigned int sampleRate);
    ~AudioReceiver();

    // Настройка приема через DataChannel (вызывать после установки соединения)
    void SetupDataChannel(const std::string& channelLabel = "audio");

    // Настройка приема через Track
    void SetupTrack(std::shared_ptr<rtc::Track> track);

    void SetOnAudioDataCallback(OnAudioDataCallback callback) {
        _onAudioData = std::move(callback);
    }

    const std::vector<int16_t>& GetReceivedAudioData() const { return _receivedData; }
    void ClearReceivedData() { _receivedData.clear(); }

    unsigned int GetSampleRate() const { return _sampleRate; }

private:
    void ProcessReceivedSamples(const int16_t* samples, size_t numSamples);

    PeerConnectionPtr _pc;
    std::shared_ptr<rtc::DataChannel> _dataChannel;
    std::shared_ptr<rtc::Track> _audioTrack;
    OnAudioDataCallback _onAudioData;
    std::vector<int16_t> _receivedData;
    unsigned int _sampleRate;
};

