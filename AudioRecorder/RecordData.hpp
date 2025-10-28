#pragma once

struct RecordData {
    std::vector<int16_t> audioData;
    std::atomic<bool> isRecording;
    unsigned int sampleRate;
};
