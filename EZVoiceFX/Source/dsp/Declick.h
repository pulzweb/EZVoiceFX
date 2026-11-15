// EZ Voice FX - de-clicker (mouth clicks, lip smacks, digital ticks)
//
// Detection: second difference of the signal compared with its running RMS.
// Repair: short marked regions (< ~1.2 ms) are rebuilt with a least-squares
// autoregressive model (order 16) predicted forward from the audio before the click
// and backward from the audio after it, then cross-faded (classic restoration method). Long "events" (consonants,
// transients) are left untouched. A fixed look-ahead keeps latency constant.
#pragma once

#include "Common.h"

namespace ez
{
class DeclickChannel
{
public:
    void prepare (double fs)
    {
        maxRun = std::max (8, (int) std::lround (0.0012 * fs));
        pre    = std::max (2, (int) std::lround (0.00006 * fs));
        post   = std::max (3, (int) std::lround (0.00015 * fs));
        M      = std::clamp ((int) std::lround (128.0 * fs / 48000.0), 64, 512);
        D      = std::max (maxRun + post + M + 8, (int) std::lround (0.0025 * fs));

        workF.assign ((size_t) (M + maxRun + 8), 0.0f);
        workB.assign ((size_t) (M + maxRun + 8), 0.0f);
        fwd.assign ((size_t) (maxRun + 8), 0.0f);
        bwd.assign ((size_t) (maxRun + 8), 0.0f);

        const int size = nextPow2 (D + M + 32) * 2;
        mask = size - 1;
        ring.assign ((size_t) size, 0.0f);
        flag.assign ((size_t) size, 0);

        t = 0;
        markUntil = -1;
        skipUntil = -1;
        x1 = x2 = 0.0f;
        env2 = 1.0e-8f;
        envAlpha = smoothAlpha (fs, 0.02);
        sens = sensTarget = 0.0f;
        sensAlpha = smoothAlpha (fs, 0.05);
        clicks.store (0);
    }

    int getLatency() const noexcept { return D; }
    void setAmount (float a) noexcept { sensTarget = std::clamp (a, 0.0f, 1.0f); }
    int getClickCount() const noexcept { return clicks.load(); }

    inline float processSample (float x) noexcept
    {
        sens += sensAlpha * (sensTarget - sens);

        const int64_t i = t;
        ring[(size_t) (i & mask)] = x;
        flag[(size_t) (i & mask)] = (i <= markUntil) ? 1 : 0;

        const float d = x - 2.0f * x1 + x2;
        x2 = x1; x1 = x;
        const float ad = std::abs (d);

        bool hit = false;
        if (sens > 0.01f)
        {
            const float k = 16.0f * std::pow (4.0f / 16.0f, sens);   // 16x .. 4x RMS
            const float thr = k * std::sqrt (env2) + 1.0e-3f;
            hit = ad > thr;
        }

        if (hit)
        {
            if (i <= skipUntil)
            {
                skipUntil = i + post;          // still inside a long event
            }
            else
            {
                const int64_t start = std::max (i - (int64_t) pre, i - (int64_t) D + 1);
                for (int64_t j = start; j <= i; ++j) flag[(size_t) (j & mask)] = 1;
                markUntil = i + post;
            }
        }
        else
        {
            env2 += envAlpha * (d * d - env2);
        }

        float y = 0.0f;
        const int64_t r = i - D;
        if (r >= 0)
        {
            if (flag[(size_t) (r & mask)]) repairFrom (r, i);
            y = ring[(size_t) (r & mask)];
        }

        ++t;
        return y;
    }

private:
    void repairFrom (int64_t r, int64_t newest) noexcept
    {
        int64_t e = r;
        while (e < newest && flag[(size_t) ((e + 1) & mask)]) ++e;

        const bool ongoing = (e >= newest);
        const int64_t len = e - r + 1;

        if (ongoing || len > maxRun)
        {
            // Not a click: leave it alone and ignore this event until it ends.
            for (int64_t j = r; j <= newest; ++j) flag[(size_t) (j & mask)] = 0;
            skipUntil = std::max (markUntil, newest);
            markUntil = r - 1;
            return;
        }

        const int gap = (int) (e - r + 1);
        const int na = (int) std::min<int64_t> (M, newest - e);

        bool ok = false;
        if (na >= 2 * kOrder + 8)
        {
            // forward model from the audio before the click
            for (int j = 0; j < M; ++j) workF[(size_t) j] = ring[(size_t) ((r - M + j) & mask)];
            // backward model: audio after the click, time-reversed
            for (int j = 0; j < na; ++j) workB[(size_t) j] = ring[(size_t) ((e + na - j) & mask)];

            double af[kOrder], ab[kOrder];
            if (solveAR (workF.data(), M, af) && solveAR (workB.data(), na, ab))
            {
                predict (workF.data(), M, af, gap, fwd.data());
                predict (workB.data(), na, ab, gap, bwd.data());
                for (int j = 0; j < gap; ++j)
                {
                    const float w = (float) (j + 1) / (float) (gap + 1);
                    const float v = (1.0f - w) * fwd[(size_t) j] + w * bwd[(size_t) (gap - 1 - j)];
                    ring[(size_t) ((r + j) & mask)] = std::clamp (v, -2.0f, 2.0f);
                    flag[(size_t) ((r + j) & mask)] = 0;
                }
                ok = true;
            }
        }

        if (! ok)
        {
            // fallback: cubic Hermite bridge
            const float ym1 = ring[(size_t) ((r - 2) & mask)];
            const float y0  = ring[(size_t) ((r - 1) & mask)];
            const float y1  = ring[(size_t) ((e + 1) & mask)];
            const float y2  = (e + 2 <= newest) ? ring[(size_t) ((e + 2) & mask)] : y1;
            const float L  = (float) (e - r + 2);
            const float m0 = (y0 - ym1) * L;
            const float m1 = (y2 - y1) * L;
            for (int64_t j = r; j <= e; ++j)
            {
                const float u  = (float) (j - (r - 1)) / L;
                const float u2 = u * u, u3 = u2 * u;
                ring[(size_t) (j & mask)] = (2.0f * u3 - 3.0f * u2 + 1.0f) * y0 + (u3 - 2.0f * u2 + u) * m0
                                          + (-2.0f * u3 + 3.0f * u2) * y1 + (u3 - u2) * m1;
                flag[(size_t) (j & mask)] = 0;
            }
        }
        clicks.fetch_add (1);
    }

    static constexpr int kOrder = 16;

    // Least-squares AR fit (covariance method): x[t] ~ sum a[j] * x[t-1-j]
    static bool solveAR (const float* seg, int n, double* a) noexcept
    {
        constexpr int p = kOrder;
        double A[p][p + 1] = {};
        for (int t = p; t < n; ++t)
        {
            for (int j = 0; j < p; ++j)
            {
                const double hj = seg[t - 1 - j];
                A[j][p] += hj * seg[t];
                for (int k = 0; k < p; ++k) A[j][k] += hj * seg[t - 1 - k];
            }
        }
        double tr = 0.0;
        for (int j = 0; j < p; ++j) tr += A[j][j];
        if (tr < 1.0e-14) return false;
        const double ridge = 1.0e-5 * tr / p + 1.0e-12;
        for (int j = 0; j < p; ++j) A[j][j] += ridge;

        for (int c = 0; c < p; ++c)
        {
            int piv = c;
            for (int r2 = c + 1; r2 < p; ++r2)
                if (std::abs (A[r2][c]) > std::abs (A[piv][c])) piv = r2;
            if (std::abs (A[piv][c]) < 1.0e-18) return false;
            if (piv != c)
                for (int k = 0; k <= p; ++k) std::swap (A[c][k], A[piv][k]);
            const double inv = 1.0 / A[c][c];
            for (int k = c; k <= p; ++k) A[c][k] *= inv;
            for (int r2 = 0; r2 < p; ++r2)
            {
                if (r2 == c) continue;
                const double f = A[r2][c];
                if (f == 0.0) continue;
                for (int k = c; k <= p; ++k) A[r2][k] -= f * A[c][k];
            }
        }
        for (int j = 0; j < p; ++j) a[j] = A[j][p];
        return true;
    }

    // Continue 'work' (n samples of history) by 'gap' predicted samples.
    static void predict (float* work, int n, const double* a, int gap, float* out) noexcept
    {
        for (int g = 0; g < gap; ++g)
        {
            double v = 0.0;
            for (int j = 0; j < kOrder; ++j) v += a[j] * work[n + g - 1 - j];
            const float fv = std::clamp ((float) v, -2.0f, 2.0f);
            work[n + g] = fv;
            out[g] = fv;
        }
    }

    std::vector<float> workF, workB, fwd, bwd;
    int M = 128;
    std::vector<float> ring;
    std::vector<uint8_t> flag;
    int64_t t = 0, markUntil = -1, skipUntil = -1;
    int mask = 0, D = 128, maxRun = 64, pre = 8, post = 16;
    float x1 = 0.0f, x2 = 0.0f, env2 = 1.0e-8f, envAlpha = 0.001f;
    float sens = 0.0f, sensTarget = 0.0f, sensAlpha = 0.001f;
    std::atomic<int> clicks { 0 };
};
} // namespace ez
