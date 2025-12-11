#include "AudioReceiver.hpp"

#include <iostream>

AudioReceiver::AudioReceiver(PeerConnectionPtr pc, unsigned int sampleRate)
    : _pc(std::move(pc))
    , _sampleRate(sampleRate)
{
}

AudioReceiver::~AudioReceiver() = default;

void AudioReceiver::SetupDataChannel(const std::string& channelLabel) {
    if (!_pc) return;

    // Set up callback for when DataChannel is received
    _pc->onDataChannel([this, channelLabel](std::shared_ptr<rtc::DataChannel> dc) {
        if (dc->label() == channelLabel) {
            std::cout << "AudioReceiver: DataChannel '" << channelLabel << "' received" << std::endl;
            _dataChannel = dc;

            dc->onOpen([this, dc]() {
                std::cout << "AudioReceiver: DataChannel opened" << std::endl;
            });

            dc->onMessage(
                [this](rtc::binary message) {
                    // Convert received binary data to PCM samples
                    const auto* samples = reinterpret_cast<const int16_t*>(message.data());
                    size_t numSamples = message.size() / sizeof(int16_t);
                    ProcessReceivedSamples(samples, numSamples);
                },
                [](rtc::string) {
                    // Ignore string messages
                }
            );

            dc->onClosed([this]() {
                std::cout << "AudioReceiver: DataChannel closed" << std::endl;
                _dataChannel.reset();
            });
        }
    });
}

void AudioReceiver::SetupTrack(std::shared_ptr<rtc::Track> track) {
    if (!track) return;

    std::cout << "AudioReceiver: Setting up track" << std::endl;
    _audioTrack = track;

    track->onOpen([this]() {
        std::cout << "AudioReceiver: Track opened" << std::endl;
    });

    // For media tracks, use onFrame callback
    track->onFrame([this](rtc::binary data, rtc::FrameInfo info) {
        // Convert received frame data to PCM samples
        const auto* samples = reinterpret_cast<const int16_t*>(data.data());
        size_t numSamples = data.size() / sizeof(int16_t);
        ProcessReceivedSamples(samples, numSamples);
    });

    track->onClosed([this]() {
        std::cout << "AudioReceiver: Track closed" << std::endl;
        _audioTrack.reset();
    });
}

void AudioReceiver::ProcessReceivedSamples(const int16_t* samples, size_t numSamples) {
    if (!samples || numSamples == 0) {
        return;
    }

    // Store received samples
    _receivedData.insert(_receivedData.end(), samples, samples + numSamples);

    // Call user callback if set
    if (_onAudioData) {
        _onAudioData(samples, numSamples, _sampleRate);
    }
}

