// EZ Voice FX - small radix-2 complex FFT (no external dependency)
#pragma once

#include "Common.h"

namespace ez
{
class FFT
{
public:
    void prepare (int size)
    {
        n = size;
        int bits = 0;
        while ((1 << bits) < n) ++bits;

        rev.assign ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                if (i & (1 << b)) r |= 1 << (bits - 1 - b);
            rev[(size_t) i] = r;
        }

        tw.resize ((size_t) (n / 2));
        for (int k = 0; k < n / 2; ++k)
        {
            const double a = -2.0 * kPi * (double) k / (double) n;
            tw[(size_t) k] = std::complex<float> ((float) std::cos (a), (float) std::sin (a));
        }
    }

    int size() const noexcept { return n; }

    void forward (std::complex<float>* d) noexcept { transform (d, false); }

    // Normalised inverse (result scaled by 1/N)
    void inverse (std::complex<float>* d) noexcept
    {
        transform (d, true);
        const float s = 1.0f / (float) n;
        for (int i = 0; i < n; ++i) d[i] *= s;
    }

private:
    void transform (std::complex<float>* d, bool inv) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = rev[(size_t) i];
            if (i < j) std::swap (d[i], d[j]);
        }

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len >> 1;
            const int step = n / len;
            for (int i = 0; i < n; i += len)
            {
                for (int k = 0; k < half; ++k)
                {
                    std::complex<float> w = tw[(size_t) (k * step)];
                    if (inv) w = std::conj (w);
                    const std::complex<float> u = d[i + k];
                    const std::complex<float> v = d[i + k + half] * w;
                    d[i + k]        = u + v;
                    d[i + k + half] = u - v;
                }
            }
        }
    }

    int n = 0;
    std::vector<int> rev;
    std::vector<std::complex<float>> tw;
};
} // namespace ez
