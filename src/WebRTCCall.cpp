#include "../include/WebRTCCall.hpp"
#include <iostream>
#include <thread>
#include <chrono>

WebRTCCall::WebRTCCall(const std::string& local_id, const std::string& remote_id,
                       const std::string& signaling_server)
    : _local_id(local_id), _remote_id(remote_id),
      _signaling_server(signaling_server),
      _state(State::Disconnected), _isCaller(false) {

    // Инициализация логгера libdatachannel
    rtc::InitLogger(rtc::LogLevel::None);
}

WebRTCCall::~WebRTCCall() {
    HangUp();
}

bool WebRTCCall::Initialize() {
    if (_state != State::Disconnected) {
        return false;
    }

    ChangeState(State::Connecting);

    // Инициализация сигнального клиента
    _signalingClient = std::make_unique<SignalingClient>(_signaling_server, _local_id);
    _signalingClient->SetMessageCallback([this](const nlohmann::json& message) {
        HandleSignalingMessage(message);
    });

    if (!_signalingClient->Connect()) {
        std::cerr << "Failed to connect to signaling server" << std::endl;
        ChangeState(State::Disconnected);
        return false;
    }

    // Инициализация шумоподавления
    _noiseSuppressor = std::make_unique<NoiseSuppressor>();
    _noiseSuppressor->SetEnabled(true);

    // Создание PeerConnection
    CreatePeerConnection();

    ChangeState(State::Connected);
    return true;
}

void WebRTCCall::CreatePeerConnection() {
    rtc::Configuration config;
    config.enableIceTcp = false;

    _peerConnection = std::make_shared<rtc::PeerConnection>(config);

    _peerConnection->onStateChange([this](rtc::PeerConnection::State state) {
        std::cout << "PeerConnection state changed: " << static_cast<int>(state) << std::endl;
        if (state == rtc::PeerConnection::State::Failed) {
            HangUp();
        }
    });

    _peerConnection->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
        std::cout << "ICE gathering state: " << static_cast<int>(state) << std::endl;
    });

    _peerConnection->onDataChannel([this](std::shared_ptr<rtc::DataChannel> dc) {
        OnDataChannel(dc);
    });

    _peerConnection->onTrack([this](std::shared_ptr<rtc::Track> track) {
        OnTrack(track);
    });

    // Настройка ICE кандидатов
    _peerConnection->onLocalCandidate([this](const rtc::Candidate& candidate) {
        SendIceCandidate(candidate);
    });
}

void WebRTCCall::SetupAudioTracks() {
    // Создание аудио трека для отправки
    _audioTrack = _peerConnection->addTrack(rtc::Description::Audio("audio"));

    // Настройка получения аудио с микрофона (упрощенная реализация)
    // В реальном приложении здесь будет интеграция с AudioRecorder
    std::cout << "Audio tracks setup completed" << std::endl;
}

void WebRTCCall::StartCall() {
    if (_state != State::Connected) {
        std::cerr << "Cannot start call: not connected" << std::endl;
        return;
    }

    _isCaller = true;
    ChangeState(State::Calling);
    SetupAudioTracks();
    SendOffer();
}

void WebRTCCall::AnswerCall(const nlohmann::json& offer) {
    if (_state != State::Connected) {
        std::cerr << "Cannot answer call: not connected" << std::endl;
        return;
    }

    _isCaller = false;
    ChangeState(State::InCall);

    try {
        std::string sdp = offer.value("sdp", "");
        _peerConnection->setRemoteDescription(rtc::Description(sdp, "offer"));
        SetupAudioTracks();
        SendAnswer();
    } catch (const std::exception& e) {
        std::cerr << "Error answering call: " << e.what() << std::endl;
        HangUp();
    }
}

void WebRTCCall::SendOffer() {
    try {
        auto offer = _peerConnection->createOffer();
        _peerConnection->setLocalDescription(offer);

        nlohmann::json message = {
            {"type", "offer"},
            {"id", _remote_id},
            {"sdp", std::string(offer)}
        };

        _signalingClient->SendMessage(message);
    } catch (const std::exception& e) {
        std::cerr << "Error sending offer: " << e.what() << std::endl;
        HangUp();
    }
}

void WebRTCCall::SendAnswer() {
    try {
        auto answer = _peerConnection->createAnswer();
        _peerConnection->setLocalDescription(answer);

        nlohmann::json message = {
            {"type", "answer"},
            {"id", _remote_id},
            {"sdp", std::string(answer)}
        };

        _signalingClient->SendMessage(message);
    } catch (const std::exception& e) {
        std::cerr << "Error sending answer: " << e.what() << std::endl;
        HangUp();
    }
}

void WebRTCCall::SendIceCandidate(const rtc::Candidate& candidate) {
    nlohmann::json message = {
        {"type", "candidate"},
        {"id", _remote_id},
        {"candidate", candidate.candidate()},
        {"mid", candidate.mid()},
        {"mlineIndex", candidate.mlineIndex()}
    };

    _signalingClient->SendMessage(message);
}

void WebRTCCall::HandleSignalingMessage(const nlohmann::json& message) {
    try {
        std::string type = message.value("type", "");
        std::string from_id = message.value("id", "");

        if (from_id != _remote_id) {
            return; // Игнорируем сообщения не от нашего партнера
        }

        if (type == "offer") {
            if (_state == State::Connected || _state == State::Calling) {
                AnswerCall(message);
            }
        }
        else if (type == "answer") {
            if (_isCaller && _state == State::Calling) {
                std::string sdp = message.value("sdp", "");
                _peerConnection->setRemoteDescription(rtc::Description(sdp, "answer"));
                ChangeState(State::InCall);
            }
        }
        else if (type == "candidate") {
            std::string candidate_str = message.value("candidate", "");
            std::string mid = message.value("mid", "");
            int mlineIndex = message.value("mlineIndex", 0);

            rtc::Candidate candidate(candidate_str, mid, mlineIndex);
            _peerConnection->addRemoteCandidate(candidate);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error handling signaling message: " << e.what() << std::endl;
    }
}

void WebRTCCall::OnDataChannel(std::shared_ptr<rtc::DataChannel> dc) {
    std::cout << "New DataChannel: " << dc->label() << std::endl;
    _dataChannel = dc;

    _dataChannel->onMessage([](const std::variant<rtc::binary, rtc::string>& message) {
        if (std::holds_alternative<rtc::binary>(message)) {
            auto data = std::get<rtc::binary>(message);
            std::cout << "Received binary data: " << data.size() << " bytes" << std::endl;
        } else {
            auto text = std::get<rtc::string>(message);
            std::cout << "Received text: " << text << std::endl;
        }
    });

    _dataChannel->onOpen([dc]() {
        std::cout << "DataChannel opened" << std::endl;
        // Отправляем тестовое сообщение
        dc->send("Hello from " + dc->label());
    });
}

void WebRTCCall::OnTrack(std::shared_ptr<rtc::Track> track) {
    std::cout << "New Track received: " << track->mid() << " (" << track->description().type() << ")" << std::endl;

    if (track->description().type() == "audio") {
        _remoteAudioTrack = track;

        track->onMessage([this](const std::variant<rtc::binary, rtc::string>& message) {
            if (std::holds_alternative<rtc::binary>(message)) {
                auto data = std::get<rtc::binary>(message);
                // Здесь можно обрабатывать полученные аудио данные
                std::cout << "Received audio data: " << data.size() << " bytes" << std::endl;
            }
        });
    }
}

void WebRTCCall::HangUp() {
    ChangeState(State::Disconnected);

    if (_peerConnection) {
        try {
            _peerConnection->close();
        } catch (...) {
            // Игнорируем ошибки при закрытии
        }
        _peerConnection = nullptr;
    }

    if (_signalingClient) {
        _signalingClient->Disconnect();
    }

    _audioTrack.reset();
    _dataChannel.reset();
    _remoteAudioTrack.reset();
}

void WebRTCCall::ChangeState(State newState) {
    State oldState = _state.exchange(newState);
    if (oldState != newState && _stateCallback) {
        _stateCallback(newState);
    }
}

void WebRTCCall::SetStateCallback(StateCallback callback) {
    _stateCallback = std::move(callback);
}
