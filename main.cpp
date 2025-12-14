#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include "WebRTCCall.hpp"

class VoiceCallApplication {
public:
    VoiceCallApplication(const std::string& local_id, const std::string& remote_id,
                        const std::string& signaling_server)
        : _local_id(local_id), _remote_id(remote_id), _signaling_server(signaling_server),
          _running(true) {
    }

    bool Run() {
        _call = std::make_unique<WebRTCCall>(_local_id, _remote_id, _signaling_server);

        _call->SetStateCallback([this](WebRTCCall::State state) {
            OnStateChanged(state);
        });

        if (!_call->Initialize()) {
            std::cerr << "Failed to initialize call" << std::endl;
            return false;
        }

        PrintHelp();

        std::string command;
        while (_running && std::cin >> command) {
            if (!ProcessCommand(command)) {
                break;
            }
        }

        _call->HangUp();
        return true;
    }

private:
    void OnStateChanged(WebRTCCall::State state) {
        std::string stateStr;
        switch (state) {
            case WebRTCCall::State::Disconnected: stateStr = "Disconnected"; break;
            case WebRTCCall::State::Connecting: stateStr = "Connecting"; break;
            case WebRTCCall::State::Connected: stateStr = "Connected"; break;
            case WebRTCCall::State::Calling: stateStr = "Calling"; break;
            case WebRTCCall::State::InCall: stateStr = "In Call"; break;
        }
        std::cout << "[STATE] " << stateStr << std::endl;
    }

    bool ProcessCommand(const std::string& command) {
        if (command == "call") {
            _call->StartCall();
        }
        else if (command == "hangup") {
            _call->HangUp();
        }
        else if (command == "quit" || command == "exit") {
            _running = false;
            return false;
        }
        else if (command == "help") {
            PrintHelp();
        }
        else {
            std::cout << "Unknown command: " << command << std::endl;
            PrintHelp();
        }
        return true;
    }

    void PrintHelp() {
        std::cout << "\n=== Voice Call Application ===" << std::endl;
        std::cout << "Commands:" << std::endl;
        std::cout << "  call    - Start a call" << std::endl;
        std::cout << "  hangup  - End current call" << std::endl;
        std::cout << "  help    - Show this help" << std::endl;
        std::cout << "  quit    - Exit application" << std::endl;
        std::cout << "===============================\n" << std::endl;
    }

    std::string _local_id;
    std::string _remote_id;
    std::string _signaling_server;
    std::unique_ptr<WebRTCCall> _call;
    std::atomic<bool> _running;
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <local_id> <remote_id> <signaling_server_url>" << std::endl;
        std::cout << "Example: " << argv[0] << " user1 user2 ws://localhost:8000" << std::endl;
        return 1;
    }

    std::string local_id = argv[1];
    std::string remote_id = argv[2];
    std::string signaling_server = argv[3];

    VoiceCallApplication app(local_id, remote_id, signaling_server);
    return app.Run() ? 0 : 1;
}
