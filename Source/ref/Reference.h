// EZ Voice FX - reference track analysis
//
// Loads a reference song, (optionally) separates vocals from the instrumental with the
// MDX-Net network, and measures: vocal-to-instrumental loudness balance, vocal tone
// (10 bands), vocal dynamics (LRA) and sibilance. Runs on a background thread.
#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "OrtEngine.h"

namespace ez
{
constexpr int kMatchBands = 10;
constexpr float kMatchFreqs[kMatchBands] = { 100.0f, 200.0f, 400.0f, 800.0f, 1600.0f, 3150.0f, 5000.0f, 8000.0f, 11000.0f, 15000.0f };

struct ReferenceProfile
{
    bool valid = false, balanceValid = false, separated = false, approximate = false, gpu = false;
    juce::String name;
    float balanceDb = 0.0f;          // vocal loudness minus instrumental loudness (LU)
    float lra = 8.0f, sibilance = 0.1f, seconds = 0.0f;
    float bands[kMatchBands] {};     // vocal tone, dB, normalised to the 400 Hz - 8 kHz average

    juce::String toString() const;
    static ReferenceProfile fromString (const juce::String& s);
};

class ReferenceAnalyzer : public juce::Thread
{
public:
    enum Mode { Auto = 0, Precise = 1, Fast = 2, Acapella = 3 };

    ReferenceAnalyzer();
    ~ReferenceAnalyzer() override;

    // message thread
    void startAnalysis (const juce::File& file, Mode mode, bool allowGpu);
    void cancel();
    bool isBusy() const noexcept { return isThreadRunning(); }
    float getProgress() const noexcept { return progress.load(); }
    juce::String getStatus() const;
    bool takeResult (ReferenceProfile& out);     // true once per finished analysis

    void run() override;

private:
    struct FrameStat { float kv, ki, hi, full; float bands[kMatchBands]; };

    bool loadAudio (juce::AudioBuffer<float>& out, double& seconds);
    bool analyse (const juce::AudioBuffer<float>& audio, Mode mode, std::vector<FrameStat>& frames, bool& usedGpu, Mode& usedMode);
    void finish (const std::vector<FrameStat>& frames, Mode mode, bool usedGpu, double seconds);
    void setStatus (const juce::String& s);

    juce::File file;
    Mode mode = Auto;
    bool allowGpu = true;
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> resultReady { false };
    juce::CriticalSection lock;
    juce::String status;
    ReferenceProfile result;
    std::unique_ptr<OrtEngine> engine;
    bool engineTried = false, engineGpuTried = false;
};
} // namespace ez
