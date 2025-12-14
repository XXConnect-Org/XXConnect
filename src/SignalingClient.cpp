#include "../include/SignalingClient.hpp"

SignalingClient::SignalingClient(const std::string& server_url, const std::string& client_id)
    : _server_url(server_url), _client_id(client_id), _connected(false), _running(false) {

    _endpoint.clear_access_channels(websocketpp::log::alevel::all);
    _endpoint.clear_error_channels(websocketpp::log::elevel::all);

    _endpoint.init_asio();
    _endpoint.start_perpetual();

    _endpoint.set_open_handler(bind(&SignalingClient::OnOpen, this, ::_1));
    _endpoint.set_close_handler(bind(&SignalingClient::OnClose, this, ::_1));
    _endpoint.set_fail_handler(bind(&SignalingClient::OnFail, this, ::_1));
    _endpoint.set_message_handler(bind(&SignalingClient::OnMessage, this, ::_1, ::_2));
}

SignalingClient::~SignalingClient() {
    Disconnect();
    _endpoint.stop_perpetual();
    if (_thread && _thread->joinable()) {
        _thread->join();
    }
}

bool SignalingClient::Connect() {
    if (_running) return false;

    try {
        websocketpp::lib::error_code ec;
        std::string full_url = _server_url + "/" + _client_id;
        client::connection_ptr con = _endpoint.get_connection(full_url, ec);

        if (ec) {
            std::cerr << "Could not create connection: " << ec.message() << std::endl;
            return false;
        }

        _endpoint.connect(con);

        _thread.reset(new std::thread([this]() {
            _running = true;
            try {
                _endpoint.run();
            } catch (const std::exception& e) {
                std::cerr << "WebSocket run error: " << e.what() << std::endl;
            }
            _running = false;
        }));

        // Ждем немного для установления соединения
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return _connected;
    } catch (const std::exception& e) {
        std::cerr << "Connection error: " << e.what() << std::endl;
        return false;
    }
}

void SignalingClient::Disconnect() {
    if (_connected) {
        websocketpp::lib::error_code ec;
        _endpoint.close(_hdl, websocketpp::close::status::normal, "", ec);
        if (ec) {
            std::cerr << "Error closing connection: " << ec.message() << std::endl;
        }
        _connected = false;
    }
}

void SignalingClient::SendMessage(const nlohmann::json& message) {
    if (!_connected) {
        std::cerr << "Not connected to signaling server" << std::endl;
        return;
    }

    try {
        websocketpp::lib::error_code ec;
        _endpoint.send(_hdl, message.dump(), websocketpp::frame::opcode::text, ec);
        if (ec) {
            std::cerr << "Error sending message: " << ec.message() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception sending message: " << e.what() << std::endl;
    }
}

void SignalingClient::SetMessageCallback(MessageCallback callback) {
    _message_callback = std::move(callback);
}

void SignalingClient::OnOpen(websocketpp::connection_hdl hdl) {
    _hdl = hdl;
    _connected = true;
    std::cout << "Connected to signaling server as " << _client_id << std::endl;
}

void SignalingClient::OnClose(websocketpp::connection_hdl hdl) {
    _connected = false;
    std::cout << "Disconnected from signaling server" << std::endl;
}

void SignalingClient::OnFail(websocketpp::connection_hdl hdl) {
    _connected = false;
    std::cerr << "Connection failed" << std::endl;
}

void SignalingClient::OnMessage(websocketpp::connection_hdl hdl, client::message_ptr msg) {
    try {
        auto json_msg = nlohmann::json::parse(msg->get_payload());
        if (_message_callback) {
            _message_callback(json_msg);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing message: " << e.what() << std::endl;
    }
}
