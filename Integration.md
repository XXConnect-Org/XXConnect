# Интеграция с сигнальным сервером

Пример интеграции с менеджером соединений:

```
// Его генерила GPT-шка, возможно тут бредик

void CreateConnection(const Client& client) {
    auto pc = CreatePeerConnection(client.id);
    
    // Сам шумодав
    static NoiseSuppressor suppressor;
    suppressor.SetEnabled(true);
    
    unsigned int sampleRate = 48000; // почему-то этот самый предпочтительный
    auto audioSender = std::make_unique<AudioSender>(pc, suppressor, sampleRate);
    audioSender->AttachTrack(); // ВАЖНО: до setLocalDescription()
    
    audio_senders_[client.id] = std::move(audioSender);
    
    // Настройка callback для отправки аудио
    if (!recorder_) {
        auto worker = std::make_shared<WavWorker>("dummy.wav"); // или nullptr или потом заглушку приделаю
        recorder_ = std::make_unique<AudioRecorder>(worker);
        
        recorder_->SetOnBufferCallback([this](
            const int16_t* samples,
            size_t numSamples,
            unsigned int rate
        ) {
            // Отправляем аудио во все активные соединения
            for (auto& [id, sender] : audio_senders_) {
                if (sender && connections_[id]) {
                    sender->OnAudioBuffer(samples, numSamples);
                }
            }
        });
    }
    
    // Ваши WebRTC callbacks
    pc->onLocalDescription([this, id = client.id](rtc::Description desc) {
        // Отправить SDP через ws_
        SendToSignalingServer(id, "offer", std::string(desc));
    });
    
    pc->onLocalCandidate([this, id = client.id](rtc::Candidate candidate) {
        // Отправить ICE через ws_
        SendToSignalingServer(id, "candidate", std::string(candidate));
    });
    
    // Создать offer после прикрепления трека
    pc->setLocalDescription();
}
    
void OnSignalingMessage(const std::string& from_id, const std::string& type, const std::string& data) {
    if (!HasConnectionWith(from_id)) return;
    
    auto pc = GetConnectionWith(from_id);
    
    if (type == "answer") {
        pc->setRemoteDescription(rtc::Description(data));
    } else if (type == "candidate") {
        pc->addRemoteCandidate(rtc::Candidate(data));
    }
}


// эти поля она хочет добавить в класс менеджера
private:
    std::unique_ptr<AudioRecorder> recorder_;
    std::unordered_map<std::string, std::unique_ptr<AudioSender>> audio_senders_;
    std::thread record_thread_;
    std::atomic<bool> is_recording_{true};
```
