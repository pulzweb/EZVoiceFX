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

std::unique_ptr<juce::AudioParameterFloat> unitParam (const char* id, const char* name, float lo, float hi,
                                                      float step, float def, float centre,
                                                      std::function<juce::String (float, int)> fmt)
{
    juce::NormalisableRange<float> r (lo, hi, step);
    if (centre > lo && centre < hi) r.setSkewForCentre (centre);
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, r, def,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (fmt)));
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout EZVoiceFXAudioProcessor::createLayout()
{
    using namespace ParamIDs;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto hz  = [] (float v, int) { return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz"; };
    const auto db1 = [] (float v, int) { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; };

    layout.add (boolParam (lowcutOn, "Low Cut On", true));
    layout.add (unitParam (lowcutFreq, "Low Cut Freq", 20.0f, 300.0f, 1.0f, 80.0f, 90.0f, hz));

    layout.add (boolParam (humOn, "Hum Removal On", false));
    layout.add (percentParam (humAmt, "Hum Removal", 70.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { humFreq, 1 }, "Hum Frequency",
                                                              juce::StringArray { "Auto", "50 Hz", "60 Hz" }, 0));

    layout.add (boolParam (declickOn, "De-click On", true));
    layout.add (percentParam (declickAmt, "De-click", 40.0f));

    layout.add (boolParam (denoiseOn, "AI Denoise On", true));
    layout.add (percentParam (denoiseAmt, "AI Denoise", 75.0f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { denoiseEngine, 1 }, "AI Engine",
                                                              juce::StringArray { "AI Pro (DeepFilterNet3)", "AI Lite (RNNoise)" }, 0));

    layout.add (boolParam (deplosiveOn, "De-plosive On", true));
    layout.add (percentParam (deplosiveAmt, "De-plosive", 50.0f));

    layout.add (boolParam (dereverbOn, "De-reverb On", true));
    layout.add (percentParam (dereverbAmt, "De-reverb", 30.0f));
    layout.add (unitParam (roomSize, "Room Size", 0.2f, 1.5f, 0.01f, 0.6f, 0.6f,
                           [] (float v, int) { return juce::String (v, 2) + " s"; }));

    layout.add (boolParam (gateOn, "Gate On", true));
    layout.add (percentParam (gateAmt, "Gate", 40.0f));

    layout.add (boolParam (breathOn, "Breath On", true));
    layout.add (percentParam (breathAmt, "Breath", 30.0f));

    layout.add (boolParam (deessOn, "De-ess On", true));
    layout.add (percentParam (deessAmt, "De-ess", 50.0f));
    layout.add (unitParam (deessFreq, "De-ess Freq", 3000.0f, 10000.0f, 10.0f, 5500.0f, 5500.0f, hz));

    layout.add (boolParam (levelOn, "Auto-Level On", true));
    layout.add (unitParam (levelTarget, "Target Loudness", -30.0f, -10.0f, 0.5f, -18.0f, 0.0f,
                           [] (float v, int) { return juce::String (v, 1) + " LUFS"; }));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { levelMode, 1 }, "Auto-Level Mode",
                                                              juce::StringArray { "Ride", "Learn" }, 0));
    layout.add (boolParam (learnAuto, "Learn With Transport", false));
    layout.add (boolParam (assistant, "Assistant", true));

    layout.add (boolParam (limiterOn, "Limiter On", true));
    layout.add (unitParam (limiterCeil, "Ceiling", -12.0f, 0.0f, 0.1f, -1.0f, 0.0f, db1));

    layout.add (unitParam (outputGain, "Output", -12.0f, 12.0f, 0.1f, 0.0f, 0.0f, db1));
    layout.add (percentParam (mix, "Mix", 100.0f));
    layout.add (boolParam (mono, "Mono", false));
    layout.add (boolParam (matchBypass, "Level-Matched Bypass", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { listen, 1 }, "Listen",
                                                              juce::StringArray { "Output", "Removed" }, 0));
    layout.add (boolParam (bypass, "Bypass", false));
    return layout;
}

//==============================================================================
EZVoiceFXAudioProcessor::EZVoiceFXAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "EZVoiceFX", createLayout())
{
    bypassParam = apvts.getParameter (ParamIDs::bypass);
}

EZVoiceFXAudioProcessor::~EZVoiceFXAudioProcessor()
{
    cancelPendingUpdate();
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
        hum[c].prepare (fs);
        declick[c].prepare (fs);
        denoise[c].prepare (fs);
        deplosive[c].prepare (fs);
        dereverb[c].prepare (fs);
        deess[c].prepare (fs);
        const size_t cap = (size_t) std::max (samplesPerBlock, 8192);
        dryScratch[c].assign (cap, 0.0f);
        cleanScratch[c].assign (cap, 0.0f);
    }
    monoScratch.assign ((size_t) std::max (samplesPerBlock, 8192), 0.0f);

    gate.prepare (fs);
    breath.prepare (fs);
    autoLevel.prepare (fs);
    limiter.prepare (fs);
    outMeter.prepare (fs);
    inMeter.prepare (fs);
    analyzer.prepare (fs);
    analyzing = false;

    const int cleanLatency = declick[0].getLatency() + denoise[0].getLatency()
                           + deplosive[0].getLatency() + dereverb[0].getLatency();
    const int latency = cleanLatency + limiter.getLatency();
    for (int c = 0; c < 2; ++c)
    {
        dryDelay[c].prepare (latency);
        cleanDelay[c].prepare (cleanLatency);
    }
    setLatencySamples (latency);

    outGainSm.reset (fs, 0.05);   outGainSm.setCurrentAndTargetValue (ez::dbToGain (param (ParamIDs::outputGain)));
    bypassSm.reset (fs, 0.03);    bypassSm.setCurrentAndTargetValue (flag (ParamIDs::bypass) ? 1.0f : 0.0f);
    mixSm.reset (fs, 0.05);       mixSm.setCurrentAndTargetValue (param (ParamIDs::mix) * 0.01f);
    matchSm.reset (fs, 0.3);      matchSm.setCurrentAndTargetValue (1.0f);
    deltaSm.reset (fs, 0.03);     deltaSm.setCurrentAndTargetValue (param (ParamIDs::listen) >= 0.5f ? 1.0f : 0.0f);
    lowcutFreqSm.reset (fs, 0.05);
    lowcutFreqSm.setCurrentAndTargetValue (flag (ParamIDs::lowcutOn) ? param (ParamIDs::lowcutFreq) : 10.0f);

    historyLen = (int) std::lround (0.1 * fs);
    historyCounter = 0;

    meters.latencySamples.store (latency);
    meters.sampleRate.store (fs);
}

//==============================================================================
void EZVoiceFXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    using namespace ParamIDs;
    const double t0 = juce::Time::getMillisecondCounterHiRes();

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, n);

    const int nch = std::min (2, buffer.getNumChannels());
    if (nch <= 0 || n <= 0) return;

    float* ch[2] = { buffer.getWritePointer (0), nch > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int c = 0; c < nch; ++c)
    {
        if ((int) dryScratch[c].size() < n) dryScratch[c].resize ((size_t) n);      // rare fallback
        if ((int) cleanScratch[c].size() < n) cleanScratch[c].resize ((size_t) n);
    }
    if ((int) monoScratch.size() < n) monoScratch.resize ((size_t) n);

    // ---- parameters ----
    const auto amt = [this] (const char* onId, const char* amtId) { return flag (onId) ? param (amtId) * 0.01f : 0.0f; };
    const float aHum = amt (humOn, humAmt), aDeclick = amt (declickOn, declickAmt), aDenoise = amt (denoiseOn, denoiseAmt);
    const float aPlos = amt (deplosiveOn, deplosiveAmt), aVerb = amt (dereverbOn, dereverbAmt);
    const float aGate = amt (gateOn, gateAmt), aBreath = amt (breathOn, breathAmt), aDeess = amt (deessOn, deessAmt);
    const int humMode = juce::roundToInt (param (humFreq));
    const bool usePro = param (denoiseEngine) < 0.5f;
    const bool learnMode = param (levelMode) >= 0.5f;

    lowcutFreqSm.setTargetValue (flag (lowcutOn) ? param (lowcutFreq) : 10.0f);
    autoLevel.setParams (flag (levelOn), param (levelTarget), learnMode);
    outGainSm.setTargetValue (ez::dbToGain (param (outputGain)));
    bypassSm.setTargetValue (flag (bypass) ? 1.0f : 0.0f);
    mixSm.setTargetValue (param (mix) * 0.01f);
    deltaSm.setTargetValue (param (listen) >= 0.5f ? 1.0f : 0.0f);

    // ---- learn requests (GUI button or DAW transport) ----
    if (flag (learnAuto) && learnMode)
    {
        bool playing = false;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                playing = pos->getIsPlaying();
        if (playing && ! wasPlaying && ! autoLevel.isLearning()) learnStartReq.store (true);
        if (! playing && wasPlaying && autoLevel.isLearning())   learnStopReq.store (true);
        wasPlaying = playing;
    }
    if (learnStartReq.exchange (false))
    {
        autoLevel.requestLearnStart();
        if (! analysisPending.load()) { analyzer.reset(); analyzing = true; }
    }
    if (learnStopReq.exchange (false))
    {
        autoLevel.requestLearnStop();
        if (analyzing)
        {
            analyzing = false;
            if (flag (assistant)) { analysisPending.store (true); triggerAsyncUpdate(); }
        }
    }

    // ---- mono option ----
    if (flag (mono) && nch == 2)
        for (int i = 0; i < n; ++i) { const float m = 0.5f * (ch[0][i] + ch[1][i]); ch[0][i] = ch[1][i] = m; }

    // ---- input taps: analyser, meter, spectrum, latency-aligned dry ----
    if (analyzing) analyzer.process (ch, nch, n);
    float inPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float m = 0.0f;
        for (int c = 0; c < nch; ++c) { inPeak = std::max (inPeak, std::abs (ch[c][i])); m += ch[c][i]; }
        monoScratch[(size_t) i] = m / (float) nch;
    }
    inputTap.push (monoScratch.data(), n);
    for (int c = 0; c < nch; ++c)
        for (int i = 0; i < n; ++i) dryScratch[c][(size_t) i] = dryDelay[c].process (ch[c][i]);
    {
        const float* d[2] = { dryScratch[0].data(), dryScratch[nch > 1 ? 1 : 0].data() };
        inMeter.process (d, nch, n);
    }

    // ---- 1. low cut ----
    const float lcf = lowcutFreqSm.skip (n);
    for (int c = 0; c < nch; ++c)
    {
        lowcut[c].setFrequency (lcf);
        for (int i = 0; i < n; ++i) ch[c][i] = lowcut[c].process (ch[c][i]);
        for (int i = 0; i < n; ++i) cleanScratch[c][(size_t) i] = cleanDelay[c].process (ch[c][i]);   // for "Removed"
    }

    // ---- 2..6. hum, de-click, AI denoise, de-plosive, de-reverb ----
    const float room = param (roomSize);
    for (int c = 0; c < nch; ++c)
    {
        hum[c].setParams (aHum, humMode);
        declick[c].setAmount (aDeclick);
        denoise[c].setAmount (aDenoise);
        denoise[c].setUsePro (usePro);
        deplosive[c].setAmount (aPlos);
        dereverb[c].setAmount (aVerb);
        dereverb[c].setRoom (room);

        float* d = ch[c];
        for (int i = 0; i < n; ++i)
        {
            float x = hum[c].processSample (d[i], c == 0);
            x = declick[c].processSample (x);
            x = denoise[c].processSample (x);
            x = deplosive[c].processSample (x);
            x = dereverb[c].processSample (x);
            d[i] = x;
        }
    }

    // ---- 7. gate, 8. breath (linked, use the neural voice detector) ----
    const float vad = denoise[0].getVad();
    gate.setVad (vad);
    gate.setAmount (aGate);
    gate.process (ch, nch, n);
    breath.setVad (vad);
    breath.setAmount (aBreath);
    breath.process (ch, nch, n);

    // ---- 9. de-ess ----
    float deessMin = 1.0f, plosiveMin = 1.0f;
    const float dsf = param (deessFreq);
    for (int c = 0; c < nch; ++c)
    {
        deess[c].setAmount (aDeess);
        deess[c].setFrequency (dsf);
        float* d = ch[c];
        for (int i = 0; i < n; ++i)
        {
            d[i] = deess[c].processSample (d[i]);
            deessMin = std::min (deessMin, deess[c].getGain());
        }
        plosiveMin = std::min (plosiveMin, deplosive[c].getGain());
    }

    // ---- "Removed" listen mode: what the clean-up took away ----
    if (deltaSm.isSmoothing() || deltaSm.getTargetValue() > 0.0f)
    {
        for (int i = 0; i < n; ++i)
        {
            const float dl = deltaSm.getNextValue();
            for (int c = 0; c < nch; ++c)
            {
                const float removed = cleanScratch[c][(size_t) i] - ch[c][i];
                ch[c][i] += dl * (removed - ch[c][i]);
            }
        }
    }

    // ---- 10. auto-level, output trim, 11. true-peak limiter ----
    autoLevel.process (ch, nch, n);
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainSm.getNextValue();
        for (int c = 0; c < nch; ++c) ch[c][i] *= g;
    }
    limiter.process (ch, nch, n, param (limiterCeil), flag (limiterOn));

    // ---- global mix, then (level-matched) bypass ----
    const float inL = inMeter.getLufs(), outL = outMeter.getLufs();
    float match = 1.0f;
    if (flag (matchBypass) && inL > -60.0f && outL > -60.0f)
        match = ez::dbToGain (juce::jlimit (-24.0f, 24.0f, outL - inL));
    matchSm.setTargetValue (match);

    const bool needMix = mixSm.isSmoothing() || mixSm.getTargetValue() < 0.999f;
    const bool needBypass = bypassSm.isSmoothing() || bypassSm.getTargetValue() > 0.0f;
    if (needMix || needBypass)
    {
        for (int i = 0; i < n; ++i)
        {
            const float mx = mixSm.getNextValue();
            const float b = bypassSm.getNextValue();
            const float mg = matchSm.getNextValue();
            for (int c = 0; c < nch; ++c)
            {
                const float dry = dryScratch[c][(size_t) i];
                const float wet = dry + mx * (ch[c][i] - dry);
                ch[c][i] = wet + b * (dry * mg - wet);
            }
        }
    }
    else
    {
        mixSm.skip (n); bypassSm.skip (n); matchSm.skip (n);
    }

    // ---- meters & visualisation ----
    float outPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float m = 0.0f;
        for (int c = 0; c < nch; ++c) { outPeak = std::max (outPeak, std::abs (ch[c][i])); m += ch[c][i]; }
        monoScratch[(size_t) i] = m / (float) nch;
    }
    outputTap.push (monoScratch.data(), n);
    outMeter.process (ch, nch, n);

    historyCounter += n;
    if (historyCounter >= historyLen)
    {
        historyCounter = 0;
        const int p = (meters.historyPos.load() + 1) % Meters::kHistory;
        meters.history[p].store (outMeter.getLufs());
        meters.historyPos.store (p);
    }

    auto decayTo = [] (std::atomic<float>& a, float v, float fall)
    {
        const float prev = a.load();
        a.store (v > prev ? v : std::max (v, prev - fall));
    };
    decayTo (meters.inPeakDb, ez::gainToDb (inPeak), 1.5f);
    decayTo (meters.outPeakDb, ez::gainToDb (outPeak), 1.5f);
    meters.outLufs.store (outL);
    meters.inLufs.store (inL);
    meters.autoGainDb.store (autoLevel.getGainDb());
    meters.compGrDb.store (autoLevel.getCompGrDb());
    meters.vad.store (vad);
    decayTo (meters.limiterGrDb, limiter.getGrDb(), 0.5f);
    decayTo (meters.deessGrDb, -ez::gainToDb (deessMin), 0.5f);
    decayTo (meters.plosiveGrDb, -ez::gainToDb (plosiveMin), 0.5f);
    meters.breathGrDb.store (-ez::gainToDb (breath.getGain()));
    meters.gateGrDb.store (-ez::gainToDb (gate.getGain()));
    meters.clicks.store (declick[0].getClickCount() + (nch > 1 ? declick[1].getClickCount() : 0));
    meters.humBase.store ((float) hum[0].getBase());
    proActive.store (denoise[0].isProActive());

    for (int c = nch; c < buffer.getNumChannels(); ++c)
        buffer.copyFrom (c, 0, buffer, 0, 0, n);

    const double blockMs = 1000.0 * n / fs;
    const float load = (float) ((juce::Time::getMillisecondCounterHiRes() - t0) / blockMs);
    meters.cpu.store (meters.cpu.load() * 0.9f + 0.1f * load);
}

//==============================================================================
void EZVoiceFXAudioProcessor::setParam (const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void EZVoiceFXAudioProcessor::handleAsyncUpdate()
{
    if (! analysisPending.load()) return;

    ez::Suggestion s;
    juce::String report;
    if (analyzer.compute (s))
    {
        using namespace ParamIDs;
        setParam (denoiseAmt, std::round (s.denoise));
        setParam (gateAmt, std::round (s.gate));
        setParam (deessAmt, std::round (s.deess));
        setParam (deplosiveAmt, std::round (s.deplosive));
        if (s.humOn) { setParam (humOn, 1.0f); setParam (humAmt, s.hum); }

        report << juce::String::fromUTF8 ("Шум: ") << juce::String (s.noiseFloor, 0) << " LUFS, "
               << juce::String::fromUTF8 ("голос/шум: ") << juce::String (s.snrDb, 0) << " dB\n"
               << "Denoise " << juce::roundToInt (s.denoise) << "%, Gate " << juce::roundToInt (s.gate)
               << "%, De-ess " << juce::roundToInt (s.deess) << "%, De-plosive " << juce::roundToInt (s.deplosive) << "%"
               << (s.humOn ? juce::String::fromUTF8 (", гул найден") : juce::String());
    }
    else
    {
        report = juce::String::fromUTF8 ("Слишком короткий фрагмент для анализа (нужно от 2 секунд).");
    }
    {
        const juce::ScopedLock sl (reportLock);
        assistantReport = report;
    }
    analysisPending.store (false);
}

//==============================================================================
void EZVoiceFXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("learnedLufs", (double) autoLevel.getLearnedLufs(), nullptr);
    state.setProperty ("learnedValid", autoLevel.hasLearned(), nullptr);
    state.setProperty ("version", "1.1.0", nullptr);
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
