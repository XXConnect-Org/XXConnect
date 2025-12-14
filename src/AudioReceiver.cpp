#include "../include/AudioReceiver.hpp"
#include "../include/debug_log.hpp"
#include <algorithm>

AudioReceiver::AudioReceiver(PeerConnectionPtr pc, unsigned int sampleRate)
    : _pc(std::move(pc))
    , _sampleRate(sampleRate)
    , _jitterBufferEnabled(false)
    , _minJitterPackets(3)
    , _maxJitterPackets(10)
    , _expectedTimestamp(0)
    , _firstPacket(true)
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
        if (_jitterBufferEnabled) {
            ProcessReceivedSamplesWithJitter(samples, numSamples, info.timestamp);
        } else {
            ProcessReceivedSamples(samples, numSamples);
        }
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

void AudioReceiver::SetJitterBufferSize(size_t minPackets, size_t maxPackets) {
    std::lock_guard<std::mutex> lock(_jitterBufferMutex);
    _minJitterPackets = minPackets;
    _maxJitterPackets = maxPackets;
}

void AudioReceiver::ProcessReceivedSamplesWithJitter(const int16_t* samples, size_t numSamples, uint32_t timestamp) {
    if (!samples || numSamples == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(_jitterBufferMutex);

    AudioPacket packet;
    packet.data.assign(samples, samples + numSamples);
    packet.timestamp = timestamp;
    packet.receivedTime = std::chrono::steady_clock::now();

    auto it = _jitterBuffer.begin();
    while (it != _jitterBuffer.end() && it->timestamp < timestamp) {
        ++it;
    }
    _jitterBuffer.insert(it, packet);

    while (_jitterBuffer.size() > _maxJitterPackets) {
        _jitterBuffer.pop_front();
    }

    if (_firstPacket) {
        _expectedTimestamp = timestamp;
        _firstPacket = false;
        _lastOutputTime = std::chrono::steady_clock::now();
    }

    _receivedData.insert(_receivedData.end(), samples, samples + numSamples);
}

bool AudioReceiver::GetNextAudioPacket(std::vector<int16_t>& output, size_t requestedSamples) {
    if (!_jitterBufferEnabled) {
        return false;
    }

    std::lock_guard<std::mutex> lock(_jitterBufferMutex);

    if (_jitterBuffer.size() < _minJitterPackets) {
        return false;
    }

    if (_jitterBuffer.empty()) {
        return false;
    }

    auto it = _jitterBuffer.begin();
    while (it != _jitterBuffer.end() && it->timestamp < _expectedTimestamp) {
        ++it;
    }

    if (it == _jitterBuffer.end() && !_jitterBuffer.empty()) {
        it = _jitterBuffer.begin();
    }

    if (it != _jitterBuffer.end()) {
        output = it->data;
        _expectedTimestamp = it->timestamp + static_cast<uint32_t>(it->data.size());
        _jitterBuffer.erase(it);
        return true;
    }

    return false;
}

