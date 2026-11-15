// EZ Voice FX - common DSP helpers
#pragma once

#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <complex>
#include <atomic>
#include <iterator>

namespace ez
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

inline float gainToDb (float g, float floorDb = -120.0f) noexcept
{
    return g > 1.0e-9f ? std::max (floorDb, 20.0f * std::log10 (g)) : floorDb;
}

inline float lerpf (float a, float b, float t) noexcept { return a + (b - a) * t; }

// exp(-1 / (time * fs)) : classic one-pole coefficient
inline float timeCoef (double fs, double ms) noexcept
{
    if (ms <= 0.0 || fs <= 0.0) return 0.0f;
    return (float) std::exp (-1.0 / (ms * 0.001 * fs));
}

// 1 - exp(...) : smoothing "alpha" used as  y += alpha * (x - y)
inline float smoothAlpha (double fs, double seconds) noexcept
{
    if (seconds <= 0.0 || fs <= 0.0) return 1.0f;
    return (float) (1.0 - std::exp (-1.0 / (seconds * fs)));
}

inline int nextPow2 (int v) noexcept
{
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

//==============================================================================
struct EnvFollower
{
    float att = 0.0f, rel = 0.0f, v = 0.0f;

    void set (double fs, double attackMs, double releaseMs)
    {
        att = timeCoef (fs, attackMs);
        rel = timeCoef (fs, releaseMs);
    }
    void reset() noexcept { v = 0.0f; }

    inline float process (float x) noexcept
    {
        const float c = x > v ? att : rel;
        v = x + c * (v - x);
        return v;
    }
};

//==============================================================================
// Transposed direct form II biquad (double state for stability at low cutoffs)
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    void reset() noexcept { z1 = z2 = 0.0; }

    void setCoefficients (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        const double inv = 1.0 / na0;
        b0 = nb0 * inv; b1 = nb1 * inv; b2 = nb2 * inv;
        a1 = na1 * inv; a2 = na2 * inv;
    }

    void setHighpass (double fs, double freq, double q) noexcept
    {
        freq = std::clamp (freq, 5.0, 0.45 * fs);
        const double w0 = 2.0 * kPi * freq / fs;
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        setCoefficients ((1.0 + cs) * 0.5, -(1.0 + cs), (1.0 + cs) * 0.5,
                         1.0 + alpha, -2.0 * cs, 1.0 - alpha);
    }

    void setLowpass (double fs, double freq, double q) noexcept
    {
        freq = std::clamp (freq, 5.0, 0.45 * fs);
        const double w0 = 2.0 * kPi * freq / fs;
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        setCoefficients ((1.0 - cs) * 0.5, 1.0 - cs, (1.0 - cs) * 0.5,
                         1.0 + alpha, -2.0 * cs, 1.0 - alpha);
    }

    // constant 0 dB peak band-pass (x - bandpass = notch)
    void setBandpass (double fs, double freq, double q) noexcept
    {
        freq = std::clamp (freq, 5.0, 0.45 * fs);
        const double w0 = 2.0 * kPi * freq / fs;
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        setCoefficients (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * cs, 1.0 - alpha);
    }

    inline float process (float xin) noexcept
    {
        const double x = xin;
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
};

//==============================================================================
// Linkwitz-Riley 4th order crossover: low + high sums to an allpass (flat magnitude)
struct Crossover
{
    Biquad lp1, lp2, hp1, hp2;

    void set (double fs, double freq)
    {
        constexpr double q = 0.70710678118654752;
        lp1.setLowpass (fs, freq, q);  lp2.setLowpass (fs, freq, q);
        hp1.setHighpass (fs, freq, q); hp2.setHighpass (fs, freq, q);
    }
    void reset() noexcept { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }

    inline void process (float x, float& lo, float& hi) noexcept
    {
        lo = lp2.process (lp1.process (x));
        hi = hp2.process (hp1.process (x));
    }
};

//==============================================================================
// Simple integer delay line
struct DelayLine
{
    std::vector<float> buf;
    int idx = 0;

    void prepare (int delaySamples)
    {
        buf.assign ((size_t) std::max (0, delaySamples), 0.0f);
        idx = 0;
    }

    inline float process (float x) noexcept
    {
        if (buf.empty()) return x;
        const float y = buf[(size_t) idx];
        buf[(size_t) idx] = x;
        if (++idx >= (int) buf.size()) idx = 0;
        return y;
    }
};

//==============================================================================
// Delay line whose delay can change at run time (0 .. size-1 samples)
struct VarDelay
{
    std::vector<float> buf;
    int w = 0, mask = 0;

    void prepare (int maxDelay)
    {
        const int size = nextPow2 (std::max (2, maxDelay + 1));
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }

    inline float process (float x, int delay) noexcept
    {
        buf[(size_t) w] = x;
        const float y = buf[(size_t) ((w - delay) & mask)];
        w = (w + 1) & mask;
        return y;
    }
};

//==============================================================================
// Two cascaded Butterworth high-pass sections (24 dB/oct rumble filter)
struct LowCut
{
    Biquad s1, s2;
    double fs = 48000.0;
    float currentFreq = -1.0f;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        s1.reset(); s2.reset();
        currentFreq = -1.0f;
        setFrequency (80.0f);
    }

    void setFrequency (float f)
    {
        if (std::abs (f - currentFreq) < 0.01f) return;
        currentFreq = f;
        constexpr double q = 0.70710678118654752;
        s1.setHighpass (fs, f, q);
        s2.setHighpass (fs, f, q);
    }

    inline float process (float x) noexcept { return s2.process (s1.process (x)); }
};

} // namespace ez
