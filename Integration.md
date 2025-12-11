# Интеграция с ConnectionManager (много буков от ЧатГупэТэ)

## Что добавить в ConnectionManager

```cpp
    std::unique_ptr<AudioRecorder> recorder_;
    std::unique_ptr<NoiseSuppressor> suppressor_;
    std::unordered_map<std::string, std::unique_ptr<AudioSender>> audio_senders_;
    std::unordered_map<std::string, std::unique_ptr<AudioReceiver>> audio_receivers_;
    std::mutex audio_senders_mutex_;
    std::atomic<bool> is_recording_{false};
```

## Инициализация в конструкторе

```cpp
ConnectionManager::ConnectionManager(const Client& client, rtc::Configuration config)
    : client_(client), config_(std::move(config))
{
    suppressor_ = std::make_unique<NoiseSuppressor>();
    suppressor_->SetEnabled(true);
    
    auto nullWorker = std::make_shared<NullSavingWorker>();
    recorder_ = std::make_unique<AudioRecorder>(nullWorker);
    
    recorder_->SetOnBufferCallback([this](
        const int16_t* samples, size_t numSamples, unsigned int sampleRate
    ) {
        std::lock_guard<std::mutex> lock(audio_senders_mutex_);
        for (auto& [clientId, sender] : audio_senders_) {
            if (sender && sender->IsTrackOpen()) {
                sender->OnAudioBuffer(samples, numSamples);
            }
        }
    });
}
```

## Создание соединения

```cpp
void ConnectionManager::CreateConnection(const Client& client) {
    auto pc = CreatePeerConnection(client.id);
    if (!pc) return;
    
    unsigned int sampleRate = recorder_->GetSampleRate();
    
    // Отправка
    auto sender = std::make_unique<AudioSender>(pc, *suppressor_, sampleRate);
    sender->AttachTrack(); // ВАЖНО: до setLocalDescription()
    {
        std::lock_guard<std::mutex> lock(audio_senders_mutex_);
        audio_senders_[client.id] = std::move(sender);
    }
    
    // Прием
    auto receiver = std::make_unique<AudioReceiver>(pc, sampleRate);
    receiver->SetJitterBufferEnabled(true);
    receiver->SetOnAudioDataCallback([this, id = client.id](
        const int16_t* samples, size_t numSamples, unsigned int rate
    ) {
        OnAudioReceived(id, samples, numSamples, rate);
    });
    audio_receivers_[client.id] = std::move(receiver);
    
    // WebRTC callbacks
    pc->onLocalDescription([this, id = client.id](const rtc::Description& desc) {
        SendToSignalingServer(id, "offer", std::string(desc));
    });
    
    pc->onLocalCandidate([this, id = client.id](const rtc::Candidate& candidate) {
        SendToSignalingServer(id, "candidate", std::string(candidate));
    });
    
    pc->onTrack([this, id = client.id](std::shared_ptr<rtc::Track> track) {
        if (auto it = audio_receivers_.find(id); it != audio_receivers_.end()) {
            it->second->SetupTrack(track);
        }
    });
    
    pc->setLocalDescription(rtc::Description::Type::Offer);
}
```

## Обработка сигнальных сообщений

```cpp
void ConnectionManager::OnSignalingMessage(
    const std::string& from_id, const std::string& type, const std::string& data
) {
    if (!HasConnectionWith(from_id)) return;
    
    auto pc = GetConnectionWith(from_id);
    if (type == "answer") {
        pc->setRemoteDescription(rtc::Description(data));
    } else if (type == "candidate") {
        pc->addRemoteCandidate(rtc::Candidate(data));
    }
}
```

## Управление записью

```cpp
void StartAudioRecording() {
    if (!is_recording_.exchange(true)) {
        recorder_->StartRecording();
    }
}

void StopAudioRecording() {
    if (is_recording_.exchange(false)) {
        recorder_->StopRecording();
    }
}
```

## Очистка

```cpp
void CloseConnection(const std::string& clientId) {
    {
        std::lock_guard<std::mutex> lock(audio_senders_mutex_);
        audio_senders_.erase(clientId);
    }
    audio_receivers_.erase(clientId);
    connections_.erase(clientId);
}
```

## Важно

- `AttachTrack()` вызывается **до** `setLocalDescription()`
- Используйте синхронизацию при доступе к `audio_senders_` из callback
- Один `AudioRecorder` и `NoiseSuppressor` для всех соединений
- Отдельный `AudioSender` и `AudioReceiver` для каждого соединения
