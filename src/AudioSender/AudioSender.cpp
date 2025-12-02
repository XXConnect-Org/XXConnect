#include "AudioSender.hpp"

#include "../AudioRecorder/NoiseSuppressor.hpp"

AudioSender::AudioSender(PeerConnectionPtr pc,
                         NoiseSuppressor& suppressor,
                         unsigned int sampleRate)
    : _pc(std::move(pc))
    , _suppressor(suppressor)
    , _sampleRate(sampleRate)
{
}

void AudioSender::AttachTrack() {
    if (!_pc) return;

    // Describe an outgoing audio track (Opus)
    rtc::Description::Audio audioDesc("audio", rtc::Description::Direction::SendOnly);
    audioDesc.addOpusCodec(111);
    audioDesc.setBitrate(64000); // 64 kbps as a starting point

    _audioTrack = _pc->addTrack(audioDesc);
}

void AudioSender::OnAudioBuffer(const int16_t* samples, size_t numSamples) {
    if (!_pc || !_audioTrack || !samples || numSamples == 0) {
        return;
    }

    // Optionally denoise
    std::vector<int16_t> processed = _suppressor.IsEnabled()
        ? _suppressor.ProcessSamples(samples, numSamples, _sampleRate, _sampleRate)
        : std::vector<int16_t>(samples, samples + numSamples);

    // libdatachannel transports media over RTP; for simple custom sources one
    // common pattern is to send the raw PCM frames as binary messages on the track.
    // Exact handling on the receiver side is up to your application.

    const std::byte* begin = reinterpret_cast<const std::byte*>(processed.data());
    const std::byte* end   = begin + processed.size() * sizeof(int16_t);
    rtc::binary frame(begin, end);

    _audioTrack->send(frame);
}


