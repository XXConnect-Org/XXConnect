#include "client.h"

#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>

namespace cm = connection_management;

std::string generateClientId() {
    static const std::string letters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::string id;
    id.reserve(cm::client_id_size);

    for (size_t i = 0; i < cm::client_id_size; ++i) {
        id += letters[rand() % letters.size()];
    }
    return id;
}

cm::Client::Client() : id_(generateClientId()) {}

std::string cm::Client::Id() const { return id_; }
