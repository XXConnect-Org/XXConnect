#pragma once

#include <rtc/rtc.hpp>
#include <memory>
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <nlohmann/json.hpp>
#include "SignalingClient.hpp"
#include "AudioRecorder.hpp"
#include "NoiseSuppressor.hpp"

class WebRTCCall {
public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Calling,
        InCall
    };

    using StateCallback = std::function<void(State)>;

    WebRTCCall(const std::string& local_id, const std::string& remote_id,
               const std::string& signaling_server);
    ~WebRTCCall();

    bool Initialize();
    void StartCall();
    void AnswerCall(const nlohmann::json& offer);
    void HangUp();

    void SetStateCallback(StateCallback callback);
    State GetCurrentState() const { return _state; }

    // Для тестирования
    std::shared_ptr<rtc::PeerConnection> GetPeerConnection() { return _peerConnection; }

private:
    void CreatePeerConnection();
    void SetupAudioTracks();
    void HandleSignalingMessage(const nlohmann::json& message);
    void OnDataChannel(std::shared_ptr<rtc::DataChannel> dc);
    void OnTrack(std::shared_ptr<rtc::Track> track);

    void ChangeState(State newState);

    // Signaling handlers
    void SendOffer();
    void SendAnswer();
    void SendIceCandidate(const rtc::Candidate& candidate);

    std::string _local_id;
    std::string _remote_id;
    std::string _signaling_server;

    std::shared_ptr<rtc::PeerConnection> _peerConnection;
    std::shared_ptr<rtc::Track> _audioTrack;
    std::shared_ptr<rtc::DataChannel> _dataChannel;

    std::unique_ptr<SignalingClient> _signalingClient;

    // Audio processing
    std::unique_ptr<AudioRecorder> _audioRecorder;
    std::unique_ptr<NoiseSuppressor> _noiseSuppressor;
    std::shared_ptr<rtc::Track> _remoteAudioTrack;

    StateCallback _stateCallback;
    std::atomic<State> _state;
    bool _isCaller;

    static const unsigned int AUDIO_SAMPLE_RATE = 48000;
};
