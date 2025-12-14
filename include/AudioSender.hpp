#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <cstdint>

#include "rtc/rtc.hpp"

class AudioRecorder;
class NoiseSuppressor;

class AudioSender {
public:
    using PeerConnectionPtr = std::shared_ptr<rtc::PeerConnection>;

    AudioSender(PeerConnectionPtr pc,
                NoiseSuppressor& suppressor,
                unsigned int sampleRate);

    // Вызывать до создания offer
    void AttachTrack();

    void OnAudioBuffer(const int16_t* samples, size_t numSamples);

    unsigned int GetSampleRate() const { return _sampleRate; }
    
    bool IsTrackOpen() const { return _audioTrack && _audioTrack->isOpen(); }

private:
    PeerConnectionPtr _pc;
    std::shared_ptr<rtc::Track> _audioTrack;
    NoiseSuppressor& _suppressor;
    unsigned int _sampleRate;
    uint32_t _timestamp;
};


