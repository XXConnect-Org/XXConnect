#include <iostream>
#include <memory>
#include <rtc/rtc.hpp>
#include <rtc/websocket.hpp>
#include <string>
#include <unordered_map>
#include <variant>

#include "client.h"

namespace connection_management {

struct ConnectionConfig {
    std::string server;
    std::string port;
};

class ConnectionManager {
   public:
    using connect_ptr = std::shared_ptr<rtc::PeerConnection>;
    ConnectionManager(const Client& client, rtc::Configuration config);

    // methods for managing ws_ connection to signalling server
    bool OpenForConnections(const ConnectionConfig& con_config);
    void CloseForConnections();

    // methods for managing peer connections between client_ and other clients
    void CreateConnection(const Client& client);
    bool HasConnectionWith(const Client& client);
    connect_ptr GetConnectionWith(const Client& client);

   private:
    void SetUpWsCallbacks();
    connect_ptr CreatePeerConnection(const std::string& id);

    Client client_;
    rtc::Configuration config_;
    std::shared_ptr<rtc::WebSocket> ws_;
    std::unordered_map<std::string, std::shared_ptr<rtc::PeerConnection>> connections_;
};

}  // namespace connection_management