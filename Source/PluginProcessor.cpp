#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }

std::unique_ptr<juce::AudioParameterFloat> percentParam (const char* id, const char* name, float def)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), def,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (percentText));
}

std::unique_ptr<juce::AudioParameterBool> boolParam (const char* id, const char* name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout EZVoiceFXAudioProcessor::createLayout()
{
    using namespace ParamIDs;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (boolParam (lowcutOn, "Low Cut On", true));
    {
        juce::NormalisableRange<float> r (20.0f, 300.0f, 1.0f);
        r.setSkewForCentre (90.0f);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { lowcutFreq, 1 }, "Low Cut Freq", r, 80.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")
                .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " Hz"; })));
    }

    layout.add (boolParam (declickOn, "De-click On", true));
    layout.add (percentParam (declickAmt, "De-click", 40.0f));

    layout.add (boolParam (denoiseOn, "AI Denoise On", true));
    layout.add (percentParam (denoiseAmt, "AI Denoise", 80.0f));

    layout.add (boolParam (deplosiveOn, "De-plosive On", true));
    layout.add (percentParam (deplosiveAmt, "De-plosive", 50.0f));

    layout.add (boolParam (dereverbOn, "De-reverb On", true));
    layout.add (percentParam (dereverbAmt, "De-reverb", 35.0f));

    layout.add (boolParam (breathOn, "Breath On", true));
    layout.add (percentParam (breathAmt, "Breath", 30.0f));

    layout.add (boolParam (deessOn, "De-ess On", true));
    layout.add (percentParam (deessAmt, "De-ess", 50.0f));

    layout.add (boolParam (levelOn, "Auto-Level On", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { levelTarget, 1 }, "Target Loudness",
        juce::NormalisableRange<float> (-30.0f, -10.0f, 0.5f), -18.0f,
        juce::AudioParameterFloatAttributes().withLabel ("LUFS")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " LUFS"; })));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { levelMode, 1 }, "Auto-Level Mode", juce::StringArray { "Ride", "Learn" }, 0));

    layout.add (boolParam (limiterOn, "Limiter On", true));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { limiterCeil, 1 }, "Ceiling",
        juce::NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -1.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; })));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputGain, 1 }, "Output",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; })));

    layout.add (boolParam (bypass, "Bypass", false));
    return layout;
}

//==============================================================================
EZVoiceFXAudioProcessor::EZVoiceFXAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EZVoiceFX", createLayout())
{
    bypassParam = apvts.getParameter (ParamIDs::bypass);
}

bool EZVoiceFXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void EZVoiceFXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;

    for (int c = 0; c < 2; ++c)
    {
        lowcut[c].prepare (fs);
        declick[c].prepare (fs);
        denoise[c].prepare (fs);
        deplosive[c].prepare (fs);
        dereverb[c].prepare (fs);
        deess[c].prepare (fs);
        dryScratch[c].assign ((size_t) std::max (samplesPerBlock, 8192), 0.0f);
    }
    breath.prepare (fs);
    autoLevel.prepare (fs);
    limiter.prepare (fs);
    outMeter.prepare (fs);

    const int latency = declick[0].getLatency() + denoise[0].getLatency()
                      + dereverb[0].getLatency() + limiter.getLatency();
    for (auto& d : bypassDelay) d.prepare (latency);
    setLatencySamples (latency);

    outGainSm.reset (fs, 0.05);
    outGainSm.setCurrentAndTargetValue (ez::dbToGain (param (ParamIDs::outputGain)));
    bypassSm.reset (fs, 0.03);
    bypassSm.setCurrentAndTargetValue (flag (ParamIDs::bypass) ? 1.0f : 0.0f);
    lowcutFreqSm.reset (fs, 0.05);
    lowcutFreqSm.setCurrentAndTargetValue (flag (ParamIDs::lowcutOn) ? param (ParamIDs::lowcutFreq) : 10.0f);

    meters.latencySamples.store (latency);
    meters.sampleRate.store (fs);
}

//==============================================================================
void EZVoiceFXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    using namespace ParamIDs;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, n);

    const int nch = std::min (2, buffer.getNumChannels());
    if (nch <= 0 || n <= 0) return;

    float* ch[2] = { buffer.getWritePointer (0), nch > 1 ? buffer.getWritePointer (1) : nullptr };

    // ---- read parameters ----
    const auto amt = [this] (const char* onId, const char* amtId)
    {
        return flag (onId) ? param (amtId) * 0.01f : 0.0f;
    };
    const float aDeclick   = amt (declickOn, declickAmt);
    const float aDenoise   = amt (denoiseOn, denoiseAmt);
    const float aDeplosive = amt (deplosiveOn, deplosiveAmt);
    const float aDereverb  = amt (dereverbOn, dereverbAmt);
    const float aBreath    = amt (breathOn, breathAmt);
    const float aDeess     = amt (deessOn, deessAmt);

    lowcutFreqSm.setTargetValue (flag (lowcutOn) ? param (lowcutFreq) : 10.0f);
    autoLevel.setParams (flag (levelOn), param (levelTarget), param (levelMode) >= 0.5f);
    outGainSm.setTargetValue (ez::dbToGain (param (outputGain)));
    bypassSm.setTargetValue (flag (bypass) ? 1.0f : 0.0f);

    // ---- input meter + latency-aligned dry copy (for bypass) ----
    float inPeak = 0.0f;
    for (int c = 0; c < nch; ++c)
    {
        if ((int) dryScratch[c].size() < n) dryScratch[c].resize ((size_t) n);   // rare fallback
        for (int i = 0; i < n; ++i)
        {
            inPeak = std::max (inPeak, std::abs (ch[c][i]));
            dryScratch[c][(size_t) i] = bypassDelay[c].process (ch[c][i]);
        }
    }

    // ---- 1. Low cut ----
    {
        const float f = lowcutFreqSm.skip (n);
        for (int c = 0; c < nch; ++c)
        {
            lowcut[c].setFrequency (f);
            for (int i = 0; i < n; ++i) ch[c][i] = lowcut[c].process (ch[c][i]);
        }
    }

    // ---- 2..5. per-channel restoration ----
    for (int c = 0; c < nch; ++c)
    {
        declick[c].setAmount (aDeclick);
        denoise[c].setMix (aDenoise);
        deplosive[c].setAmount (aDeplosive);
        dereverb[c].setAmount (aDereverb);

        float* d = ch[c];
        for (int i = 0; i < n; ++i)
        {
            float x = d[i];
            x = declick[c].processSample (x);
            x = denoise[c].processSample (x);
            x = deplosive[c].processSample (x);
            x = dereverb[c].processSample (x);
            d[i] = x;
        }
    }

    // ---- 6. Breath control (linked) ----
    breath.setAmount (aBreath);
    breath.process (ch, nch, n);

    // ---- 7. De-esser ----
    float deessMin = 1.0f, plosiveMin = 1.0f;
    for (int c = 0; c < nch; ++c)
    {
        deess[c].setAmount (aDeess);
        float* d = ch[c];
        for (int i = 0; i < n; ++i)
        {
            d[i] = deess[c].processSample (d[i]);
            deessMin = std::min (deessMin, deess[c].getGain());
        }
        plosiveMin = std::min (plosiveMin, deplosive[c].getGain());
    }

    // ---- 8. Auto-Level ----
    autoLevel.process (ch, nch, n);

    // ---- output trim (before the limiter so the ceiling always holds) ----
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainSm.getNextValue();
        for (int c = 0; c < nch; ++c) ch[c][i] *= g;
    }

    // ---- 9. Limiter ----
    limiter.process (ch, nch, n, param (limiterCeil), flag (limiterOn));

    // ---- bypass crossfade (dry is delayed by the same latency) ----
    if (bypassSm.isSmoothing() || bypassSm.getTargetValue() > 0.0f)
    {
        for (int i = 0; i < n; ++i)
        {
            const float b = bypassSm.getNextValue();
            for (int c = 0; c < nch; ++c)
                ch[c][i] = ch[c][i] * (1.0f - b) + dryScratch[c][(size_t) i] * b;
        }
    }

    // ---- meters ----
    float outPeak = 0.0f;
    for (int c = 0; c < nch; ++c)
        for (int i = 0; i < n; ++i) outPeak = std::max (outPeak, std::abs (ch[c][i]));
    outMeter.process (ch, nch, n);

    auto decayTo = [] (std::atomic<float>& a, float v, float fall)
    {
        const float prev = a.load();
        a.store (v > prev ? v : std::max (v, prev - fall));
    };
    decayTo (meters.inPeakDb, ez::gainToDb (inPeak), 1.5f);
    decayTo (meters.outPeakDb, ez::gainToDb (outPeak), 1.5f);
    meters.outLufs.store (outMeter.getLufs());
    meters.autoGainDb.store (autoLevel.getGainDb());
    meters.compGrDb.store (autoLevel.getCompGrDb());
    meters.vad.store (denoise[0].getVad());
    decayTo (meters.limiterGrDb, limiter.getGrDb(), 0.5f);
    decayTo (meters.deessGrDb, -ez::gainToDb (deessMin), 0.5f);
    decayTo (meters.plosiveGrDb, -ez::gainToDb (plosiveMin), 0.5f);
    meters.breathGrDb.store (-ez::gainToDb (breath.getGain()));
    meters.clicks.store (declick[0].getClickCount() + (nch > 1 ? declick[1].getClickCount() : 0));

    // copy mono result to any extra output channel (mono-in / stereo-out hosts)
    for (int c = nch; c < buffer.getNumChannels(); ++c)
        buffer.copyFrom (c, 0, buffer, 0, 0, n);
}

//==============================================================================
void EZVoiceFXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("learnedLufs", (double) autoLevel.getLearnedLufs(), nullptr);
    state.setProperty ("learnedValid", autoLevel.hasLearned(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void EZVoiceFXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const float lufs = (float) (double) tree.getProperty ("learnedLufs", -18.0);
            const bool valid = (bool) tree.getProperty ("learnedValid", false);
            apvts.replaceState (tree);
            autoLevel.setLearned (lufs, valid);
        }
    }
}

juce::AudioProcessorEditor* EZVoiceFXAudioProcessor::createEditor()
{
    return new EZVoiceFXAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EZVoiceFXAudioProcessor();
}
