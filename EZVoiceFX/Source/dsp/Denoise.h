// EZ Voice FX - AI noise reduction (RNNoise neural network, runs locally on CPU)
//
// RNNoise works on 480-sample frames at 48 kHz. For other host rates the signal is
// resampled to 48 kHz and back. Dry and wet are mixed in the 48 kHz domain with the
// dry signal delayed by exactly one frame (the network's algorithmic delay), so the
// "Amount" knob never causes phasing.
#pragma once

#include "Common.h"
#include <cstdio>
#include <rnnoise.h>

namespace ez
{
//==============================================================================
// Streaming cubic-Hermite resampler: push one input sample, it emits 0..n outputs.
class StreamResampler
{
public:
    void prepare (double inRate, double outRate)
    {
        step = inRate / outRate;
        reset();
    }

    void reset() noexcept
    {
        h[0] = h[1] = h[2] = h[3] = 0.0f;
        pos = 0.0;
    }

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
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    float h[4] {};
    double pos = 0.0, step = 1.0;
};

//==============================================================================
class DenoiseChannel
{
public:
    static constexpr int kFrame = 480;           // RNNoise frame (10 ms @ 48 kHz)
    static constexpr int kNetworkDelay = 480;    // RNNoise output lags input by one frame

    DenoiseChannel() = default;
    ~DenoiseChannel() { releaseState(); }
    DenoiseChannel (const DenoiseChannel&) = delete;
    DenoiseChannel& operator= (const DenoiseChannel&) = delete;

    void prepare (double hostRate)
    {
        releaseState();
        st = rnnoise_create (nullptr);   // built-in model, no files needed

        resample = std::abs (hostRate - 48000.0) > 0.5;
        ratio = hostRate / 48000.0;
        if (resample)
        {
            up.prepare (hostRate, 48000.0);
            down.prepare (48000.0, hostRate);
        }

        prefill = resample ? (int) std::ceil (kFrame * ratio) + 8 : kFrame;
        latency = resample ? prefill + (int) std::lround (kNetworkDelay * ratio + 2.0 + 2.0 * ratio)
                           : prefill + kNetworkDelay;

        const int fifoLen = nextPow2 (prefill * 2 + (int) std::ceil (kFrame * ratio) * 2 + 64);
        fifo.assign ((size_t) fifoLen, 0.0f);
        fifoMask = fifoLen - 1;
        fifoRead = 0;
        fifoWrite = prefill;   // pre-filled with silence
        fifoCount = prefill;

        std::fill (std::begin (inBuf), std::end (inBuf), 0.0f);
        std::fill (std::begin (prevIn), std::end (prevIn), 0.0f);
        inCount = 0;
        mixCur = mixTarget = 0.0f;
        vad.store (0.0f);
    }

    int getLatency() const noexcept { return latency; }
    void setMix (float m) noexcept { mixTarget = std::clamp (m, 0.0f, 1.0f); }
    float getVad() const noexcept { return vad.load(); }

    inline float processSample (float x) noexcept
    {
        if (resample) up.push (x, [this] (float s) { feed48 (s); });
        else          feed48 (x);
        return pop();
    }

private:
    void releaseState()
    {
        if (st != nullptr) { rnnoise_destroy (st); st = nullptr; }
    }

    inline void feed48 (float s) noexcept
    {
        inBuf[inCount++] = s;
        if (inCount == kFrame)
        {
            inCount = 0;
            processFrame();
        }
    }

    void processFrame() noexcept
    {
        const float m0 = mixCur, m1 = mixTarget;
        mixCur = m1;

        if (st != nullptr)
        {
            for (int i = 0; i < kFrame; ++i)
                scaled[i] = std::clamp (inBuf[i], -1.5f, 1.5f) * 32768.0f;

            const float v = rnnoise_process_frame (st, netOut, scaled);
            vad.store (v);

            constexpr float inv = 1.0f / 32768.0f;
            for (int i = 0; i < kFrame; ++i)
            {
                float wet = netOut[i] * inv;
                if (! std::isfinite (wet)) wet = 0.0f;
                const float dry = prevIn[i];                     // aligned with network delay
                const float m = m0 + (m1 - m0) * (float) (i + 1) / (float) kFrame;
                emit48 (dry + m * (wet - dry));
            }
        }
        else
        {
            for (int i = 0; i < kFrame; ++i) emit48 (prevIn[i]);
        }

        std::copy (std::begin (inBuf), std::end (inBuf), std::begin (prevIn));
    }

    inline void emit48 (float y) noexcept
    {
        if (resample) down.push (y, [this] (float z) { pushFifo (z); });
        else          pushFifo (y);
    }

    inline void pushFifo (float z) noexcept
    {
        if (fifoCount >= (int) fifo.size()) return;   // should never happen
        fifo[(size_t) fifoWrite] = z;
        fifoWrite = (fifoWrite + 1) & fifoMask;
        ++fifoCount;
    }

    inline float pop() noexcept
    {
        if (fifoCount <= 0) return 0.0f;                // underrun guard
        const float v = fifo[(size_t) fifoRead];
        fifoRead = (fifoRead + 1) & fifoMask;
        --fifoCount;
        return v;
    }

    DenoiseState* st = nullptr;
    bool resample = false;
    double ratio = 1.0;
    StreamResampler up, down;

    float inBuf[kFrame] {}, prevIn[kFrame] {}, scaled[kFrame] {}, netOut[kFrame] {};
    int inCount = 0;

    std::vector<float> fifo;
    int fifoMask = 0, fifoRead = 0, fifoWrite = 0, fifoCount = 0;
    int prefill = kFrame, latency = 2 * kFrame;

    float mixCur = 0.0f, mixTarget = 0.0f;
    std::atomic<float> vad { 0.0f };
};
} // namespace ez
