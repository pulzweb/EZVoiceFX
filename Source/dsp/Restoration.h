// EZ Voice FX - hum remover (50/60 Hz + harmonics) and voice-aware gate
#pragma once

#include "Common.h"

namespace ez
{
//==============================================================================
// Mains hum: subtracts narrow band-passes at the fundamental and 7 harmonics
// (x - bandpass = exact notch). Auto mode decides 50 or 60 Hz by itself.
class HumRemover
{
public:
    static constexpr int kHarmonics = 8;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        det50.setBandpass (fs, 50.0, 20.0); det60.setBandpass (fs, 60.0, 20.0);
        det50.reset(); det60.reset();
        e50 = e60 = 0.0f;
        detAlpha = smoothAlpha (fs, 1.0);
        decided = 50.0;
        votes = 0;
        voteLen = (int) std::lround (fs * 1.5);
        base = -1.0;
        setBase (50.0);
        amt = target = 0.0f;
        amtAlpha = smoothAlpha (fs, 0.05);
    }

    // mode: 0 = auto, 1 = 50 Hz, 2 = 60 Hz
    void setParams (float amount, int newMode) noexcept
    {
        target = std::clamp (amount, 0.0f, 1.0f);
        mode = newMode;
    }

    double getBase() const noexcept { return base; }

    // detection is driven by one channel, the filters by all
    inline float processSample (float x, bool detect) noexcept
    {
        amt += amtAlpha * (target - amt);

        if (detect)
        {
            const float a = det50.process (x), b = det60.process (x);
            e50 += detAlpha * (a * a - e50);
            e60 += detAlpha * (b * b - e60);
            if (++votes >= voteLen)
            {
                votes = 0;
                if (e50 > 2.0f * e60) decided = 50.0;
                else if (e60 > 2.0f * e50) decided = 60.0;
            }
        }

        const double want = mode == 1 ? 50.0 : (mode == 2 ? 60.0 : decided);
        if (want != base) setBase (want);

        if (amt < 0.001f) { for (int k = 0; k < kHarmonics; ++k) bp[k].process (x); return x; }

        float sum = 0.0f;
        for (int k = 0; k < active; ++k) sum += bp[k].process (x);
        return x - amt * sum;
    }

private:
    void setBase (double f)
    {
        base = f;
        active = 0;
        for (int k = 0; k < kHarmonics; ++k)
        {
            const double fk = f * (k + 1);
            if (fk < std::min (0.45 * fs, 1000.0))
            {
                bp[k].setBandpass (fs, fk, 18.0 + 4.0 * k);   // narrower at higher harmonics
                bp[k].reset();
                active = k + 1;
            }
        }
    }

    double fs = 48000.0, base = 50.0, decided = 50.0;
    Biquad bp[kHarmonics], det50, det60;
    int active = 0, mode = 0, votes = 0, voteLen = 48000;
    float e50 = 0.0f, e60 = 0.0f, detAlpha = 0.001f;
    float amt = 0.0f, target = 0.0f, amtAlpha = 0.001f;
};

//==============================================================================
// Gate / expander for the pauses between phrases. Opens on the neural voice
// detector OR on level close to the recent voice peak. Linked across channels.
class VoiceGate
{
public:
    void prepare (double fs)
    {
        env.set (fs, 1.0, 60.0); env.reset();
        vpDecay = (float) std::exp (-1.0 / (3.0 * fs));
        gAtt = timeCoef (fs, 3.0);     // opening
        gRel = timeCoef (fs, 150.0);   // closing
        holdLen = (int) std::lround (0.12 * fs);
        hold = 0; g = 1.0f; vp = 0.0f;
        amt = target = 0.0f;
        amtAlpha = smoothAlpha (fs, 0.05);
    }

    void setAmount (float a) noexcept { target = std::clamp (a, 0.0f, 1.0f); }
    void setVad (float v) noexcept { vad = v; }
    float getGain() const noexcept { return g; }

    void process (float* const* ch, int numCh, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            amt += amtAlpha * (target - amt);

            float peak = 0.0f;
            for (int c = 0; c < numCh; ++c) peak = std::max (peak, std::abs (ch[c][i]));
            const float e = env.process (peak);
            vp = std::max (e, vp * vpDecay);

            const bool open = vad > 0.45f || e > vp * 0.05f || vp < 1.0e-4f;
            if (open) hold = holdLen;
            else if (hold > 0) --hold;

            const float tg = (open || hold > 0) ? 1.0f : dbToGain (-30.0f * amt);
            const float c = tg > g ? gAtt : gRel;
            g = tg + c * (g - tg);

            for (int c2 = 0; c2 < numCh; ++c2) ch[c2][i] *= g;
        }
    }

private:
    EnvFollower env;
    float vpDecay = 0.9999f, vp = 0.0f, vad = 1.0f;
    float gAtt = 0.0f, gRel = 0.0f, g = 1.0f;
    int holdLen = 0, hold = 0;
    float amt = 0.0f, target = 0.0f, amtAlpha = 0.001f;
};
} // namespace ez
