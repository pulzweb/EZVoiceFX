// EZ Voice FX - loudness (ITU-R BS.1770 / EBU R128 K-weighting) and Auto-Level
#pragma once

#include "Common.h"

namespace ez
{
//==============================================================================
// K-weighting filter (same coefficients as libebur128, valid for any sample rate)
struct KWeighting
{
    Biquad shelf, hp;

    void prepare (double fs)
    {
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            shelf.setCoefficients (Vh + Vb * K / Q + K * K, 2.0 * (K * K - Vh), Vh - Vb * K / Q + K * K,
                                   1.0 + K / Q + K * K,     2.0 * (K * K - 1.0), 1.0 - K / Q + K * K);
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (kPi * f0 / fs);
            hp.setCoefficients (1.0, -2.0, 1.0,
                                1.0 + K / Q + K * K, 2.0 * (K * K - 1.0), 1.0 - K / Q + K * K);
        }
        shelf.reset(); hp.reset();
    }

    inline float process (float x) noexcept { return hp.process (shelf.process (x)); }
};

inline float msToLufs (double ms) noexcept { return (float) (-0.691 + 10.0 * std::log10 (ms + 1.0e-12)); }

//==============================================================================
// Ungated loudness meter for display (exponential, ~1.5 s)
class LoudnessMeter
{
public:
    void prepare (double fs)
    {
        for (auto& k : kw) k.prepare (fs);
        alpha = smoothAlpha (fs, 1.5);
        ms = 0.0;
    }

    void process (const float* const* ch, int numCh, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            double z = 0.0;
            for (int c = 0; c < numCh; ++c)
            {
                const float k = kw[c].process (ch[c][i]);
                z += (double) k * k;
            }
            ms += alpha * (z - ms);
        }
    }

    float getLufs() const noexcept { return msToLufs (ms); }

private:
    KWeighting kw[2];
    double alpha = 0.0, ms = 0.0;
};

//==============================================================================
// Auto-Level
//  Ride  : gated slow leveller that rides the vocal towards the target loudness.
//  Learn : you play the take once, the plugin measures the integrated loudness
//          (BS.1770 gating) and applies one exact static gain.
//  A gentle peak compressor after the gain keeps the level steady.
class AutoLevel
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& k : kw) k.prepare (fs);
        momAlpha  = smoothAlpha (fs, 0.4);
        longAlpha = smoothAlpha (fs, 3.0);
        gainAlpha = smoothAlpha (fs, 0.35);
        M = 0.0; S = 0.0; initialised = false;
        gainDb = 0.0f;

        compEnv.set (fs, 5.0, 150.0);
        compEnv.reset();
        compGain = 1.0f;

        subLen = std::max (1, (int) std::lround (0.1 * fs));
        blocks.clear();
        blocks.reserve (kMaxBlocks);
        resetLearnAccumulators();
        learning = false;
        learningFlag.store (false);
    }

    void setParams (bool on, float targetLufs, bool learnMode) noexcept
    {
        enabled = on; target = targetLufs; useLearned = learnMode;
    }

    // message thread
    void requestLearnStart() noexcept { reqStart.store (true); }
    void requestLearnStop()  noexcept { reqStop.store (true); }
    bool isLearning() const noexcept  { return learningFlag.load(); }
    bool hasLearned() const noexcept  { return learnedValid.load(); }
    float getLearnedLufs() const noexcept { return learnedLufs.load(); }
    void setLearned (float lufs, bool valid) noexcept { learnedLufs.store (lufs); learnedValid.store (valid); }

    float getGainDb() const noexcept { return gainDb; }
    float getCompGrDb() const noexcept { return -gainToDb (compGain); }
    float getInputLufs() const noexcept { return initialised ? msToLufs (S) : -70.0f; }

    void process (float* const* ch, int numCh, int n) noexcept
    {
        handleLearnRequests();

        const double gateRelOffset = 20.0;
        const float learnedGain = std::clamp (target - learnedLufs.load(), -30.0f, 30.0f);
        const bool fixed = useLearned && learnedValid.load() && ! learning;
        const float compThrDb = target + 10.0f;

        for (int i = 0; i < n; ++i)
        {
            double z = 0.0;
            for (int c = 0; c < numCh; ++c)
            {
                const float k = kw[c].process (ch[c][i]);
                z += (double) k * k;
            }

            M += momAlpha * (z - M);
            const float Lm = msToLufs (M);
            const float Ls = initialised ? msToLufs (S) : -70.0f;
            const bool active = Lm > -50.0f && (! initialised || Lm > Ls - (float) gateRelOffset);

            if (active)
            {
                if (! initialised) { S = std::max (M, 1.0e-10); initialised = true; }
                else S += longAlpha * (z - S);
            }

            if (learning) recordLearn (z);

            float desired;
            bool move = true;
            if (! enabled)       desired = 0.0f;
            else if (fixed)      desired = learnedGain;
            else
            {
                desired = initialised ? std::clamp (target - msToLufs (S), -24.0f, 24.0f) : 0.0f;
                move = active;   // hold the gain in pauses (no noise pumping)
            }
            if (move) gainDb += gainAlpha * (desired - gainDb);

            const float g = dbToGain (gainDb);

            float peak = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                ch[c][i] *= g;
                peak = std::max (peak, std::abs (ch[c][i]));
            }

            if (enabled)
            {
                const float e = compEnv.process (peak);
                const float eDb = gainToDb (e);
                const float over = eDb - compThrDb;
                const float grDb = over > 0.0f ? over * (1.0f - 1.0f / 2.5f) : 0.0f;
                compGain = dbToGain (-grDb);
                for (int c = 0; c < numCh; ++c) ch[c][i] *= compGain;
            }
            else
            {
                compGain = 1.0f;
            }
        }
    }

private:
    static constexpr size_t kMaxBlocks = 10 * 60 * 90;   // 90 minutes of 100 ms blocks

    void resetLearnAccumulators() noexcept
    {
        subAcc = 0.0; subN = 0; subFilled = 0; subIdx = 0;
        for (auto& s : sub) s = 0.0;
        blocks.clear();
    }

    void handleLearnRequests() noexcept
    {
        if (reqStart.exchange (false))
        {
            resetLearnAccumulators();
            learning = true;
            learningFlag.store (true);
        }
        if (reqStop.exchange (false))
        {
            if (learning) finishLearn();
            learning = false;
            learningFlag.store (false);
        }
    }

    inline void recordLearn (double z) noexcept
    {
        subAcc += z;
        if (++subN >= subLen)
        {
            sub[subIdx] = subAcc / (double) subN;
            subIdx = (subIdx + 1) & 3;
            subAcc = 0.0; subN = 0;
            if (subFilled < 4) ++subFilled;
            if (subFilled >= 4 && blocks.size() < kMaxBlocks)
                blocks.push_back (0.25 * (sub[0] + sub[1] + sub[2] + sub[3]));   // 400 ms block, 100 ms hop
        }
    }

    void finishLearn() noexcept
    {
        double sum = 0.0; int cnt = 0;
        for (double b : blocks)
            if (msToLufs (b) > -70.0f) { sum += b; ++cnt; }
        if (cnt == 0) return;   // nothing but silence: keep the previous value

        const float rel = msToLufs (sum / cnt) - 10.0f;
        double sum2 = 0.0; int cnt2 = 0;
        for (double b : blocks)
        {
            const float L = msToLufs (b);
            if (L > -70.0f && L > rel) { sum2 += b; ++cnt2; }
        }
        if (cnt2 == 0) return;

        learnedLufs.store (msToLufs (sum2 / cnt2));
        learnedValid.store (true);
    }

    double fs = 48000.0;
    KWeighting kw[2];
    double momAlpha = 0.0, longAlpha = 0.0, M = 0.0, S = 0.0;
    float gainAlpha = 0.0f, gainDb = 0.0f;
    bool initialised = false;

    EnvFollower compEnv;
    float compGain = 1.0f;

    bool enabled = true, useLearned = false;
    float target = -18.0f;

    // learn
    bool learning = false;
    int subLen = 4800, subN = 0, subFilled = 0, subIdx = 0;
    double subAcc = 0.0, sub[4] {};
    std::vector<double> blocks;
    std::atomic<bool> reqStart { false }, reqStop { false }, learningFlag { false }, learnedValid { false };
    std::atomic<float> learnedLufs { -18.0f };
};
} // namespace ez
