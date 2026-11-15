#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::String percentText (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; }
juce::String ru (const char* s) { return juce::String::fromUTF8 (s); }

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

std::unique_ptr<juce::AudioParameterChoice> choiceParam (const char* id, const char* name, juce::StringArray items, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
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
    layout.add (choiceParam (humFreq, "Hum Frequency", { "Auto", "50 Hz", "60 Hz" }, 0));
    layout.add (boolParam (declickOn, "De-click On", true));
    layout.add (percentParam (declickAmt, "De-click", 40.0f));
    layout.add (boolParam (denoiseOn, "AI Denoise On", true));
    layout.add (percentParam (denoiseAmt, "AI Denoise", 75.0f));
    layout.add (choiceParam (aiEngine, "AI Engine", { "Auto", "AI Pro (DeepFilterNet3)", "AI Lite (RNNoise)" }, 0));
    layout.add (boolParam (whisper, "Whisper Protection", false));
    layout.add (boolParam (deplosiveOn, "De-plosive On", true));
    layout.add (percentParam (deplosiveAmt, "De-plosive", 50.0f));
    layout.add (boolParam (dereverbOn, "De-reverb On", true));
    layout.add (percentParam (dereverbAmt, "De-reverb", 30.0f));
    layout.add (unitParam (roomSize, "Room Size", 0.2f, 1.5f, 0.01f, 0.6f, 0.6f, [] (float v, int) { return juce::String (v, 2) + " s"; }));
    layout.add (boolParam (gateOn, "Gate On", true));
    layout.add (percentParam (gateAmt, "Gate", 40.0f));
    layout.add (boolParam (breathOn, "Breath On", true));
    layout.add (percentParam (breathAmt, "Breath", 30.0f));
    layout.add (boolParam (deessOn, "De-ess On", true));
    layout.add (percentParam (deessAmt, "De-ess", 50.0f));
    layout.add (unitParam (deessFreq, "De-ess Freq", 3000.0f, 10000.0f, 10.0f, 5500.0f, 5500.0f, hz));
    layout.add (boolParam (voxOn, "VOX On", true));
    layout.add (percentParam (voxAmt, "VOX", 25.0f));
    layout.add (boolParam (fitOn, "Fit to Beat On", true));
    layout.add (percentParam (fitAmt, "Fit to Beat", 50.0f));
    layout.add (boolParam (levelOn, "Auto-Level On", true));
    layout.add (unitParam (levelTarget, "Target Loudness", -30.0f, -10.0f, 0.5f, -18.0f, 0.0f,
                           [] (float v, int) { return juce::String (v, 1) + " LUFS"; }));
    layout.add (choiceParam (levelMode, "Auto-Level Mode", { "Ride", "Learn" }, 0));
    layout.add (boolParam (learnAuto, "Learn With Transport", false));
    layout.add (boolParam (assistant, "Assistant", true));
    layout.add (choiceParam (linkGroup, "Link Group", { "Off", "A", "B", "C", "D" }, 0));
    layout.add (boolParam (limiterOn, "Limiter On", true));
    layout.add (unitParam (limiterCeil, "Ceiling", -12.0f, 0.0f, 0.1f, -1.0f, 0.0f, db1));
    layout.add (unitParam (outputGain, "Output", -12.0f, 12.0f, 0.1f, 0.0f, 0.0f, db1));
    layout.add (percentParam (mix, "Mix", 100.0f));
    layout.add (boolParam (mono, "Mono", false));
    layout.add (boolParam (matchBypass, "Level-Matched Bypass", true));
    layout.add (choiceParam (listen, "Listen", { "Output", "Removed" }, 0));
    layout.add (choiceParam (role, "Role", { "Vocal", "Beat Link" }, 0));
    layout.add (percentParam (unmaskAmt, "Beat Unmask", 40.0f));
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
    hubSlot = ez::LinkHub::get().acquire();

    cpuCores = juce::SystemStats::getNumPhysicalCpus();
    ramMb = juce::SystemStats::getMemorySizeInMegabytes();
    avx2 = juce::SystemStats::hasAVX2();
    cpuName = juce::SystemStats::getCpuModel().trim();
}

EZVoiceFXAudioProcessor::~EZVoiceFXAudioProcessor()
{
    cancelPendingUpdate();
    ez::LinkHub::get().release (hubSlot);
}

bool EZVoiceFXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

bool EZVoiceFXAudioProcessor::isProRecommended() const
{
    if (! isProAvailable() || ! avx2 || cpuCores < 4 || ramMb < 7000) return false;
    const float bench = ez::DeepFilterLib::get().getBenchMs();
    return bench >= 0.0f && bench < 2.5f;      // < 25 % of real time per channel
}

juce::String EZVoiceFXAudioProcessor::getHardwareText() const
{
    const float bench = ez::DeepFilterLib::get().getBenchMs();
    juce::String s;
    s << (cpuName.isNotEmpty() ? cpuName : juce::String ("CPU")) << "\n"
      << cpuCores << ru (" ядер, ") << juce::roundToInt (ramMb / 1024.0f) << ru (" ГБ ОЗУ, AVX2: ") << (avx2 ? ru ("да") : ru ("нет")) << "\n"
      << ru ("Тест AI Pro: ") << (bench < 0.0f ? ru ("идёт...") : juce::String (bench, 2) + ru (" мс на 10 мс звука")) << "\n"
      << ru ("Рекомендовано: ") << (isProRecommended() ? "AI Pro" : ru ("AI Lite (эко)"));
    return s;
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
    vox.prepare (fs);
    fit.prepare (fs);
    unmask.prepare (fs);
    bandMeter.prepare (fs);
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
    setLatencySamples (latency);     // same latency in both roles: no PDC changes when switching

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
// Beat Link role: analyse the beat, publish it, make room for the vocals.
void EZVoiceFXAudioProcessor::processBeatRole (float* const* ch, int nch, int n)
{
    auto& hub = ez::LinkHub::get();
    for (int i = 0; i < n; ++i) bandMeter.process (monoScratch[(size_t) i]);
    float bands[ez::LinkHub::kBands], vb[ez::LinkHub::kBands];
    bandMeter.getRms (bands);
    hub.publish (hubSlot, ez::LinkHub::Beat, bands, inMeter.getLufs());

    const int vocals = hub.gather (ez::LinkHub::Vocal, hubSlot, vb);
    unmask.computeTargets (param (ParamIDs::unmaskAmt) * 0.01f, vocals, vb, bands);

    for (int c = 0; c < nch; ++c)
        for (int i = 0; i < n; ++i)
            ch[c][i] = unmask.processSample (c, dryScratch[c][(size_t) i]);

    linkedVocals.store (vocals);
    for (int b = 0; b < ez::LinkHub::kBands; ++b) { meters.ownBands[b].store (bands[b]); meters.otherBands[b].store (vb[b]); }
    for (int k = 0; k < 4; ++k) meters.eqGains[k].store (unmask.getGain (k));
}

void EZVoiceFXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    using namespace ParamIDs;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    auto& hub = ez::LinkHub::get();

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

    const bool beatRole = param (role) >= 0.5f;

    // ---- transport ----
    bool hasTransport = false, playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hasTransport = true;
            playing = pos->getIsPlaying();
        }

    // ---- mono option, input taps, latency-aligned dry ----
    if (flag (mono) && nch == 2)
        for (int i = 0; i < n; ++i) { const float m = 0.5f * (ch[0][i] + ch[1][i]); ch[0][i] = ch[1][i] = m; }

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

    if (beatRole)
    {
        processBeatRole (ch, nch, n);
    }
    else
    {
        // ---- parameters ----
        const auto amt = [this] (const char* onId, const char* amtId) { return flag (onId) ? param (amtId) * 0.01f : 0.0f; };
        const float aHum = amt (humOn, humAmt), aDeclick = amt (declickOn, declickAmt), aDenoise = amt (denoiseOn, denoiseAmt);
        const float aPlos = amt (deplosiveOn, deplosiveAmt), aVerb = amt (dereverbOn, dereverbAmt);
        const float aGate = amt (gateOn, gateAmt), aBreath = amt (breathOn, breathAmt), aDeess = amt (deessOn, deessAmt);
        const float aVox = amt (voxOn, voxAmt), aFit = amt (fitOn, fitAmt);
        const int humMode = juce::roundToInt (param (humFreq));
        const int engine = juce::roundToInt (param (aiEngine));
        const bool usePro = engine == 1 || (engine == 0 && isProRecommended());
        const bool whisperOn = flag (whisper);
        const bool learnMode = param (levelMode) >= 0.5f;
        const int group = juce::roundToInt (param (linkGroup));

        lowcutFreqSm.setTargetValue (flag (lowcutOn) ? param (lowcutFreq) : 10.0f);
        autoLevel.setParams (flag (levelOn), param (levelTarget), learnMode);
        outGainSm.setTargetValue (ez::dbToGain (param (outputGain)));
        mixSm.setTargetValue (param (mix) * 0.01f);
        deltaSm.setTargetValue (param (listen) >= 0.5f ? 1.0f : 0.0f);

        // ---- group sync: start/stop learn together with the other instances ----
        if (group != lastGroup)
        {
            lastGroup = group;
            lastStartSeq = hub.getStartSeq (group);
            lastStopSeq = hub.getStopSeq (group);
        }
        if (group > 0)
        {
            const int s0 = hub.getStartSeq (group), s1 = hub.getStopSeq (group);
            if (s0 != lastStartSeq) { lastStartSeq = s0; learnStartReq.store (true); }
            if (s1 != lastStopSeq)  { lastStopSeq = s1;  learnStopReq.store (true); }
        }

        // ---- magic state machine ----
        if (magicCancelReq.exchange (false))
        {
            if (magicState.load() == MagicListening) { learnBroadcast.store (true); learnStopReq.store (true); }
            magicAnalysis.store (false);
            magicState.store (MagicIdle);
        }
        if (magicStartReq.exchange (false))
        {
            magicState.store (MagicWaiting);
            listenSamples = 0; listenBlocks = 0; beatBlocks = 0;
            listenSeconds.store (0.0f);
        }
        const int ms = magicState.load();
        if (ms == MagicWaiting && (hasTransport ? playing : inPeak > 0.003f))
        {
            learnBroadcast.store (true);
            learnStartReq.store (true);
            magicState.store (MagicListening);
        }
        else if (ms == MagicListening)
        {
            listenSamples += n;
            ++listenBlocks;
            if (linkedBeats.load() > 0) ++beatBlocks;
            listenSeconds.store ((float) ((double) listenSamples / fs));
            const bool stopped = hasTransport && ! playing && listenSamples > (int64_t) (fs * 0.5);
            if (magicFinishReq.exchange (false) || stopped || listenSamples > (int64_t) (fs * 120.0))
            {
                magicAnalysis.store (true);
                learnBroadcast.store (true);
                learnStopReq.store (true);
                magicState.store (MagicAnalyzing);
            }
        }
        else if (flag (learnAuto) && learnMode)        // learn with DAW transport
        {
            if (playing && ! wasPlaying && ! autoLevel.isLearning()) { learnBroadcast.store (true); learnStartReq.store (true); }
            if (! playing && wasPlaying && autoLevel.isLearning())   { learnBroadcast.store (true); learnStopReq.store (true); }
        }
        wasPlaying = playing;

        // ---- learn requests ----
        const bool broadcast = learnBroadcast.exchange (false);
        if (learnStartReq.exchange (false))
        {
            autoLevel.requestLearnStart();
            if (! analysisPending.load()) { analyzer.reset(); analyzing = true; }
            if (broadcast && group > 0) { hub.broadcastStart (group); lastStartSeq = hub.getStartSeq (group); }
        }
        if (learnStopReq.exchange (false))
        {
            autoLevel.requestLearnStop();
            if (broadcast && group > 0) { hub.broadcastStop (group); lastStopSeq = hub.getStopSeq (group); }
            if (analyzing)
            {
                analyzing = false;
                if (flag (assistant) || magicAnalysis.load()) { analysisPending.store (true); triggerAsyncUpdate(); }
            }
            else if (magicAnalysis.load())
            {
                analysisPending.store (true);
                triggerAsyncUpdate();
            }
        }
        if (analyzing) analyzer.process (ch, nch, n);

        // ---- band analysis + links ----
        for (int i = 0; i < n; ++i) bandMeter.process (monoScratch[(size_t) i]);
        float bands[ez::LinkHub::kBands], beatBands[ez::LinkHub::kBands];
        bandMeter.getRms (bands);
        const float vad = denoise[0].getVad();
        float pub[ez::LinkHub::kBands];
        for (int b = 0; b < ez::LinkHub::kBands; ++b) pub[b] = vad > 0.3f ? bands[b] : 0.0f;
        hub.publish (hubSlot, ez::LinkHub::Vocal, pub, outMeter.getLufs());
        const int beats = hub.gather (ez::LinkHub::Beat, hubSlot, beatBands);
        linkedBeats.store (beats);
        linkedVocals.store (hub.countRole (ez::LinkHub::Vocal));

        // ---- 1. low cut ----
        const float lcf = lowcutFreqSm.skip (n);
        for (int c = 0; c < nch; ++c)
        {
            lowcut[c].setFrequency (lcf);
            for (int i = 0; i < n; ++i) ch[c][i] = lowcut[c].process (ch[c][i]);
            for (int i = 0; i < n; ++i) cleanScratch[c][(size_t) i] = cleanDelay[c].process (ch[c][i]);
        }

        // ---- 2..6. hum, de-click, AI denoise, de-plosive, de-reverb ----
        const float room = param (roomSize);
        for (int c = 0; c < nch; ++c)
        {
            hum[c].setParams (aHum, humMode);
            declick[c].setAmount (aDeclick);
            denoise[c].setAmount (aDenoise);
            denoise[c].setUsePro (usePro);
            denoise[c].setWhisper (whisperOn);
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

        // ---- 7. gate, 8. breath ----
        gate.setVad (vad);  gate.setWhisper (whisperOn);  gate.setAmount (aGate);  gate.process (ch, nch, n);
        breath.setVad (vad); breath.setWhisper (whisperOn); breath.setAmount (aBreath); breath.process (ch, nch, n);

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

        // ---- "Removed" listen mode ----
        if (deltaSm.isSmoothing() || deltaSm.getTargetValue() > 0.0f)
            for (int i = 0; i < n; ++i)
            {
                const float dl = deltaSm.getNextValue();
                for (int c = 0; c < nch; ++c)
                    ch[c][i] += dl * ((cleanScratch[c][(size_t) i] - ch[c][i]) - ch[c][i]);
            }

        // ---- 10. VOX, 11. Fit to Beat ----
        vox.setAmount (aVox);
        vox.process (ch, nch, n);
        fit.computeTargets (aFit, beats > 0, beatBands, bands);
        for (int c = 0; c < nch; ++c)
            for (int i = 0; i < n; ++i) ch[c][i] = fit.processSample (c, ch[c][i]);

        // ---- 12. auto-level, output trim, 13. true-peak limiter ----
        autoLevel.process (ch, nch, n);
        for (int i = 0; i < n; ++i)
        {
            const float g = outGainSm.getNextValue();
            for (int c = 0; c < nch; ++c) ch[c][i] *= g;
        }
        limiter.process (ch, nch, n, param (limiterCeil), flag (limiterOn));

        // ---- vocal-only meters ----
        meters.autoGainDb.store (autoLevel.getGainDb());
        meters.compGrDb.store (autoLevel.getCompGrDb());
        meters.voxGrDb.store (vox.getGrDb());
        meters.vad.store (vad);
        meters.deessGrDb.store (-ez::gainToDb (deessMin));
        meters.plosiveGrDb.store (-ez::gainToDb (plosiveMin));
        meters.breathGrDb.store (-ez::gainToDb (breath.getGain()));
        meters.gateGrDb.store (-ez::gainToDb (gate.getGain()));
        meters.limiterGrDb.store (limiter.getGrDb());
        meters.clicks.store (declick[0].getClickCount() + (nch > 1 ? declick[1].getClickCount() : 0));
        meters.humBase.store ((float) hum[0].getBase());
        for (int b = 0; b < ez::LinkHub::kBands; ++b) { meters.ownBands[b].store (bands[b]); meters.otherBands[b].store (beatBands[b]); }
        for (int k = 0; k < 4; ++k) meters.eqGains[k].store (fit.getGain (k));
        proActive.store (denoise[0].isProActive());
    }

    // ---- global mix, then (level-matched) bypass ----
    bypassSm.setTargetValue (flag (bypass) ? 1.0f : 0.0f);
    if (beatRole) mixSm.setTargetValue (1.0f);
    const float inL = inMeter.getLufs(), outL = outMeter.getLufs();
    float match = 1.0f;
    if (! beatRole && flag (matchBypass) && inL > -60.0f && outL > -60.0f)
        match = ez::dbToGain (juce::jlimit (-24.0f, 24.0f, outL - inL));
    matchSm.setTargetValue (match);

    if (mixSm.isSmoothing() || mixSm.getTargetValue() < 0.999f || bypassSm.isSmoothing() || bypassSm.getTargetValue() > 0.0f)
    {
        for (int i = 0; i < n; ++i)
        {
            const float mx = mixSm.getNextValue(), b = bypassSm.getNextValue(), mg = matchSm.getNextValue();
            for (int c = 0; c < nch; ++c)
            {
                const float dry = dryScratch[c][(size_t) i];
                const float wet = dry + mx * (ch[c][i] - dry);
                ch[c][i] = wet + b * (dry * mg - wet);
            }
        }
    }
    else { mixSm.skip (n); bypassSm.skip (n); matchSm.skip (n); }

    // ---- common meters & visualisation ----
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
    using namespace ParamIDs;

    const bool magic = magicAnalysis.exchange (false);
    ez::Suggestion s;
    juce::String report, summary;

    if (analyzer.compute (s))
    {
        setParam (denoiseAmt, std::round (s.denoise));
        setParam (gateAmt, std::round (s.gate));
        setParam (deessAmt, std::round (s.deess));
        setParam (deplosiveAmt, std::round (s.deplosive));
        if (s.humOn) { setParam (humOn, 1.0f); setParam (humAmt, s.hum); }

        report << ru ("Шум: ") << juce::String (s.noiseFloor, 0) << " LUFS, " << ru ("голос/шум: ")
               << juce::String (s.snrDb, 0) << " dB\n"
               << "Denoise " << juce::roundToInt (s.denoise) << "%, Gate " << juce::roundToInt (s.gate)
               << "%, De-ess " << juce::roundToInt (s.deess) << "%, De-plosive " << juce::roundToInt (s.deplosive) << "%"
               << (s.humOn ? ru (", гул найден") : juce::String());

        if (magic)
        {
            setParam (voxOn, 1.0f);
            setParam (voxAmt, std::round (s.vox));
            setParam (whisper, s.whisperOn ? 1.0f : 0.0f);
            const bool beatSeen = listenBlocks > 0 && beatBlocks > listenBlocks / 3;
            setParam (fitOn, 1.0f);
            setParam (fitAmt, beatSeen ? 60.0f : 30.0f);
            setParam (levelOn, 1.0f);
            setParam (levelMode, 1.0f);

            summary << ru ("Шум убран: Denoise ") << juce::roundToInt (s.denoise) << "%, Gate " << juce::roundToInt (s.gate) << "%\n"
                    << ru ("Сибилянты: De-ess ") << juce::roundToInt (s.deess) << "%,  " << ru ("взрывные: ") << juce::roundToInt (s.deplosive) << "%\n"
                    << ru ("Динамика голоса ") << juce::String (s.lra, 1) << " dB  ->  VOX " << juce::roundToInt (s.vox) << "%\n"
                    << (s.whisperOn ? ru ("Найден шёпот: включена защита шёпота\n") : juce::String())
                    << (s.humOn ? ru ("Найден сетевой гул: включено подавление\n") : juce::String())
                    << (beatSeen ? ru ("Бит найден: вокал врезается в бит (Fit 60%)\n") : ru ("Бит не найден: мягкая полировка (Fit 30%)\n"))
                    << ru ("Громкость: ") << (hasLearned() ? juce::String (getLearnedLufs(), 1) + " -> " + juce::String (param (levelTarget), 1) + " LUFS"
                                                         : ru ("замер не удался"));
        }
    }
    else
    {
        report = ru ("Слишком короткий фрагмент для анализа (нужно от 2 секунд).");
        summary = ru ("Слишком короткий фрагмент. Проиграйте капу хотя бы 5-10 секунд и попробуйте ещё раз.");
    }
    {
        const juce::ScopedLock sl (reportLock);
        assistantReport = report;
        if (magic) magicSummary = summary;
    }
    if (magic) magicState.store (MagicDone);
    analysisPending.store (false);
}

//==============================================================================
void EZVoiceFXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("learnedLufs", (double) autoLevel.getLearnedLufs(), nullptr);
    state.setProperty ("learnedValid", autoLevel.hasLearned(), nullptr);
    state.setProperty ("version", "1.2.0", nullptr);
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
