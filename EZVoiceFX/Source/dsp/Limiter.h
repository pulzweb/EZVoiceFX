// EZ Voice FX - look-ahead brickwall limiter (linked stereo)
//
// Gain = moving average (length L) of a sliding-minimum (length L) of the required
// gain, with the audio delayed by L-1 samples. This guarantees no sample exceeds the
// ceiling while keeping the gain curve smooth (no distortion clicks).
#pragma once

#include "Common.h"

namespace ez
{
class Limiter
{
public:
    void prepare (double fs)
    {
        L = std::max (1, (int) std::lround (0.0015 * fs));
        relAlpha = smoothAlpha (fs, 0.08);

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

    int getLatency() const noexcept { return L - 1; }
    float getGrDb() const noexcept { return -gainToDb (lastGain); }

    void process (float* const* ch, int numCh, int n, float ceilingDb, bool enabled) noexcept
    {
        const float ceil = dbToGain (ceilingDb);
        float minG = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;
            for (int c = 0; c < numCh; ++c) peak = std::max (peak, std::abs (ch[c][i]));

            const float t = (enabled && peak > ceil) ? ceil / peak : 1.0f;

            // sliding minimum over the last L samples (monotonic deque)
            while (dqCount > 0 && dqVal[(size_t) back()] >= t) --dqCount;
            const int bi = (dqHead + dqCount) % cap;
            dqVal[(size_t) bi] = t; dqTime[(size_t) bi] = now; ++dqCount;
            while (dqTime[(size_t) dqHead] <= now - L) { dqHead = (dqHead + 1) % cap; --dqCount; }
            const float h = dqVal[(size_t) dqHead];

            r = std::min (h, r + (1.0f - r) * relAlpha);

            boxSum += (double) r - (double) box[(size_t) boxIdx];
            box[(size_t) boxIdx] = r;
            if (++boxIdx >= L) boxIdx = 0;
            const float g = std::min (1.0f, (float) (boxSum / (double) L));
            minG = std::min (minG, g);

            for (int c = 0; c < numCh; ++c)
            {
                float y = delay[c].process (ch[c][i]) * g;
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
