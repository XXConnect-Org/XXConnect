
#include <iostream>

#include "client_connection_manager.h"

namespace cm = connection_management;

int main() {
    rtc::Configuration config;

    std::string stunServer = "stun:23.21.150.121:3478"; // ??
    std::cout << "STUN server is " << stunServer << std::endl;
    config.iceServers.emplace_back(stunServer);
    config.enableIceUdpMux = true;
    cm::Client client = cm::Client();
    std::cout << "Create client with id: " << client.Id() << std::endl;
    cm::ConnectionManager manager(client, config);
    cm::ConnectionConfig
    manager.OpenForConnections()
}