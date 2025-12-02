#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <cstdint>

#include "rtc/rtc.hpp"

class AudioRecorder;
class NoiseSuppressor;

// This class is responsible for:
//  - adding an outgoing audio track to a libdatachannel PeerConnection
//  - exposing a callback that accepts raw PCM buffers from AudioRecorder
//  - optionally passing them through NoiseSuppressor before sending
class AudioSender {
public:
    using PeerConnectionPtr = std::shared_ptr<rtc::PeerConnection>;

    AudioSender(PeerConnectionPtr pc,
                NoiseSuppressor& suppressor,
                unsigned int sampleRate);

    // Attach an audio track (Opus) to the peer connection.
    // Should be called before you create the offer / start negotiation.
    void AttachTrack();

    // Feed a captured audio buffer (int16 mono PCM) into the WebRTC pipeline.
    // Typically called from AudioRecorder's capture callback.
    void OnAudioBuffer(const int16_t* samples, size_t numSamples);

    unsigned int GetSampleRate() const { return _sampleRate; }

private:
    PeerConnectionPtr _pc;
    std::shared_ptr<rtc::Track> _audioTrack;
    NoiseSuppressor& _suppressor;
    unsigned int _sampleRate;
};


