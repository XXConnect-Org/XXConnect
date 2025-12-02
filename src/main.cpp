#include "AudioRecorder/RecordData.hpp"
#include "SavingWorkers/WavWorker.hpp"
#include "AudioRecorder/AudioRecorder.hpp"
#include "AudioRecorder/NoiseSuppressor.hpp"

#include <iostream>
#include <vector>

int main()
{
    // 1) Record once using the raw recorder (no suppression in the callback)
    std::cout << "Recording raw audio (shared for both tests)..." << std::endl;
    auto rawWorker = std::make_shared<WavWorker>("rec_raw.wav");
    AudioRecorder recorder(rawWorker);
    recorder.Record(5000);
    recorder.SaveData(); 

    // 2) Access the same recorded buffer and run noise suppression offline
    const auto& rawData = recorder.GetAudioData();
    const auto sampleRate = recorder.GetSampleRate();

    NoiseSuppressor suppressor;
    suppressor.SetEnabled(true);

    std::cout << "Running noise suppression over recorded buffer..." << std::endl;
    std::vector<int16_t> denoised = suppressor.ProcessSamples(
        rawData.data(),
        rawData.size(),
        sampleRate,
        sampleRate
    );

    // 3) Save the denoised version using a separate worker
    auto denoisedWorker = std::make_shared<WavWorker>("rec_denoised.wav");
    denoisedWorker->SetSampleRate(sampleRate);
    denoisedWorker->SetAudioData(denoised);
    denoisedWorker->Save();

    std::cout << "Done. Compare rec_raw.wav and rec_denoised.wav (same recording, processed vs. unprocessed)." << std::endl;
    return 0;
}