# Интерфейс компонентов

## AudioRecorder

Запись аудио с микрофона.

```cpp
AudioRecorder(std::shared_ptr<ISavingWorker> saving_worker)

// Непрерывная запись
bool StartRecording()
void StopRecording()
bool IsRecording() const

// Callback для обработки каждого буфера
void SetOnBufferCallback(std::function<void(const int16_t*, size_t, unsigned int)> cb)

unsigned int GetSampleRate() const
```

**Важно:** Callback вызывается из потока аудио захвата - операции должны быть быстрыми.

## NoiseSuppressor

Подавление шума на основе RNNoise.

```cpp
NoiseSuppressor()

void SetEnabled(bool enabled)
bool IsEnabled() const

std::vector<int16_t> ProcessSamples(
    const int16_t* samples, 
    size_t numSamples, 
    unsigned int inputSampleRate, 
    unsigned int outputSampleRate
)
```

**Особенности:**
- Требует входные данные 48 kHz (автоматический ресемплинг)
- Обрабатывает фреймами по 480 сэмплов (10 мс)
- Может вернуть пустой вектор при буферизации

## AudioSender

Отправка аудио через WebRTC.

```cpp
AudioSender(PeerConnectionPtr pc, NoiseSuppressor& suppressor, unsigned int sampleRate)

// КРИТИЧЕСКИ ВАЖНО: вызывать до setLocalDescription()
void AttachTrack()

void OnAudioBuffer(const int16_t* samples, size_t numSamples)
bool IsTrackOpen() const
```

## AudioReceiver

Прием аудио из WebRTC.

```cpp
AudioReceiver(PeerConnectionPtr pc, unsigned int sampleRate)

// Вызывать после установки соединения
void SetupDataChannel(const std::string& channelLabel = "audio")
void SetupTrack(std::shared_ptr<rtc::Track> track)

void SetOnAudioDataCallback(OnAudioDataCallback callback)

// Jitter buffer для компенсации сетевых задержек
void SetJitterBufferEnabled(bool enabled)
void SetJitterBufferSize(size_t minPackets, size_t maxPackets)
```

## Порядок операций

1. **Отправка:** Создать `AudioSender` → `AttachTrack()` → `setLocalDescription()` → `OnAudioBuffer()`
2. **Прием:** Создать `AudioReceiver` → после `onTrack` вызвать `SetupTrack()` → данные приходят в callback

## Частота дискретизации

Рекомендуется 48000 Hz (требование RNNoise). `AudioRecorder` автоматически определяет поддерживаемую частоту устройства.
