#pragma once

#include <vector>
#include <atomic>
#include <functional>

struct RecordData {
    std::vector<int16_t> audioData;
    std::atomic<bool> isRecording;
    unsigned int sampleRate;

    // Optional streaming callback for each captured buffer
    // (samples, numSamples, sampleRate)
    std::function<void(const int16_t*, size_t, unsigned int)> onBuffer;
};


