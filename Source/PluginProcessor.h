#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Common.h"
#include "dsp/Denoise.h"
#include "dsp/Declick.h"
#include "dsp/Dynamics.h"
#include "dsp/Dereverb.h"
#include "dsp/Loudness.h"
#include "dsp/Limiter.h"

namespace ParamIDs
{
    inline constexpr const char* lowcutOn      = "lowcut_on";
    inline constexpr const char* lowcutFreq    = "lowcut_freq";
    inline constexpr const char* declickOn     = "declick_on";
    inline constexpr const char* declickAmt    = "declick_amt";
    inline constexpr const char* denoiseOn     = "denoise_on";
    inline constexpr const char* denoiseAmt    = "denoise_amt";
    inline constexpr const char* deplosiveOn   = "deplosive_on";
    inline constexpr const char* deplosiveAmt  = "deplosive_amt";
    inline constexpr const char* dereverbOn    = "dereverb_on";
    inline constexpr const char* dereverbAmt   = "dereverb_amt";
    inline constexpr const char* breathOn      = "breath_on";
    inline constexpr const char* breathAmt     = "breath_amt";
    inline constexpr const char* deessOn       = "deess_on";
    inline constexpr const char* deessAmt      = "deess_amt";
    inline constexpr const char* levelOn       = "level_on";
    inline constexpr const char* levelTarget   = "level_target";
    inline constexpr const char* levelMode     = "level_mode";
    inline constexpr const char* limiterOn     = "limiter_on";
    inline constexpr const char* limiterCeil   = "limiter_ceiling";
    inline constexpr const char* outputGain    = "output_gain";
    inline constexpr const char* bypass        = "bypass";
}

//==============================================================================
class EZVoiceFXAudioProcessor : public juce::AudioProcessor
{
public:
    EZVoiceFXAudioProcessor();
    ~EZVoiceFXAudioProcessor() override = default;

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
    juce::AudioProcessorValueTreeState apvts;

    // ---- Learn (called from the editor) ----
    void startLearn() { autoLevel.requestLearnStart(); }
    void stopLearn()  { autoLevel.requestLearnStop(); }
    bool isLearning() const { return autoLevel.isLearning(); }
    bool hasLearned() const { return autoLevel.hasLearned(); }
    float getLearnedLufs() const { return autoLevel.getLearnedLufs(); }
    void resetLearned() { autoLevel.setLearned (-18.0f, false); }

    // ---- meters (read by the editor) ----
    struct Meters
    {
        std::atomic<float> inPeakDb { -100.0f }, outPeakDb { -100.0f }, outLufs { -70.0f },
                           autoGainDb { 0.0f }, vad { 0.0f }, limiterGrDb { 0.0f },
                           deessGrDb { 0.0f }, plosiveGrDb { 0.0f }, breathGrDb { 0.0f }, compGrDb { 0.0f };
        std::atomic<int> clicks { 0 };
        std::atomic<int> latencySamples { 0 };
        std::atomic<double> sampleRate { 48000.0 };
    } meters;

private:
    float param (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  flag  (const char* id) const { return apvts.getRawParameterValue (id)->load() >= 0.5f; }

    ez::LowCut lowcut[2];
    ez::DeclickChannel declick[2];
    ez::DenoiseChannel denoise[2];
    ez::DeplosiveChannel deplosive[2];
    ez::DereverbChannel dereverb[2];
    ez::BreathControl breath;
    ez::DeEsserChannel deess[2];
    ez::AutoLevel autoLevel;
    ez::Limiter limiter;
    ez::LoudnessMeter outMeter;
    ez::DelayLine bypassDelay[2];

    std::vector<float> dryScratch[2];
    juce::SmoothedValue<float> outGainSm, bypassSm, lowcutFreqSm;

    juce::AudioProcessorParameter* bypassParam = nullptr;
    double fs = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EZVoiceFXAudioProcessor)
};
