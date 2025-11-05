#include "client_connection_manager.h"

#include <nlohmann/json.hpp>

using namespace connection_management;
using json = nlohmann::json;
using namespace nlohmann::literals;


ConnectionManager::ConnectionManager(const Client& client, rtc::Configuration config)
    : client_(client), config_(std::move(config)) {
    SetUpWsCallbacks();
}

// methods for managing ws_ connection to signalling server
bool ConnectionManager::OpenForConnections(const ConnectionConfig& config) {
    std::promise<void> promise;
    std::future<void> future = promise.get_future();

    ws_->onOpen([&promise]() { promise.set_value(); });
    ws_->onError([&promise](const std::string& error) {
        promise.set_exception(std::make_exception_ptr(std::runtime_error(error)));
    });

    std::string ws_pref = config.server.find("://") == std::string::npos ? "ws://" : "";
    std::string url = ws_pref + config.server + ":" + config.port + "/" + client_.Id();
    ws_->open(url);

    future.get();
    return true;
}

void ConnectionManager::CloseForConnections() { ws_->close(); }

// methods for managing peer connections between client_ and other clients

bool ConnectionManager::HasConnectionWith(const Client& client) {
    return connections_.find(client.Id()) != connections_.end();
}

ConnectionManager::connect_ptr ConnectionManager::GetConnectionWith(const Client& client) {
    if (!HasConnectionWith(client)) {
        return nullptr;
    }
    return connections_[client.Id()];
}

void ConnectionManager::CreateConnection(const Client& client) {}

ConnectionManager::connect_ptr ConnectionManager::CreatePeerConnection(const std::string& id) {
    auto pc = std::make_shared<rtc::PeerConnection>();
    pc->onLocalDescription([id, this](rtc::Description description) {
        json message = {{"id", id}, {"type", description.typeString()}, {"description", std::string(description)}};

        ws_->send(message.dump());
    });

    pc->onLocalCandidate([id, this](rtc::Candidate candidate) {
        json message = {
            {"id", id}, {"type", "candidate"}, {"candidate", std::string(candidate)}, {"mid", candidate.mid()}};

        ws_->send(message.dump());
    });
    connections_[id] = pc;
    return pc;
}

void ConnectionManager::SetUpWsCallbacks() {
    // callback for recieving messages
    ws_->onMessage([this](auto data) -> void {
        if (!std::holds_alternative<std::string>(data)) {
            return;
        }
        auto message = json::parse(std::get<std::string>(data));

        if (!message.contains("id")) {
            return;
        }
        auto id = message["id"];

        if (!message.contains("type")) {
            return;
        }
        std::string mtype = message["type"];

        connect_ptr pc;
        auto pc_it = connections_.find(id); 
        if (pc_it != connections_.end()) {
            pc = pc_it->second;
        } else if (mtype == "offer") {
            pc = CreatePeerConnection(id);  // peer connection with basic settings
        }

        if (mtype == "offer" || mtype == "answer") {
            auto sdp = message["description"];
            pc->setRemoteDescription(rtc::Description(sdp, mtype));
        } else if (mtype == "candidate") {
            auto sdp = message["candidate"];
            auto mid = message["mid"];
            pc->addRemoteCandidate(rtc::Candidate(sdp, mid));
        }
    });
}