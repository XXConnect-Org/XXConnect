#pragma once

#include <memory>
#include <functional>
#include <vector>
#include <cstdint>

#include "rtc/rtc.hpp"

class AudioRecorder;
class NoiseSuppressor;

// Отправка аудио через peer-connection
class AudioSender {
public:
    using PeerConnectionPtr = std::shared_ptr<rtc::PeerConnection>;

    AudioSender(PeerConnectionPtr pc,
                NoiseSuppressor& suppressor,
                unsigned int sampleRate);

    // Добавляет аудио-трек к peer-connection (вызывать до создания offer)
    void AttachTrack();

    // Отправка буфера аудио (вызывается из callback AudioRecorder)
    void OnAudioBuffer(const int16_t* samples, size_t numSamples);

    unsigned int GetSampleRate() const { return _sampleRate; }

private:
    PeerConnectionPtr _pc;
    std::shared_ptr<rtc::Track> _audioTrack;
    NoiseSuppressor& _suppressor;
    unsigned int _sampleRate;
};


