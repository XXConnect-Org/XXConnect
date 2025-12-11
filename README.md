# Backend сервиса для звонков с шумоподавлением

## Компоненты

- **AudioRecorder** - запись аудио с микрофона
- **NoiseSuppressor** - подавление шума (rnnoise)
- **AudioSender** - отправка аудио через peer-connection
- **AudioReceiver** - прием аудио из peer-connection

## Сборка

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Тестирование

Тестирование через фреймворк Google Tests

### Запуск через ctest

```bash
# Все тесты
ctest --test-dir build --output-on-failure

# Конкретный тест
ctest --test-dir build -R PeerConnectionTest -V
```

## Использование

### Запись аудио

```cpp
auto worker = std::make_shared<WavWorker>("output.wav");
AudioRecorder recorder(worker);
recorder.Record(5000); // 5 секунд
recorder.SaveData();
```

### Шумоподавление

```cpp
NoiseSuppressor suppressor;
suppressor.SetEnabled(true);
auto denoised = suppressor.ProcessSamples(samples, numSamples, sampleRate, sampleRate);
```

### Отправка через WebRTC

```cpp
AudioSender sender(pc, suppressor, sampleRate);
sender.AttachTrack();
recorder.SetOnBufferCallback([&sender](const int16_t* samples, size_t num, unsigned int rate) {
    sender.OnAudioBuffer(samples, num);
});
```

### Прием через WebRTC

```cpp
AudioReceiver receiver(pc, sampleRate);
receiver.SetupDataChannel("audio");
receiver.SetOnAudioDataCallback([](const int16_t* samples, size_t num, unsigned int rate) {
    // Обработка полученных данных
});
```

## Логирование

Логирование работает только в Debug режиме. В Release режиме все DEBUG_LOG макросы отключены.

Для включения логирования соберите проект в Debug режиме:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```
