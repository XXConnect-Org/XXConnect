#pragma once

#include <vector>
#include <cstdint>
#include <memory>

struct DenoiseState;

struct DenoiseDeleter {
    void operator()(DenoiseState* ptr) const noexcept;
};

class NoiseSuppressor {
public:
    NoiseSuppressor();
    ~NoiseSuppressor();

    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    std::vector<int16_t> ProcessSamples(const int16_t* samples, size_t numSamples, 
                                        unsigned int inputSampleRate, unsigned int outputSampleRate);

private:
    void Int16ToFloat32(const int16_t* input, float* output, size_t count);
    void Float32ToInt16(const float* input, int16_t* output, size_t count);
    std::vector<float> Resample(const float* input, size_t inputSamples, 
                                unsigned int inputRate, unsigned int outputRate);
    void ProcessFrame(float* frame);

    std::unique_ptr<DenoiseState, DenoiseDeleter> _denoiseState;
    bool _enabled;
    std::vector<float> _inputBuffer;
    unsigned int _currentInputRate;
    unsigned int _currentOutputRate;
};


