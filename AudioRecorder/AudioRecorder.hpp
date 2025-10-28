#pragma once

#include <RtAudio.h>
#include <vector>
#include <string>
#include <atomic>
#include <iostream>
#include <fstream>

class AudioRecorder {
public:
    AudioRecorder();
    ~AudioRecorder();

    bool initialize();
    bool startRecording();
    bool stopRecording();
    bool saveToFile(const std::string& filename = "recorded_audio.wav");
    bool isRecording() const { return m_isRecording; }
    size_t getRecordedSamples() const { return m_audioData.size(); }

private:
    static int audioCallback(void* outputBuffer, void* inputBuffer,
                           unsigned int nFrames, double streamTime,
                           RtAudioStreamStatus status, void* userData);

    bool writeWavFile(const std::string& filename);

    RtAudio m_audio;
    std::vector<int16_t> m_audioData;
    std::atomic<bool> m_isRecording;
    unsigned int m_sampleRate;
    unsigned int m_bufferFrames;
};