// EZ Voice FX - VOX (one-knob vocal leveller/compressor), Fit to Beat EQ, Beat Unmask
#pragma once

#include "LinkHub.h"

namespace ez
{
//==============================================================================
// VOX: one knob like a "vocal rider + compressor". Threshold follows the vocal's own
// long-term level, ratio and depth grow with the knob, automatic make-up keeps the
// loudness steady. Linked across channels.
class VoxCompressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        rmsAlpha = smoothAlpha (fs, 0.008);
        ltUp = smoothAlpha (fs, 0.4);
        ltAlpha = smoothAlpha (fs, 3.0);
        warmLen = (int) std::lround (0.3 * fs);
        warm = 0;
        mkAlpha = smoothAlpha (fs, 3.0);
        gAtt = timeCoef (fs, 3.0);
        gRelFast = timeCoef (fs, 60.0);
        gRelSlow = timeCoef (fs, 250.0);
        ms = 0.0f; ltDb = -30.0f; ltInit = false; grDb = 0.0f; makeupDb = 0.0f;
        amt = target = 0.0f;
        amtAlpha = smoothAlpha (fs, 0.05);
    }

    void setAmount (float a) noexcept { target = std::clamp (a, 0.0f, 1.0f); }
    // false = Density (loudness kept), true = Loud like Renaissance Vox (make-up by peaks)
    void setLoud (bool l) noexcept { loud = l; }
    float getGrDb() const noexcept { return grDb; }

    static float reduction (float levelDb, float thr, float ratio) noexcept
    {
        constexpr float knee = 6.0f;
        const float over = levelDb - thr;
        if (over > knee * 0.5f) return over * (1.0f - 1.0f / ratio);
        if (over > -knee * 0.5f)
        {
            const float k = over + knee * 0.5f;
            return (1.0f - 1.0f / ratio) * k * k / (2.0f * knee);
        }
        return 0.0f;
    }

    void process (float* const* ch, int numCh, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            amt += amtAlpha * (target - amt);

            float sq = 0.0f;
            for (int c = 0; c < numCh; ++c) sq = std::max (sq, ch[c][i] * ch[c][i]);
            ms += rmsAlpha * (sq - ms);
            const float eDb = 10.0f * std::log10 (ms + 1.0e-12f);

            // long-term vocal level (only while the voice is present): rises fast, falls slowly
            if (eDb > -50.0f && (! ltInit || eDb > ltDb - 20.0f))
            {
                if (! ltInit) { ltDb = eDb; ltInit = true; warm = 0; }
                else if (warm < warmLen) { ltDb = std::max (ltDb, eDb); ++warm; }   // first 300 ms: follow peaks
                else ltDb += (eDb > ltDb ? ltUp : ltAlpha) * (eDb - ltDb);
            }

            float want = 0.0f;
            const float thr = ltDb + 4.0f - 14.0f * amt;
            const float ratio = 1.5f + 4.5f * amt;
            const bool active = amt > 0.005f && ltInit;
            if (active) want = std::min (reduction (eDb, thr, ratio), 18.0f);

            const float c = want > grDb ? gAtt : (grDb > 6.0f ? gRelSlow : gRelFast);
            grDb = want + c * (grDb - want);

            if (loud)
            {
                // RVox style: loud phrases keep their level, everything quieter comes up -> louder + denser
                const float mkTarget = active ? std::min (reduction (ltDb + 6.0f, thr, ratio), 18.0f) + 4.0f * amt : 0.0f;
                makeupDb += mkAlpha * (mkTarget - makeupDb);
            }
            else if (eDb > -50.0f)
            {
                makeupDb += mkAlpha * (grDb - makeupDb);     // density: average loudness kept
            }

            const float g = dbToGain (makeupDb - grDb);
            for (int c2 = 0; c2 < numCh; ++c2) ch[c2][i] *= g;
        }
    }

private:
    double fs = 48000.0;
    float rmsAlpha = 0.01f, ltAlpha = 0.0001f, ltUp = 0.001f, mkAlpha = 0.0001f;
    int warm = 0, warmLen = 14400;
    float gAtt = 0.0f, gRelFast = 0.0f, gRelSlow = 0.0f;
    float ms = 0.0f, ltDb = -30.0f, grDb = 0.0f, makeupDb = 0.0f;
    bool ltInit = false, loud = false;
    float amt = 0.0f, target = 0.0f, amtAlpha = 0.001f;
};

//==============================================================================
// Fit to Beat: shapes the vocal so it sits in the beat.
//   - cuts vocal low end / low-mids where the beat is dense (kick, bass, pads)
//   - lifts presence where the beat crowds 2-5 kHz, adds a little air
// Without a Beat Link a gentle "polish" curve is used.
class FitEq
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& c : chans) for (auto& f : c) f.reset();
        for (auto& g : gains) g = 0.0f;
        for (auto& g : applied) g = 99.0f;
        update();
    }

    // call once per block (message-free: reads the hub)
    void computeTargets (float amount, bool beatPresent, const float* beat, const float* vocal) noexcept
    {
        float t[4] = { 0.0f, 0.0f, 0.0f, 0.0f };   // low shelf 150, peak 320, peak 3k, high shelf 10k
        if (amount > 0.001f)
        {
            if (beatPresent)
            {
                float total = 1.0e-9f;
                for (int b = 0; b < LinkHub::kBands; ++b) total += beat[b] * beat[b];
                auto share = [&] (int b0, int b1)
                {
                    float e = 0.0f;
                    for (int b = b0; b <= b1; ++b) e += beat[b] * beat[b];
                    return e / total;
                };
                const float low = share (0, 1), lowMid = share (2, 3), pres = share (5, 6);
                const float vocalMud = vocal[2] / (vocal[4] + vocal[5] + 1.0e-6f);
                t[0] = -4.0f * amount * std::clamp (low * 1.6f, 0.0f, 1.0f);
                t[1] = -4.0f * amount * std::clamp (lowMid * 3.0f + vocalMud * 0.3f, 0.0f, 1.0f);
                t[2] =  3.5f * amount * std::clamp (pres * 5.0f, 0.3f, 1.0f);
                t[3] =  2.5f * amount;
            }
            else
            {
                t[0] = -1.5f * amount; t[1] = -1.5f * amount; t[2] = 1.5f * amount; t[3] = 2.0f * amount;
            }
        }
        for (int k = 0; k < 4; ++k) gains[k] += 0.08f * (t[k] - gains[k]);   // ~0.5 s glide
        update();
    }

    float getGain (int k) const noexcept { return gains[k]; }

    inline float processSample (int c, float x) noexcept
    {
        auto& f = chans[c];
        return f[3].process (f[2].process (f[1].process (f[0].process (x))));
    }

private:
    void update() noexcept
    {
        bool changed = false;
        for (int k = 0; k < 4; ++k) if (std::abs (gains[k] - applied[k]) > 0.05f) changed = true;
        if (! changed) return;
        for (auto& c : chans)
        {
            c[0].setLowShelf (fs, 150.0, gains[0]);
            c[1].setPeaking (fs, 320.0, 1.0, gains[1]);
            c[2].setPeaking (fs, 3000.0, 0.9, gains[2]);
            c[3].setHighShelf (fs, 10000.0, gains[3]);
        }
        for (int k = 0; k < 4; ++k) applied[k] = gains[k];
    }

    double fs = 48000.0;
    Biquad chans[2][4];
    float gains[4] {}, applied[4] {};
};

//==============================================================================
// Beat Unmask (runs on the beat instance): dips the beat where the vocal is active
// in the intelligibility range (600 Hz - 5 kHz).
class BeatUnmask
{
public:
    static constexpr int kN = 4;
    static constexpr float kFreqs[kN] = { 600.0f, 1200.0f, 2500.0f, 5000.0f };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& c : chans) for (auto& f : c) f.reset();
        for (int k = 0; k < kN; ++k) { gains[k] = 0.0f; applied[k] = 99.0f; }
        update();
    }

    void computeTargets (float amount, int vocals, const float* vocal, const float* beat) noexcept
    {
        for (int k = 0; k < kN; ++k)
        {
            float t = 0.0f;
            if (amount > 0.001f && vocals > 0)
            {
                const int b = k + 3;                                   // bands 600..5k
                const float ratio = vocal[b] / (beat[b] + 1.0e-6f);
                t = -6.0f * amount * std::clamp ((ratio - 0.15f) / 0.6f, 0.0f, 1.0f);
            }
            const float a = t < gains[k] ? 0.25f : 0.05f;               // dip fast, recover slow
            gains[k] += a * (t - gains[k]);
        }
        update();
    }

    float getGain (int k) const noexcept { return gains[k]; }

    inline float processSample (int c, float x) noexcept
    {
        auto& f = chans[c];
        return f[3].process (f[2].process (f[1].process (f[0].process (x))));
    }

private:
    void update() noexcept
    {
        bool changed = false;
        for (int k = 0; k < kN; ++k) if (std::abs (gains[k] - applied[k]) > 0.05f) changed = true;
        if (! changed) return;
        for (auto& c : chans)
            for (int k = 0; k < kN; ++k) c[k].setPeaking (fs, std::min ((double) kFreqs[k], fs * 0.4), 1.1, gains[k]);
        for (int k = 0; k < kN; ++k) applied[k] = gains[k];
    }

    double fs = 48000.0;
    Biquad chans[2][kN];
    float gains[kN] {}, applied[kN] {};
};
} // namespace ez
