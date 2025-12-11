#pragma once

#include "rtc/rtc.hpp"
#include "SavingWorkers/ISavingWorker.hpp"

#include <memory>

namespace test_utils {

struct PeerPair {
    std::shared_ptr<rtc::PeerConnection> caller;
    std::shared_ptr<rtc::PeerConnection> callee;
};

// Создает пару PeerConnection для тестирования без внешнего сигнального сервера
inline PeerPair CreateLoopbackPair() {
    rtc::Configuration config;
    config.disableAutoNegotiation = true;

    auto pcA = std::make_shared<rtc::PeerConnection>(config);
    auto pcB = std::make_shared<rtc::PeerConnection>(config);

    pcA->onLocalDescription([pcB](const rtc::Description& desc) {
        pcB->setRemoteDescription(desc);
        if (desc.type() == rtc::Description::Type::Offer) {
            pcB->setLocalDescription(rtc::Description::Type::Answer);
        }
    });
    pcB->onLocalDescription([pcA](const rtc::Description& desc) {
        pcA->setRemoteDescription(desc);
    });

    pcA->onLocalCandidate([pcB](const rtc::Candidate& c) { pcB->addRemoteCandidate(c); });
    pcB->onLocalCandidate([pcA](const rtc::Candidate& c) { pcA->addRemoteCandidate(c); });

    return {pcA, pcB};
}

class NullSavingWorker : public ISavingWorker {
public:
    bool Save() override { return true; }
};

} // namespace test_utils

