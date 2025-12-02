
### AudioRecorder
- `AudioRecorder(std::shared_ptr<ISavingWorker> worker)` — worker может быть nullptr
- `void SetOnBufferCallback(callback)` — вызывается для каждого аудио буфера
- `void Record(unsigned int seconds)` — начинает запись на N секунд

### NoiseSuppressor
- `NoiseSuppressor()`
- `void SetEnabled(bool)` — включить/выключить подавление шума

### AudioSender
- `AudioSender(PeerConnectionPtr, NoiseSuppressor&, sampleRate)` — обычно 48000
- `void AttachTrack()` — **вызвать до `setLocalDescription()`**
- `void OnAudioBuffer(const int16_t*, size_t)` — отправить буфер

## Важные моменты

1. `AudioSender::AttachTrack()` вызывается **до** `pc->setLocalDescription()`
2. Используйте частоту дискретизации 48000 Hz (требование RNNoise)
3. `SetOnBufferCallback` вызывается из потока аудио захвата
4. На macOS потребуется разрешение на доступ к микрофону