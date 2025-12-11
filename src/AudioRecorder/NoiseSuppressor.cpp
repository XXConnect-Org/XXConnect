#include "NoiseSuppressor.hpp"
#include "rnnoise.h"
#include <algorithm>
#include <cmath>
#include <cstring>

void DenoiseDeleter::operator()(DenoiseState* ptr) const noexcept {
    if (ptr) {
        rnnoise_destroy(ptr);
    }
}

NoiseSuppressor::NoiseSuppressor()
    : _denoiseState(rnnoise_create(nullptr))
    , _enabled(false)
    , _currentInputRate(44100)
    , _currentOutputRate(44100) {
    _inputBuffer.reserve(480 * 4);
}

NoiseSuppressor::~NoiseSuppressor() = default;

void NoiseSuppressor::SetEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled) {
        _inputBuffer.clear();
    }
}

bool NoiseSuppressor::IsEnabled() const {
    return _enabled;
}

void NoiseSuppressor::Int16ToFloat32(const int16_t* input, float* output, size_t count) {
    const float scale = 1.0f / 32768.0f;
    for (size_t i = 0; i < count; ++i) {
        output[i] = static_cast<float>(input[i]) * scale;
    }
}

void NoiseSuppressor::Float32ToInt16(const float* input, int16_t* output, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        float sample = std::max(-1.0f, std::min(1.0f, input[i]));
        output[i] = static_cast<int16_t>(sample * 32767.0f);
    }
}

std::vector<float> NoiseSuppressor::Resample(const float* input, size_t inputSamples, 
                                              unsigned int inputRate, unsigned int outputRate) {
    if (inputRate == outputRate) {
        return std::vector<float>(input, input + inputSamples);
    }

    const double ratio = static_cast<double>(outputRate) / static_cast<double>(inputRate);
    const size_t outputSamples = static_cast<size_t>(std::round(inputSamples * ratio));
    std::vector<float> output(outputSamples);

    for (size_t i = 0; i < outputSamples; ++i) {
        const double srcIndex = i / ratio;
        const size_t srcIndex0 = static_cast<size_t>(srcIndex);
        const size_t srcIndex1 = std::min(srcIndex0 + 1, inputSamples - 1);
        const double t = srcIndex - srcIndex0;

        if (srcIndex0 < inputSamples) {
            output[i] = input[srcIndex0] * (1.0 - t) + input[srcIndex1] * t;
        } else {
            output[i] = input[inputSamples - 1];
        }
    }

    return output;
}

void NoiseSuppressor::ProcessFrame(float* frame) {
    if (!_denoiseState || !_enabled) {
        return;
    }
    rnnoise_process_frame(_denoiseState.get(), frame, frame);
}

std::vector<int16_t> NoiseSuppressor::ProcessSamples(const int16_t* samples, size_t numSamples,
                                                     unsigned int inputSampleRate, 
                                                     unsigned int outputSampleRate) {
    if (numSamples == 0) {
        return std::vector<int16_t>();
    }

    if (!_enabled) {
        if (inputSampleRate != outputSampleRate) {
            std::vector<float> floatInput(numSamples);
            Int16ToFloat32(samples, floatInput.data(), numSamples);
            auto resampled = Resample(floatInput.data(), numSamples, inputSampleRate, outputSampleRate);
            std::vector<int16_t> result(resampled.size());
            Float32ToInt16(resampled.data(), result.data(), resampled.size());
            return result;
        }
        return std::vector<int16_t>(samples, samples + numSamples);
    }

    _currentInputRate = inputSampleRate;
    _currentOutputRate = outputSampleRate;

    std::vector<float> floatInput(numSamples);
    Int16ToFloat32(samples, floatInput.data(), numSamples);

    std::vector<float> resampled48k;
    if (inputSampleRate != 48000) {
        resampled48k = Resample(floatInput.data(), numSamples, inputSampleRate, 48000);
    } else {
        resampled48k = std::move(floatInput);
    }

    _inputBuffer.insert(_inputBuffer.end(), resampled48k.begin(), resampled48k.end());

    // rnnoise требует ровно 480 сэмплов (10 мс при 48kHz)
    const size_t frameSize = 480;
    std::vector<float> processedFrames;
    
    while (_inputBuffer.size() >= frameSize) {
        float frame[frameSize];
        std::memcpy(frame, _inputBuffer.data(), frameSize * sizeof(float));
        _inputBuffer.erase(_inputBuffer.begin(), _inputBuffer.begin() + frameSize);
        ProcessFrame(frame);
        processedFrames.insert(processedFrames.end(), frame, frame + frameSize);
    }

    // Если данных недостаточно, возвращаем пустой результат (данные останутся в буфере)
    if (processedFrames.empty()) {
        return std::vector<int16_t>();
    }

    std::vector<float> outputFloat;
    if (outputSampleRate != 48000) {
        outputFloat = Resample(processedFrames.data(), processedFrames.size(), 48000, outputSampleRate);
    } else {
        outputFloat = std::move(processedFrames);
    }

    std::vector<int16_t> result(outputFloat.size());
    Float32ToInt16(outputFloat.data(), result.data(), outputFloat.size());

    return result;
}


