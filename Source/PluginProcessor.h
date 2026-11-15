#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Common.h"
#include "dsp/Denoise.h"
#include "dsp/Declick.h"
#include "dsp/Dynamics.h"
#include "dsp/Dereverb.h"
#include "dsp/Loudness.h"
#include "dsp/Limiter.h"
#include "dsp/Restoration.h"
#include "dsp/Analyzer.h"
#include "SampleFifo.h"

namespace ParamIDs
{
    inline constexpr const char* lowcutOn      = "lowcut_on";
    inline constexpr const char* lowcutFreq    = "lowcut_freq";
    inline constexpr const char* humOn         = "hum_on";
    inline constexpr const char* humAmt        = "hum_amt";
    inline constexpr const char* humFreq       = "hum_freq";
    inline constexpr const char* declickOn     = "declick_on";
    inline constexpr const char* declickAmt    = "declick_amt";
    inline constexpr const char* denoiseOn     = "denoise_on";
    inline constexpr const char* denoiseAmt    = "denoise_amt";
    inline constexpr const char* denoiseEngine = "denoise_engine";
    inline constexpr const char* deplosiveOn   = "deplosive_on";
    inline constexpr const char* deplosiveAmt  = "deplosive_amt";
    inline constexpr const char* dereverbOn    = "dereverb_on";
    inline constexpr const char* dereverbAmt   = "dereverb_amt";
    inline constexpr const char* roomSize      = "room_size";
    inline constexpr const char* gateOn        = "gate_on";
    inline constexpr const char* gateAmt       = "gate_amt";
    inline constexpr const char* breathOn      = "breath_on";
    inline constexpr const char* breathAmt     = "breath_amt";
    inline constexpr const char* deessOn       = "deess_on";
    inline constexpr const char* deessAmt      = "deess_amt";
    inline constexpr const char* deessFreq     = "deess_freq";
    inline constexpr const char* levelOn       = "level_on";
    inline constexpr const char* levelTarget   = "level_target";
    inline constexpr const char* levelMode     = "level_mode";
    inline constexpr const char* learnAuto     = "learn_auto";
    inline constexpr const char* assistant     = "assistant";
    inline constexpr const char* limiterOn     = "limiter_on";
    inline constexpr const char* limiterCeil   = "limiter_ceiling";
    inline constexpr const char* outputGain    = "output_gain";
    inline constexpr const char* mix           = "mix";
    inline constexpr const char* mono          = "mono";
    inline constexpr const char* matchBypass   = "match_bypass";
    inline constexpr const char* listen        = "listen";
    inline constexpr const char* bypass        = "bypass";
}

//==============================================================================
class EZVoiceFXAudioProcessor : public juce::AudioProcessor,
                                private juce::AsyncUpdater
{
public:
    EZVoiceFXAudioProcessor();
    ~EZVoiceFXAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    // ---- Learn / Assistant (editor) ----
    void startLearn() { learnStartReq.store (true); }
    void stopLearn()  { learnStopReq.store (true); }
    bool isLearning() const { return autoLevel.isLearning(); }
    bool hasLearned() const { return autoLevel.hasLearned(); }
    float getLearnedLufs() const { return autoLevel.getLearnedLufs(); }
    juce::String getAssistantReport() const { const juce::ScopedLock sl (reportLock); return assistantReport; }

    // ---- engine info ----
    juce::String getEngineStatus() const { return ez::DeepFilterLib::get().getStatus(); }
    bool isProEngineActive() const { return proActive.load(); }

    // ---- UI settings stored in the session ----
    void setUiProperty (const juce::Identifier& id, const juce::var& v) { apvts.state.setProperty (id, v, nullptr); }
    juce::var getUiProperty (const juce::Identifier& id, const juce::var& def) const { return apvts.state.getProperty (id, def); }

    // ---- meters / visualisation ----
    struct Meters
    {
        std::atomic<float> inPeakDb { -100.0f }, outPeakDb { -100.0f }, outLufs { -70.0f }, inLufs { -70.0f },
                           autoGainDb { 0.0f }, vad { 0.0f }, limiterGrDb { 0.0f }, deessGrDb { 0.0f },
                           plosiveGrDb { 0.0f }, breathGrDb { 0.0f }, gateGrDb { 0.0f }, compGrDb { 0.0f },
                           cpu { 0.0f }, humBase { 50.0f };
        std::atomic<int> clicks { 0 }, latencySamples { 0 };
        std::atomic<double> sampleRate { 48000.0 };

        static constexpr int kHistory = 300;   // 30 s of 100 ms loudness points
        std::atomic<float> history[kHistory];
        std::atomic<int> historyPos { 0 };
        Meters() { for (auto& h : history) h.store (-70.0f); }
    } meters;

    SampleFifo inputTap, outputTap;

private:
    void handleAsyncUpdate() override;
    void setParam (const char* id, float value);

    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  flag  (const char* id) const { return apvts.getRawParameterValue (id)->load() >= 0.5f; }

    ez::LowCut lowcut[2];
    ez::HumRemover hum[2];
    ez::DeclickChannel declick[2];
    ez::DenoiseChannel denoise[2];
    ez::DeplosiveChannel deplosive[2];
    ez::DereverbChannel dereverb[2];
    ez::VoiceGate gate;
    ez::BreathControl breath;
    ez::DeEsserChannel deess[2];
    ez::AutoLevel autoLevel;
    ez::Limiter limiter;
    ez::LoudnessMeter outMeter, inMeter;
    ez::InputAnalyzer analyzer;
    ez::DelayLine dryDelay[2], cleanDelay[2];

    std::vector<float> dryScratch[2], cleanScratch[2], monoScratch;
    juce::SmoothedValue<float> outGainSm, bypassSm, lowcutFreqSm, mixSm, matchSm, deltaSm;

    juce::AudioProcessorParameter* bypassParam = nullptr;
    double fs = 48000.0;
    int historyCounter = 0, historyLen = 4800;

    std::atomic<bool> learnStartReq { false }, learnStopReq { false }, analysisPending { false };
    bool analyzing = false, wasPlaying = false;
    std::atomic<bool> proActive { false };

    juce::CriticalSection reportLock;
    juce::String assistantReport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EZVoiceFXAudioProcessor)
};
