// EZ Voice FX - de-reverb (late reverberation suppression, STFT based)
//
// Late reverb power is predicted from the signal's own spectrum ~50 ms earlier,
// decayed with an exponential room model (Lebart method). That estimate is then
// subtracted spectrally with temporal + frequency smoothing to avoid "musical noise".
#pragma once

#include "FFT.h"

namespace ez
{
class DereverbChannel
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        N = fs <= 50000.0 ? 1024 : (fs <= 100000.0 ? 2048 : 4096);
        hop = N / 4;
        mask = N - 1;
        bins = N / 2 + 1;
        fft.prepare (N);

        win.resize ((size_t) N);
        for (int i = 0; i < N; ++i)
            win[(size_t) i] = (float) std::sqrt (0.5 - 0.5 * std::cos (2.0 * kPi * i / N)); // sqrt-Hann

        inRing.assign ((size_t) N, 0.0f);
        outRing.assign ((size_t) N, 0.0f);
        frame.assign ((size_t) N, std::complex<float> (0.0f, 0.0f));
        psd.assign ((size_t) bins, 0.0f);
        gSm.assign ((size_t) bins, 1.0f);
        gTmp.assign ((size_t) bins, 1.0f);

        delayFrames = std::max (1, (int) std::lround (0.05 * fs / hop));
        slots = delayFrames + 1;
        hist.assign ((size_t) (slots * bins), 0.0f);
        histPos = 0;

        pos = 0; hopCount = 0;
        amount = target = 0.0f;
    }

    int getLatency() const noexcept { return N; }
    void setAmount (float a) noexcept { target = std::clamp (a, 0.0f, 1.0f); }

    inline float processSample (float x) noexcept
    {
        inRing[(size_t) pos] = x;
        const float y = outRing[(size_t) pos];
        outRing[(size_t) pos] = 0.0f;
        pos = (pos + 1) & mask;

        if (++hopCount >= hop)
        {
            hopCount = 0;
            processFrame();
        }
        return y;
    }

private:
    void processFrame() noexcept
    {
        amount += 0.3f * (target - amount);
        const float a = amount;

        // oldest .. newest sample, windowed
        for (int n = 0; n < N; ++n)
            frame[(size_t) n] = std::complex<float> (inRing[(size_t) ((pos + n) & mask)] * win[(size_t) n], 0.0f);

        fft.forward (frame.data());

        float* cur = &hist[(size_t) (histPos * bins)];
        const float* old = &hist[(size_t) (((histPos + 1) % slots) * bins)];   // delayFrames ago

        const float rt60  = 0.4f + 0.6f * a;
        const float delta = 3.0f * std::log (10.0f) / rt60;
        const float td    = (float) (delayFrames * hop / fs);
        const float decay = std::exp (-2.0f * delta * td);
        const float beta  = 2.0f * a;
        const float floorG = dbToGain (-18.0f * a);

        for (int k = 0; k < bins; ++k)
        {
            const float P = std::norm (frame[(size_t) k]);
            psd[(size_t) k] = 0.6f * psd[(size_t) k] + 0.4f * P;
            const float R = decay * old[k];
            float G = 1.0f - beta * R / (psd[(size_t) k] + 1.0e-12f);
            G = std::clamp (G, floorG, 1.0f);
            gSm[(size_t) k] = 0.5f * gSm[(size_t) k] + 0.5f * G;
            cur[k] = psd[(size_t) k];
        }
        histPos = (histPos + 1) % slots;

        // light smoothing across frequency
        gTmp[0] = gSm[0];
        gTmp[(size_t) (bins - 1)] = gSm[(size_t) (bins - 1)];
        for (int k = 1; k < bins - 1; ++k)
            gTmp[(size_t) k] = 0.25f * gSm[(size_t) (k - 1)] + 0.5f * gSm[(size_t) k] + 0.25f * gSm[(size_t) (k + 1)];

        frame[0] *= gTmp[0];
        frame[(size_t) (N / 2)] *= gTmp[(size_t) (N / 2)];
        for (int k = 1; k < N / 2; ++k)
        {
            frame[(size_t) k] *= gTmp[(size_t) k];
            frame[(size_t) (N - k)] *= gTmp[(size_t) k];   // keep conjugate symmetry
        }

        fft.inverse (frame.data());

        // overlap-add: sqrt-Hann analysis * synthesis at 75 % overlap sums to 2.0
        for (int n = 0; n < N; ++n)
            outRing[(size_t) ((pos + n) & mask)] += frame[(size_t) n].real() * win[(size_t) n] * 0.5f;
    }

    double fs = 48000.0;
    int N = 1024, hop = 256, mask = 1023, bins = 513;
    FFT fft;
    std::vector<float> win, inRing, outRing, psd, gSm, gTmp, hist;
    std::vector<std::complex<float>> frame;
    int delayFrames = 9, slots = 10, histPos = 0;
    int pos = 0, hopCount = 0;
    float amount = 0.0f, target = 0.0f;
};
} // namespace ez
