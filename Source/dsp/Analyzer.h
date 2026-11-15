// EZ Voice FX - Assistant: analyses the raw take while "Learn" runs and suggests settings
#pragma once

#include "Loudness.h"

namespace ez
{
struct Suggestion
{
    float denoise = 70.0f, gate = 40.0f, deess = 50.0f, deplosive = 50.0f, hum = 0.0f;
    bool humOn = false;
    float vox = 30.0f;
    bool whisperOn = false;
    float snrDb = 0.0f, noiseFloor = -90.0f, speech = -30.0f, lra = 8.0f, whisperShare = 0.0f;
};

class InputAnalyzer
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& k : kw) k.prepare (fs);
        hp.setHighpass (fs, std::min (5000.0, fs * 0.4), 0.7071); hp.reset();
        lp.setLowpass (fs, 150.0, 0.7071); lp.reset();
        b50.setBandpass (fs, 50.0, 20.0); b60.setBandpass (fs, 60.0, 20.0);
        b50.reset(); b60.reset();
        blockLen = std::max (1, (int) std::lround (0.1 * fs));
        blocks.clear();
        blocks.reserve (kMax);
        resetAcc();
    }

    void reset() { blocks.clear(); resetAcc(); }

    // audio thread, only while learning
    void process (const float* const* ch, int numCh, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            float m = 0.0f;
            double z = 0.0;
            for (int c = 0; c < numCh; ++c)
            {
                m += ch[c][i];
                const float k = kw[c].process (ch[c][i]);
                z += (double) k * k;
            }
            m /= (float) numCh;
            const float h = hp.process (m), l = lp.process (m);
            const float a = b50.process (m), b = b60.process (m);
            accK += z; accFull += (double) m * m; accHi += (double) h * h;
            accLo += (double) l * l; accHum += (double) a * a + (double) b * b;
            if (++accN >= blockLen)
            {
                if (blocks.size() < kMax)
                    blocks.push_back ({ (float) (accK / accN), (float) (accFull / accN), (float) (accHi / accN),
                                        (float) (accLo / accN), (float) (accHum / accN) });
                resetAcc();
            }
        }
    }

    bool compute (Suggestion& s) const
    {
        if (blocks.size() < 20) return false;

        std::vector<float> L;
        L.reserve (blocks.size());
        for (auto& b : blocks) L.push_back (msToLufs (b.k));

        // gated speech loudness (simplified BS.1770 on 100 ms blocks)
        double sum = 0.0; int cnt = 0;
        for (size_t i = 0; i < blocks.size(); ++i) if (L[i] > -70.0f) { sum += blocks[i].k; ++cnt; }
        if (cnt == 0) return false;
        const float rel = msToLufs (sum / cnt) - 10.0f;
        double sum2 = 0.0; int cnt2 = 0;
        for (size_t i = 0; i < blocks.size(); ++i) if (L[i] > rel) { sum2 += blocks[i].k; ++cnt2; }
        const float speech = cnt2 > 0 ? msToLufs (sum2 / cnt2) : rel + 10.0f;

        std::vector<float> sorted (L);
        std::sort (sorted.begin(), sorted.end());
        const float floorL = sorted[(size_t) (sorted.size() / 10)];
        const float snr = speech - floorL;

        auto interp = [] (float x, float x0, float y0, float x1, float y1)
        {
            const float t = std::clamp ((x - x0) / (x1 - x0), 0.0f, 1.0f);
            return y0 + t * (y1 - y0);
        };
        s.snrDb = snr; s.noiseFloor = floorL; s.speech = speech;
        s.denoise = snr > 35.0f ? interp (snr, 35.0f, 55.0f, 55.0f, 25.0f) : interp (snr, 15.0f, 100.0f, 35.0f, 55.0f);
        s.gate = interp (snr, 20.0f, 60.0f, 50.0f, 20.0f);

        // sibilance: 90th percentile of high-band share in voiced blocks
        std::vector<float> ratios;
        int plosives = 0, voiced = 0;
        double humShare = 0.0; int quiet = 0;
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            const auto& b = blocks[i];
            if (L[i] > speech - 15.0f && b.full > 1.0e-10f)
            {
                ratios.push_back (b.hi / b.full);
                ++voiced;
                if (i > 0 && b.lo > 10.0f * blocks[i - 1].lo && b.lo > b.hi * 4.0f) ++plosives;
            }
            if (L[i] < floorL + 6.0f && b.full > 1.0e-12f) { humShare += b.hum / b.full; ++quiet; }
        }
        if (! ratios.empty())
        {
            std::sort (ratios.begin(), ratios.end());
            const float r90 = ratios[(size_t) (ratios.size() * 9 / 10)];
            s.deess = interp (r90, 0.03f, 30.0f, 0.30f, 85.0f);
        }
        const float minutes = (float) voiced * 0.1f / 60.0f;
        const float perMin = minutes > 0.0f ? (float) plosives / minutes : 0.0f;
        s.deplosive = interp (perMin, 2.0f, 30.0f, 20.0f, 80.0f);
        const float hs = quiet > 0 ? (float) (humShare / quiet) : 0.0f;
        s.humOn = hs > 0.25f;
        s.hum = s.humOn ? 80.0f : 0.0f;

        // dynamics of the voice (loudness range of voiced blocks) -> VOX amount
        std::vector<float> vl;
        int whispered = 0;
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            if (L[i] <= floorL + 10.0f) continue;
            vl.push_back (L[i]);
            const auto& b = blocks[i];
            if (b.full > 1.0e-10f && b.hi / b.full > 0.35f && L[i] < speech - 8.0f) ++whispered;
        }
        if (vl.size() > 10)
        {
            std::sort (vl.begin(), vl.end());
            s.lra = vl[vl.size() * 95 / 100] - vl[vl.size() * 10 / 100];
            s.vox = interp (s.lra, 5.0f, 15.0f, 16.0f, 60.0f);
            s.whisperShare = (float) whispered / (float) vl.size();
            s.whisperOn = s.whisperShare > 0.25f;
            if (s.whisperOn) { s.denoise = std::min (s.denoise, 60.0f); s.gate = std::min (s.gate, 20.0f); }
        }
        return true;
    }

private:
    struct Block { float k, full, hi, lo, hum; };
    static constexpr size_t kMax = 10 * 60 * 90;

    void resetAcc() noexcept { accK = accFull = accHi = accLo = accHum = 0.0; accN = 0; }

    double fs = 48000.0;
    KWeighting kw[2];
    Biquad hp, lp, b50, b60;
    int blockLen = 4800, accN = 0;
    double accK = 0.0, accFull = 0.0, accHi = 0.0, accLo = 0.0, accHum = 0.0;
    std::vector<Block> blocks;
};
} // namespace ez
