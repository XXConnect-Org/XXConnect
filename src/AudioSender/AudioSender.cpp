#include "AudioSender.hpp"

#include "../AudioRecorder/NoiseSuppressor.hpp"
#include "rtc/frameinfo.hpp"

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

    // Check if track is open before sending
    if (!_audioTrack->isOpen()) {
        return;
    }

    // Optionally denoise
    std::vector<int16_t> processed = _suppressor.IsEnabled()
        ? _suppressor.ProcessSamples(samples, numSamples, _sampleRate, _sampleRate)
        : std::vector<int16_t>(samples, samples + numSamples);

    // For media tracks, we need to use sendFrame with FrameInfo
    // Using a simple timestamp based on sample count
    static uint32_t timestamp = 0;
    timestamp += static_cast<uint32_t>(processed.size());
    
    const std::byte* begin = reinterpret_cast<const std::byte*>(processed.data());
    const std::byte* end   = begin + processed.size() * sizeof(int16_t);
    rtc::binary frame(begin, end);
    
    rtc::FrameInfo frameInfo(timestamp);
    _audioTrack->sendFrame(frame, frameInfo);
}


