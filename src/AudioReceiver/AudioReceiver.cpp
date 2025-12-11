#include "AudioReceiver.hpp"
#include "../common/debug_log.hpp"

AudioReceiver::AudioReceiver(PeerConnectionPtr pc, unsigned int sampleRate)
    : _pc(std::move(pc))
    , _sampleRate(sampleRate)
{
}

AudioReceiver::~AudioReceiver() = default;

void AudioReceiver::SetupDataChannel(const std::string& channelLabel) {
    if (!_pc) return;

    _pc->onDataChannel([this, channelLabel](std::shared_ptr<rtc::DataChannel> dc) {
        if (dc->label() == channelLabel) {
            DEBUG_LOG("AudioReceiver: DataChannel '" << channelLabel << "' received" << DEBUG_LOG_ENDL);
            _dataChannel = dc;

            dc->onOpen([this, dc]() {
                DEBUG_LOG("AudioReceiver: DataChannel opened" << DEBUG_LOG_ENDL);
            });

            dc->onMessage(
                [this](rtc::binary message) {
                    const auto* samples = reinterpret_cast<const int16_t*>(message.data());
                    size_t numSamples = message.size() / sizeof(int16_t);
                    ProcessReceivedSamples(samples, numSamples);
                },
                [](rtc::string) {}
            );

            dc->onClosed([this]() {
                DEBUG_LOG("AudioReceiver: DataChannel closed" << DEBUG_LOG_ENDL);
                _dataChannel.reset();
            });
        }
    });
}

void AudioReceiver::SetupTrack(std::shared_ptr<rtc::Track> track) {
    if (!track) return;

    DEBUG_LOG("AudioReceiver: Setting up track" << DEBUG_LOG_ENDL);
    _audioTrack = track;

    track->onOpen([this]() {
        DEBUG_LOG("AudioReceiver: Track opened" << DEBUG_LOG_ENDL);
    });

    track->onFrame([this](rtc::binary data, rtc::FrameInfo info) {
        const auto* samples = reinterpret_cast<const int16_t*>(data.data());
        size_t numSamples = data.size() / sizeof(int16_t);
        ProcessReceivedSamples(samples, numSamples);
    });

    track->onClosed([this]() {
        DEBUG_LOG("AudioReceiver: Track closed" << DEBUG_LOG_ENDL);
        _audioTrack.reset();
    });
}

void AudioReceiver::ProcessReceivedSamples(const int16_t* samples, size_t numSamples) {
    if (!samples || numSamples == 0) {
        return;
    }

    _receivedData.insert(_receivedData.end(), samples, samples + numSamples);

    if (_onAudioData) {
        _onAudioData(samples, numSamples, _sampleRate);
    }
}

