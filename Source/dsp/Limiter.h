// EZ Voice FX - look-ahead true-peak limiter (linked stereo)
//
// Peak detection includes inter-sample peaks estimated with an 8x windowed-sinc
// interpolator (ITU-R BS.1770 style true peak). Gain = moving average (length L) of
// a sliding minimum (length L) of the required gain, audio delayed accordingly:
// smooth gain curve and no overs.
#pragma once

#include "Common.h"

namespace ez
{
class Limiter
{
public:
    static constexpr int kTaps = 16;
    static constexpr int kPhases = 7;    // 8x oversampled detection
    static constexpr int kTpDelay = 7;   // detector centre: hist[7]

    void prepare (double fs)
    {
        L = std::max (1, (int) std::lround (0.0015 * fs));
        relAlpha = smoothAlpha (fs, 0.08);

        // fractional-delay interpolation kernels for phases 1/8 .. 7/8
        for (int p = 0; p < kPhases; ++p)
        {
            const double frac = (double) (p + 1) / (double) (kPhases + 1);
            double sum = 0.0;
            for (int j = 0; j < kTaps; ++j)
            {
                const double xx = (double) j - (double) kTpDelay - frac;
                const double sinc = std::abs (xx) < 1.0e-9 ? 1.0 : std::sin (kPi * xx) / (kPi * xx);
                const double w = 0.5 + 0.5 * std::cos (kPi * xx / (kTaps * 0.5 + 0.5));    // Hann window
                kern[p][j] = (float) (sinc * w);
                sum += kern[p][j];
            }
            for (int j = 0; j < kTaps; ++j) kern[p][j] = (float) (kern[p][j] / sum);
        }
        for (auto& h : hist) for (auto& v : h) v = 0.0f;

        cap = L + 2;
        dqVal.assign ((size_t) cap, 1.0f);
        dqTime.assign ((size_t) cap, 0);
        dqHead = 0; dqCount = 0;

        box.assign ((size_t) L, 1.0f);
        boxSum = (double) L;
        boxIdx = 0;

        for (auto& d : delay) d.prepare (L - 1);
        r = 1.0f;
        now = 0;
        lastGain = 1.0f;
    }

    int getLatency() const noexcept { return kTpDelay + L - 1; }
    float getGrDb() const noexcept { return -gainToDb (lastGain); }

    void process (float* const* ch, int numCh, int n, float ceilingDb, bool enabled) noexcept
    {
        const float ceil = dbToGain (ceilingDb);
        const float detCeil = ceil * 0.983f;     // 0.15 dB safety margin for gain modulation
        float minG = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            float centre[2] = { 0.0f, 0.0f };

            for (int c = 0; c < numCh; ++c)
            {
                float* h = hist[c];
                for (int j = 0; j < kTaps - 1; ++j) h[j] = h[j + 1];
                h[kTaps - 1] = ch[c][i];
                centre[c] = h[kTpDelay];

                float pk = std::abs (h[kTpDelay]);
                for (int p = 0; p < kPhases; ++p)
                {
                    float v = 0.0f;
                    for (int j = 0; j < kTaps; ++j) v += kern[p][j] * h[j];
                    pk = std::max (pk, std::abs (v));
                }
                peak = std::max (peak, pk);
            }

            const float t = (enabled && peak > detCeil) ? detCeil / peak : 1.0f;

            while (dqCount > 0 && dqVal[(size_t) back()] >= t) --dqCount;
            const int bi = (dqHead + dqCount) % cap;
            dqVal[(size_t) bi] = t; dqTime[(size_t) bi] = now; ++dqCount;
            while (dqTime[(size_t) dqHead] <= now - L) { dqHead = (dqHead + 1) % cap; --dqCount; }
            const float hmin = dqVal[(size_t) dqHead];

            r = std::min (hmin, r + (1.0f - r) * relAlpha);

            boxSum += (double) r - (double) box[(size_t) boxIdx];
            box[(size_t) boxIdx] = r;
            if (++boxIdx >= L) boxIdx = 0;
            const float g = std::min (1.0f, (float) (boxSum / (double) L));
            minG = std::min (minG, g);

            for (int c = 0; c < numCh; ++c)
            {
                float y = delay[c].process (centre[c]) * g;
                if (enabled) y = std::clamp (y, -ceil, ceil);
                ch[c][i] = y;
            }
            ++now;
        }
        lastGain = minG;
    }

private:
    int back() const noexcept { return (dqHead + dqCount - 1) % cap; }

    int L = 72, cap = 74;
    float kern[kPhases][kTaps] {};
    float hist[2][kTaps] {};
    float relAlpha = 0.001f, r = 1.0f, lastGain = 1.0f;
    std::vector<float> dqVal;
    std::vector<int64_t> dqTime;
    int dqHead = 0, dqCount = 0;
    std::vector<float> box;
    double boxSum = 72.0;
    int boxIdx = 0;
    DelayLine delay[2];
    int64_t now = 0;
};
} // namespace ez
