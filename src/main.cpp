#include "AudioRecorder/RecordData.hpp"
#include "SavingWorkers/WavWorker.hpp"
#include "AudioRecorder/AudioRecorder.hpp"
#include "AudioRecorder/NoiseSuppressor.hpp"
#include "common/debug_log.hpp"

#include <vector>

int main()
{
    auto rawWorker = std::make_shared<WavWorker>("rec_raw.wav");
    AudioRecorder recorder(rawWorker);
    recorder.Record(5000);
    recorder.SaveData(); 

    const auto& rawData = recorder.GetAudioData();
    const auto sampleRate = recorder.GetSampleRate();

    NoiseSuppressor suppressor;
    suppressor.SetEnabled(true);

    DEBUG_LOG("Running noise suppression over recorded buffer..." << DEBUG_LOG_ENDL);
    std::vector<int16_t> denoised = suppressor.ProcessSamples(
        rawData.data(),
        rawData.size(),
        sampleRate,
        sampleRate
    );

    auto denoisedWorker = std::make_shared<WavWorker>("rec_denoised.wav");
    denoisedWorker->SetSampleRate(sampleRate);
    denoisedWorker->SetAudioData(denoised);
    denoisedWorker->Save();

    DEBUG_LOG("Done. Compare rec_raw.wav and rec_denoised.wav" << DEBUG_LOG_ENDL);
    return 0;
}