// EZ Voice FX - AI noise reduction with two local neural engines
//
//  AI Pro  : DeepFilterNet3 (MIT/Apache) loaded at run time from ezvfx_deepfilter.dll
//            + DeepFilterNet3_onnx.tar.gz placed next to the plugin binary.
//  AI Lite : RNNoise (BSD), compiled into the plugin, always available.
//
// Both engines run on 480-sample frames at 48 kHz. Other host rates are resampled.
// Engine outputs are padded to one common delay so switching engines never changes
// the plugin latency. The DeepFilterNet delay is verified at run time by
// cross-correlation and the padding is corrected automatically.
#pragma once

#include "Common.h"
#include <cstdio>
#include <mutex>
#include <rnnoise.h>
#include <juce_core/juce_core.h>

namespace ez
{
//==============================================================================
class DeepFilterLib
{
public:
    using CreateFn   = void* (*) (const char*, float, const char*);
    using FrameLenFn = size_t (*) (void*);
    using ProcessFn  = float (*) (void*, float*, float*);
    using SetLimFn   = void (*) (void*, float);
    using FreeFn     = void (*) (void*);

    static DeepFilterLib& get()
    {
        // intentionally leaked: never unload the DLL during static destruction
        static DeepFilterLib* lib = new DeepFilterLib();
        return *lib;
    }

    bool isAvailable() const noexcept { return available; }
    juce::String getStatus() const { return status; }

    void* create (float attenDb)
    {
        std::lock_guard<std::mutex> lock (mutex);
        if (! available) return nullptr;
        void* st = createFn (modelPath.toRawUTF8(), attenDb, nullptr);
        if (st == nullptr) return nullptr;
        if ((int) frameLenFn (st) != 480) { freeFn (st); return nullptr; }
        return st;
    }

    void destroy (void* st)
    {
        std::lock_guard<std::mutex> lock (mutex);
        if (st != nullptr && freeFn != nullptr) freeFn (st);
    }

    float process (void* st, float* in, float* out) const noexcept { return processFn (st, in, out); }
    void setAttenuation (void* st, float db) const noexcept { setLimFn (st, db); }

private:
    DeepFilterLib()
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
       #if JUCE_WINDOWS
        const auto dll = dir.getChildFile ("ezvfx_deepfilter.dll");
       #elif JUCE_MAC
        const auto dll = dir.getChildFile ("libezvfx_deepfilter.dylib");
       #else
        const auto dll = dir.getChildFile ("libezvfx_deepfilter.so");
       #endif
        const auto model = dir.getChildFile ("DeepFilterNet3_onnx.tar.gz");

        if (! dll.existsAsFile())   { status = "AI Pro: engine file not found, using AI Lite"; return; }
        if (! model.existsAsFile()) { status = "AI Pro: model file not found, using AI Lite"; return; }
        if (! lib.open (dll.getFullPathName())) { status = "AI Pro: engine failed to load, using AI Lite"; return; }

        createFn   = reinterpret_cast<CreateFn>   (lib.getFunction ("df_create"));
        frameLenFn = reinterpret_cast<FrameLenFn> (lib.getFunction ("df_get_frame_length"));
        processFn  = reinterpret_cast<ProcessFn>  (lib.getFunction ("df_process_frame"));
        setLimFn   = reinterpret_cast<SetLimFn>   (lib.getFunction ("df_set_atten_lim"));
        freeFn     = reinterpret_cast<FreeFn>     (lib.getFunction ("df_free"));

        if (createFn == nullptr || frameLenFn == nullptr || processFn == nullptr
            || setLimFn == nullptr || freeFn == nullptr)
        {
            status = "AI Pro: incompatible engine, using AI Lite";
            return;
        }
        modelPath = model.getFullPathName();
        available = true;
        status = "AI Pro (DeepFilterNet3) ready";
    }

    juce::DynamicLibrary lib;
    CreateFn createFn = nullptr;
    FrameLenFn frameLenFn = nullptr;
    ProcessFn processFn = nullptr;
    SetLimFn setLimFn = nullptr;
    FreeFn freeFn = nullptr;
    juce::String modelPath, status { "AI Pro: not loaded" };
    bool available = false;
    std::mutex mutex;
};

//==============================================================================
// Streaming cubic-Hermite resampler: push one input sample, it emits 0..n outputs.
class StreamResampler
{
public:
    void prepare (double inRate, double outRate) { step = inRate / outRate; reset(); }
    void reset() noexcept { h[0] = h[1] = h[2] = h[3] = 0.0f; pos = 0.0; }

    template <typename Emit>
    inline void push (float x, Emit&& emit)
    {
        h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = x;
        while (pos < 1.0)
        {
            emit (hermite ((float) pos));
            pos += step;
        }
        pos -= 1.0;
    }

private:
    inline float hermite (float t) const noexcept
    {
        const float y0 = h[0], y1 = h[1], y2 = h[2], y3 = h[3];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    float h[4] {};
    double pos = 0.0, step = 1.0;
};

//==============================================================================
class DenoiseChannel
{
public:
    static constexpr int kFrame = 480;
    static constexpr int kRnDelay = 480;      // RNNoise: one frame
    static constexpr int kMaxDelay = 1920;    // common engine delay (48 kHz samples)
    static constexpr int kNumLags = 4;

    DenoiseChannel() = default;
    ~DenoiseChannel()
    {
        releaseRn();
        if (df != nullptr) DeepFilterLib::get().destroy (df);
    }
    DenoiseChannel (const DenoiseChannel&) = delete;
    DenoiseChannel& operator= (const DenoiseChannel&) = delete;

    // call from prepareToPlay (may take ~0.3 s the first time: model loading)
    void prepare (double hostRate)
    {
        releaseRn();
        rn = rnnoise_create (nullptr);
        if (df == nullptr && DeepFilterLib::get().isAvailable())
            df = DeepFilterLib::get().create (60.0f);
        lastLim = -1.0f;

        resample = std::abs (hostRate - 48000.0) > 0.5;
        ratio = hostRate / 48000.0;
        if (resample)
        {
            up.prepare (hostRate, 48000.0);
            down.prepare (48000.0, hostRate);
        }
        antiAlias = resample && hostRate < 47999.0;
        aa1.setLowpass (48000.0, 0.45 * hostRate, 0.54119610);
        aa2.setLowpass (48000.0, 0.45 * hostRate, 1.30656296);
        aa1.reset(); aa2.reset();

        prefill = resample ? (int) std::ceil (kFrame * ratio) + 8 : kFrame;
        latency = resample ? prefill + (int) std::lround (kMaxDelay * ratio + 2.0 + 2.0 * ratio)
                           : prefill + kMaxDelay;

        const int fifoLen = nextPow2 (prefill * 2 + (int) std::ceil (kFrame * ratio) * 2 + 64);
        fifo.assign ((size_t) fifoLen, 0.0f);
        fifoMask = fifoLen - 1;
        fifoRead = 0; fifoWrite = prefill; fifoCount = prefill;

        rnPad.prepare (kMaxDelay - kRnDelay);
        dfPad.prepare (kMaxDelay);
        hist.assign (4096, 0.0f);
        histMask = 4095;
        n48 = 0;

        dfDelay = 1440;      // expected: (fft - hop) + 2 lookahead frames
        dfLocked = false;
        voicedFrames = 0;
        for (auto& c : corr) c = 0.0;

        std::fill (std::begin (inBuf), std::end (inBuf), 0.0f);
        std::fill (std::begin (prevIn), std::end (prevIn), 0.0f);
        inCount = 0;
        amtCur = amtTarget = 0.0f;
        dfW = 0.0f; dfRunFrames = 0;
        vad.store (0.0f);
    }

    int getLatency() const noexcept { return latency; }
    void setAmount (float a) noexcept { amtTarget = std::clamp (a, 0.0f, 1.0f); }
    void setUsePro (bool p) noexcept { usePro = p; }
    bool isProActive() const noexcept { return df != nullptr && dfW > 0.5f; }
    bool hasPro() const noexcept { return df != nullptr; }
    float getVad() const noexcept { return vad.load(); }

    inline float processSample (float x) noexcept
    {
        if (resample) up.push (x, [this] (float s) { feed48 (s); });
        else          feed48 (x);
        return pop();
    }

private:
    void releaseRn()
    {
        if (rn != nullptr) { rnnoise_destroy (rn); rn = nullptr; }
    }

    inline void feed48 (float s) noexcept
    {
        inBuf[inCount++] = s;
        if (inCount == kFrame) { inCount = 0; processFrame(); }
    }

    void processFrame() noexcept
    {
        const float a0 = amtCur, a1 = amtTarget;
        amtCur = a1;
        constexpr float inv = 1.0f / 32768.0f;

        // history for delay verification
        const int64_t frameStart = n48;
        for (int i = 0; i < kFrame; ++i) hist[(size_t) ((frameStart + i) & histMask)] = inBuf[i];
        n48 += kFrame;

        // ---- AI Lite (RNNoise): dry/wet mix, dry aligned with the 1-frame network delay
        float vadRn = 0.0f;
        if (rn != nullptr)
        {
            for (int i = 0; i < kFrame; ++i) scaled[i] = std::clamp (inBuf[i], -1.5f, 1.5f) * 32768.0f;
            vadRn = rnnoise_process_frame (rn, rnOut, scaled);
            for (int i = 0; i < kFrame; ++i)
            {
                float wet = rnOut[i] * inv;
                if (! std::isfinite (wet)) wet = 0.0f;
                const float m = a0 + (a1 - a0) * (float) (i + 1) / (float) kFrame;
                rnOut[i] = prevIn[i] + m * (wet - prevIn[i]);
            }
        }
        else
        {
            for (int i = 0; i < kFrame; ++i) rnOut[i] = prevIn[i];
        }

        // ---- AI Pro (DeepFilterNet3): strength = attenuation limit
        const bool wantPro = usePro && df != nullptr;
        float lsnr = -15.0f;
        if (df != nullptr && (wantPro || dfW > 0.0f))
        {
            const float lim = a1 >= 0.995f ? 100.0f : std::max (0.5f, a1 * 60.0f);
            if (std::abs (lim - lastLim) > 0.1f)
            {
                DeepFilterLib::get().setAttenuation (df, lim);
                lastLim = lim;
            }
            std::copy (std::begin (inBuf), std::end (inBuf), std::begin (dfIn));
            lsnr = DeepFilterLib::get().process (df, dfIn, dfOut);
            for (int i = 0; i < kFrame; ++i)
                if (! std::isfinite (dfOut[i])) dfOut[i] = 0.0f;
            ++dfRunFrames;
            if (! dfLocked) verifyDelay (frameStart);
        }
        else
        {
            dfRunFrames = 0;
            std::fill (std::begin (dfOut), std::end (dfOut), 0.0f);
        }

        const float wTarget = (wantPro && dfRunFrames > 6) ? 1.0f : 0.0f;
        constexpr float wStep = 1.0f / (0.08f * 48000.0f);
        const int dfPadLen = kMaxDelay - dfDelay;

        for (int i = 0; i < kFrame; ++i)
        {
            if (dfW < wTarget) dfW = std::min (wTarget, dfW + wStep);
            else if (dfW > wTarget) dfW = std::max (wTarget, dfW - wStep);

            const float r = rnPad.process (rnOut[i]);
            const float d = dfPad.process (dfOut[i], dfPadLen);
            emit48 (r + dfW * (d - r));
        }

        vad.store (dfW > 0.5f ? std::clamp (lsnr / 20.0f, 0.0f, 1.0f) : vadRn);
        std::copy (std::begin (inBuf), std::end (inBuf), std::begin (prevIn));
    }

    // Cross-correlate the DF output with the delayed input at the plausible lags.
    void verifyDelay (int64_t frameStart) noexcept
    {
        double e = 0.0;
        for (int i = 0; i < kFrame; ++i) e += (double) inBuf[i] * inBuf[i];
        if (e / kFrame < 1.0e-5) return;     // only voiced / loud frames

        static constexpr int lags[kNumLags] = { 480, 960, 1440, 1920 };
        for (int l = 0; l < kNumLags; ++l)
        {
            double c = 0.0;
            for (int i = 0; i < kFrame; ++i)
                c += (double) dfOut[i] * hist[(size_t) ((frameStart + i - lags[l]) & histMask)];
            corr[l] += c;
        }
        if (++voicedFrames >= 200)
        {
            int best = 0;
            for (int l = 1; l < kNumLags; ++l) if (corr[l] > corr[best]) best = l;
            if (corr[best] > 0.0) dfDelay = lags[best];
            dfLocked = true;
        }
    }

    inline void emit48 (float y) noexcept
    {
        if (antiAlias) y = aa2.process (aa1.process (y));
        if (resample) down.push (y, [this] (float z) { pushFifo (z); });
        else          pushFifo (y);
    }

    inline void pushFifo (float z) noexcept
    {
        if (fifoCount >= (int) fifo.size()) return;
        fifo[(size_t) fifoWrite] = z;
        fifoWrite = (fifoWrite + 1) & fifoMask;
        ++fifoCount;
    }

    inline float pop() noexcept
    {
        if (fifoCount <= 0) return 0.0f;
        const float v = fifo[(size_t) fifoRead];
        fifoRead = (fifoRead + 1) & fifoMask;
        --fifoCount;
        return v;
    }

    DenoiseState* rn = nullptr;
    void* df = nullptr;
    bool usePro = true;
    float lastLim = -1.0f;

    bool resample = false, antiAlias = false;
    double ratio = 1.0;
    StreamResampler up, down;
    Biquad aa1, aa2;

    float inBuf[kFrame] {}, prevIn[kFrame] {}, scaled[kFrame] {}, rnOut[kFrame] {};
    float dfIn[kFrame] {}, dfOut[kFrame] {};
    int inCount = 0;

    DelayLine rnPad;
    VarDelay dfPad;
    std::vector<float> hist;
    int histMask = 4095;
    int64_t n48 = 0;
    int dfDelay = 1440, voicedFrames = 0, dfRunFrames = 0;
    bool dfLocked = false;
    double corr[kNumLags] {};
    float dfW = 0.0f;

    std::vector<float> fifo;
    int fifoMask = 0, fifoRead = 0, fifoWrite = 0, fifoCount = 0;
    int prefill = kFrame, latency = kFrame + kMaxDelay;

    float amtCur = 0.0f, amtTarget = 0.0f;
    std::atomic<float> vad { 0.0f };
};
} // namespace ez
