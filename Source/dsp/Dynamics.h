// EZ Voice FX - de-plosive, de-esser and breath control
#pragma once

#include "Common.h"

namespace ez
{
//==============================================================================
// De-plosive: tames "P" / "B" air bursts below ~150 Hz relative to the upper band.
class DeplosiveChannel
{
public:
    void prepare (double fs)
    {
        xo.set (fs, 150.0);
        xo.reset();
        loEnv.set (fs, 1.0, 60.0);   loEnv.reset();
        hiEnv.set (fs, 3.0, 120.0);  hiEnv.reset();
        gAtt = timeCoef (fs, 0.3);
        gRel = timeCoef (fs, 50.0);
        g = 1.0f;
        amt = target = 0.0f;
        amtAlpha = smoothAlpha (fs, 0.03);
    }

    void setAmount (float a) noexcept { target = std::clamp (a, 0.0f, 1.0f); }
    float getGain() const noexcept { return g; }

    inline float processSample (float x) noexcept
    {
        amt += amtAlpha * (target - amt);

        float lo, hi;
        xo.process (x, lo, hi);
        const float le = loEnv.process (std::abs (lo));
        const float he = hiEnv.process (std::abs (hi));

        float tg = 1.0f;
        if (amt > 0.005f && le > 1.0e-4f)
        {
            const float allowed = he * dbToGain (lerpf (9.0f, 0.0f, amt)) + 1.0e-6f;
            if (le > allowed) tg = allowed / le;
            tg = std::max (tg, dbToGain (-20.0f * amt));
        }

        const float c = tg < g ? gAtt : gRel;
        g = tg + c * (g - tg);
        return hi + lo * g;
    }

private:
    Crossover xo;
    EnvFollower loEnv, hiEnv;
    float gAtt = 0.0f, gRel = 0.0f, g = 1.0f;
    float amt = 0.0f, target = 0.0f, amtAlpha = 0.001f;
};

//==============================================================================
// De-esser: split-band, reduces only the band above ~5.5 kHz when its share of the
// total level shows sibilance ("S", "SH", "CH").
class DeEsserChannel
{
public:
    void prepare (double fs)
    {
        xo.set (fs, std::min (5500.0, fs * 0.4));
        xo.reset();
        hiEnv.set (fs, 0.5, 40.0);   hiEnv.reset();
        fullEnv.set (fs, 0.5, 40.0); fullEnv.reset();
        gAtt = timeCoef (fs, 0.5);
        gRel = timeCoef (fs, 60.0);
        g = 1.0f;
        amt = target = 0.0f;
        amtAlpha = smoothAlpha (fs, 0.03);
    }

    void setAmount (float a) noexcept { target = std::clamp (a, 0.0f, 1.0f); }
    float getGain() const noexcept { return g; }

    inline float processSample (float x) noexcept
    {
        amt += amtAlpha * (target - amt);

        float lo, hi;
        xo.process (x, lo, hi);
        const float h = hiEnv.process (std::abs (hi));
        const float f = fullEnv.process (std::abs (x));

        float tg = 1.0f;
        if (amt > 0.005f && f > 1.0e-4f)
        {
            const float r = h / (f + 1.0e-9f);
            const float thr = lerpf (0.60f, 0.22f, amt);
            if (r > thr)
            {
                const float q = thr / r;
                tg = q * q;
            }
            tg = std::max (tg, dbToGain (-18.0f * amt));
        }

        const float c = tg < g ? gAtt : gRel;
        g = tg + c * (g - tg);
        return lo + hi * g;
    }

private:
    Crossover xo;
    EnvFollower hiEnv, fullEnv;
    float gAtt = 0.0f, gRel = 0.0f, g = 1.0f;
    float amt = 0.0f, target = 0.0f, amtAlpha = 0.001f;
};

//==============================================================================
// Breath control: lowers quiet, noisy (high zero-crossing) segments between phrases.
// Linked across channels.
class BreathControl
{
public:
    void prepare (double fs)
    {
        env.set (fs, 3.0, 40.0);
        env.reset();
        vpDecay = (float) std::exp (-1.0 / (2.5 * fs));
        zcAlpha = smoothAlpha (fs, 0.012);
        zcThr = (float) (2.0 * 2500.0 / fs);       // dominant energy above ~2.5 kHz
        gAtt = timeCoef (fs, 12.0);
        gRel = timeCoef (fs, 40.0);
        holdLen = (int) std::lround (0.015 * fs);
        hold = 0;
        g = 1.0f; vp = 0.0f; zcr = 0.0f; prevPos = true;
        amt = target = 0.0f;
        amtAlpha = smoothAlpha (fs, 0.03);
    }

    void setAmount (float a) noexcept { target = std::clamp (a, 0.0f, 1.0f); }
    float getGain() const noexcept { return g; }

    void process (float* const* ch, int numCh, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            amt += amtAlpha * (target - amt);

            float m = 0.0f;
            for (int c = 0; c < numCh; ++c) m += ch[c][i];
            m /= (float) numCh;

            const float e = env.process (std::abs (m));
            vp = std::max (e, vp * vpDecay);

            const bool pos = m >= 0.0f;
            zcr += zcAlpha * ((pos != prevPos ? 1.0f : 0.0f) - zcr);
            prevPos = pos;

            bool breath = amt > 0.01f && vp > 1.0e-3f && e > 1.0e-4f
                          && e < vp * 0.2f && zcr > zcThr;
            if (breath) hold = holdLen;
            else if (hold > 0) { --hold; breath = true; }

            const float tg = breath ? dbToGain (-20.0f * amt) : 1.0f;
            const float c = tg < g ? gAtt : gRel;
            g = tg + c * (g - tg);

            for (int c2 = 0; c2 < numCh; ++c2) ch[c2][i] *= g;
        }
    }

private:
    EnvFollower env;
    float vpDecay = 0.9999f, vp = 0.0f, zcAlpha = 0.01f, zcr = 0.0f, zcThr = 0.1f;
    bool prevPos = true;
    float gAtt = 0.0f, gRel = 0.0f, g = 1.0f;
    int holdLen = 0, hold = 0;
    float amt = 0.0f, target = 0.0f, amtAlpha = 0.001f;
};
} // namespace ez
