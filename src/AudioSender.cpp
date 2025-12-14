#include "../include/AudioSender.hpp"
#include "../include/NoiseSuppressor.hpp"
#include "rtc/frameinfo.hpp"
#include "../include/debug_log.hpp"

AudioSender::AudioSender(PeerConnectionPtr pc,
                         NoiseSuppressor& suppressor,
                         unsigned int sampleRate)
    : _pc(std::move(pc))
    , _suppressor(suppressor)
    , _sampleRate(sampleRate)
    , _timestamp(0)
{
}

void AudioSender::AttachTrack() {
    if (!_pc) return;

    rtc::Description::Audio audioDesc("audio", rtc::Description::Direction::SendOnly);
    audioDesc.addOpusCodec(111);
    audioDesc.setBitrate(64000);

    _audioTrack = _pc->addTrack(audioDesc);
}

void AudioSender::OnAudioBuffer(const int16_t* samples, size_t numSamples) {
    if (!_pc || !_audioTrack || !samples || numSamples == 0) {
        return;
    }

    if (!_audioTrack->isOpen()) {
        return;
    }

    std::vector<int16_t> processed;
    if (_suppressor.IsEnabled()) {
        processed = _suppressor.ProcessSamples(samples, numSamples, _sampleRate, _sampleRate);
        // Если шумоподавление вернуло пустой результат (буферизация), пропускаем буфер
        if (processed.empty()) {
            return;
        }
    } else {
        processed.assign(samples, samples + numSamples);
    }

    _timestamp += static_cast<uint32_t>(processed.size());

    const std::byte* begin = reinterpret_cast<const std::byte*>(processed.data());
    const std::byte* end   = begin + processed.size() * sizeof(int16_t);
    rtc::binary frame(begin, end);

    rtc::FrameInfo frameInfo(_timestamp);
    _audioTrack->sendFrame(frame, frameInfo);
}


