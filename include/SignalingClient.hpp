#pragma once

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <iostream>

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

typedef websocketpp::client<websocketpp::config::asio_client> client;

class SignalingClient {
public:
    using MessageCallback = std::function<void(const nlohmann::json&)>;

    SignalingClient(const std::string& server_url, const std::string& client_id);
    ~SignalingClient();

    bool Connect();
    void Disconnect();
    void SendMessage(const nlohmann::json& message);
    void SetMessageCallback(MessageCallback callback);
    bool IsConnected() const { return _connected; }

private:
    void OnMessage(websocketpp::connection_hdl hdl, client::message_ptr msg);
    void OnOpen(websocketpp::connection_hdl hdl);
    void OnClose(websocketpp::connection_hdl hdl);
    void OnFail(websocketpp::connection_hdl hdl);

    client _endpoint;
    websocketpp::connection_hdl _hdl;
    std::string _server_url;
    std::string _client_id;
    MessageCallback _message_callback;
    std::unique_ptr<std::thread> _thread;
    std::atomic<bool> _connected;
    std::atomic<bool> _running;
};
