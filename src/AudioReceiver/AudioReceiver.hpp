#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <cstdint>

#include "rtc/rtc.hpp"

// This class is responsible for:
//  - receiving incoming audio data from a libdatachannel PeerConnection
//  - exposing a callback that provides raw PCM buffers to the application
//  - optionally processing received audio through NoiseSuppressor
class AudioReceiver {
public:
    using PeerConnectionPtr = std::shared_ptr<rtc::PeerConnection>;
    
    // Callback type for received audio data: (samples, numSamples, sampleRate)
    using OnAudioDataCallback = std::function<void(const int16_t*, size_t, unsigned int)>;

    AudioReceiver(PeerConnectionPtr pc, unsigned int sampleRate);
    ~AudioReceiver();

    // Set up reception from a DataChannel (for raw PCM data)
    // Should be called after peer connection is established
    void SetupDataChannel(const std::string& channelLabel = "audio");

    // Set up reception from a Track (for media streams)
    // This will be called automatically when a track is received via onTrack callback
    void SetupTrack(std::shared_ptr<rtc::Track> track);

    // Set callback for received audio data
    void SetOnAudioDataCallback(OnAudioDataCallback callback) {
        _onAudioData = std::move(callback);
    }

    // Get accumulated received audio data
    const std::vector<int16_t>& GetReceivedAudioData() const { return _receivedData; }
    
    // Clear accumulated received audio data
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

