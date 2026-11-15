// EZ Voice FX - reference matching on the vocal: tone profiler, Match EQ, beat-relative balance
#pragma once

#include "Loudness.h"
#include "FFT.h"
#include "../ref/Reference.h"
#include <array>

namespace ez
{
//==============================================================================
// Gated long-term loudness (LUFS), ~3 s memory, ignores pauses. Linked channels.
class LoudTracker
{
public:
    void prepare (double fs)
    {
        for (auto& k : kw) k.prepare (fs);
        momAlpha = smoothAlpha (fs, 0.4);
        longAlpha = smoothAlpha (fs, 3.0);
        M = S = 0.0; init = false; active = false;
    }

    void process (const float* const* ch, int numCh, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            double z = 0.0;
            for (int c = 0; c < numCh; ++c) { const float k = kw[c].process (ch[c][i]); z += (double) k * k; }
            M += momAlpha * (z - M);
            const float lm = msToLufs (M), ls = init ? msToLufs (S) : -70.0f;
            active = lm > -50.0f && (! init || lm > ls - 20.0f);
            if (active)
            {
                if (! init) { S = std::max (M, 1.0e-10); init = true; }
                else S += longAlpha * (z - S);
            }
        }
    }

    bool hasValue() const noexcept { return init; }
    bool isActive() const noexcept { return active; }
    float getLufs() const noexcept { return init ? msToLufs (S) : -70.0f; }

private:
    KWeighting kw[2];
    double momAlpha = 0.0, longAlpha = 0.0, M = 0.0, S = 0.0;
    bool init = false, active = false;
};

//==============================================================================
// Long-term tone of the vocal in the 10 match bands (only while the voice is present)
class VocalProfiler
{
public:
    static constexpr int kN = 4096;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        fft.prepare (kN);
        ring.assign ((size_t) kN, 0.0f);
        buf.assign ((size_t) kN, std::complex<float> (0.0f, 0.0f));
        win.resize ((size_t) kN);
        for (int i = 0; i < kN; ++i) win[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / kN));
        pos = 0; count = 0; frames = 0;
        for (auto& b : avg) b = 0.0;
        for (int b = 1; b < kN / 2; ++b)
        {
            const float f = (float) (b * fs / kN);
            bandOf[b] = -1;
            for (int k = 0; k < kMatchBands; ++k)
                if (f >= kMatchFreqs[k] / 1.33f && f < kMatchFreqs[k] * 1.33f) { bandOf[b] = k; break; }
        }
    }

    // mono input, voice flag from the neural VAD
    void process (const float* x, int n, bool voice) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) pos] = x[i];
            pos = (pos + 1) & (kN - 1);
            if (++count >= kN / 2)
            {
                count = 0;
                if (voice) analyseFrame();
            }
        }
    }

    bool hasProfile() const noexcept { return frames > 40; }

    // dB per band normalised to the 400 Hz - 8 kHz average (same as ReferenceProfile)
    void getProfile (float* out) const noexcept
    {
        double mean = 0.0;
        float db[kMatchBands];
        for (int b = 0; b < kMatchBands; ++b)
        {
            db[b] = (float) (10.0 * std::log10 (avg[b] + 1.0e-20));
            if (b >= 2 && b <= 7) mean += db[b];
        }
        mean /= 6.0;
        for (int b = 0; b < kMatchBands; ++b) out[b] = db[b] - (float) mean;
    }

private:
    void analyseFrame() noexcept
    {
        for (int i = 0; i < kN; ++i)
            buf[(size_t) i] = std::complex<float> (ring[(size_t) ((pos + i) & (kN - 1))] * win[(size_t) i], 0.0f);
        fft.forward (buf.data());
        double e[kMatchBands] {};
        for (int b = 1; b < kN / 2; ++b) if (bandOf[b] >= 0) e[bandOf[b]] += std::norm (buf[(size_t) b]);
        const double a = frames < 100 ? 1.0 / (frames + 1) : 0.01;     // ~8 s memory after warm-up
        for (int k = 0; k < kMatchBands; ++k) avg[k] += a * (e[k] - avg[k]);
        ++frames;
    }

    double fs = 48000.0;
    FFT fft;
    std::vector<float> ring, win;
    std::vector<std::complex<float>> buf;
    int pos = 0, count = 0, frames = 0;
    int bandOf[kN / 2] {};
    double avg[kMatchBands] {};
};

//==============================================================================
// Match EQ: 10 peaking filters moving the vocal tone towards the reference
class MatchEq
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& c : chans) for (auto& f : c) f.reset();
        for (int k = 0; k < kMatchBands; ++k) { gains[k] = 0.0f; applied[k] = 99.0f; }
        update();
    }

    void computeTargets (float amount, bool haveRef, const float* ref, bool haveOwn, const float* own) noexcept
    {
        for (int k = 0; k < kMatchBands; ++k)
        {
            float t = 0.0f;
            if (amount > 0.001f && haveRef && haveOwn)
                t = juce::jlimit (-8.0f, 8.0f, (ref[k] - own[k]) * amount);
            gains[k] += 0.02f * (t - gains[k]);     // very slow glide, no zipper or pumping
        }
        update();
    }

    float getGain (int k) const noexcept { return gains[k]; }

    inline float processSample (int c, float x) noexcept
    {
        for (auto& f : chans[c]) x = f.process (x);
        return x;
    }

private:
    void update() noexcept
    {
        bool changed = false;
        for (int k = 0; k < kMatchBands; ++k) if (std::abs (gains[k] - applied[k]) > 0.05f) changed = true;
        if (! changed) return;
        for (auto& c : chans)
            for (int k = 0; k < kMatchBands; ++k)
            {
                const double f = std::min ((double) kMatchFreqs[k], fs * 0.42);
                c[(size_t) k].setPeaking (fs, f, k >= 8 ? 2.0 : 1.4, gains[k] * 0.8);   // overlap compensation
            }
        for (int k = 0; k < kMatchBands; ++k) applied[k] = gains[k];
    }

    double fs = 48000.0;
    std::array<Biquad, kMatchBands> chans[2];
    float gains[kMatchBands] {}, applied[kMatchBands] {};
};

//==============================================================================
// Keeps the vocal at "beat loudness + offset" (measured after all vocal processing)
class BalanceTrim
{
public:
    void prepare (double fs)
    {
        out.prepare (fs);
        alpha = smoothAlpha (fs, 2.0);
        trimDb = 0.0f;
    }

    void process (float* const* ch, int numCh, int n, bool enabled, bool beatPresent, float beatLufs, float offsetDb) noexcept
    {
        out.process (ch, numCh, n);                          // measured before the trim
        float target = 0.0f;
        if (enabled && beatPresent && out.hasValue())
            target = juce::jlimit (-15.0f, 15.0f, (beatLufs + offsetDb) - out.getLufs());
        else if (! enabled)
            target = 0.0f;
        else
            target = trimDb;                                   // no beat: hold

        const float a = (enabled && ! out.isActive()) ? 0.0f : alpha;  // hold in pauses
        for (int i = 0; i < n; ++i)
        {
            trimDb += a * (target - trimDb);
            const float g = dbToGain (trimDb);
            for (int c = 0; c < numCh; ++c) ch[c][i] *= g;
        }
    }

    float getTrimDb() const noexcept { return trimDb; }
    float getVocalLufs() const noexcept { return out.getLufs(); }

private:
    LoudTracker out;
    float alpha = 0.0001f, trimDb = 0.0f;
};
} // namespace ez
