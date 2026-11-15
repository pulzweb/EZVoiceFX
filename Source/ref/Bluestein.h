// EZ Voice FX - FFT of any length (Bluestein / chirp-z on top of the radix-2 FFT).
// Needed for the MDX-Net STFT (n_fft = 7680, not a power of two).
#pragma once

#include "../dsp/FFT.h"

namespace ez
{
class BluesteinFFT
{
public:
    void prepare (int length)
    {
        n = length;
        m = nextPow2 (2 * n - 1);
        fft.prepare (m);
        w.resize ((size_t) n);
        for (int k = 0; k < n; ++k)
        {
            const long long kk = ((long long) k * k) % (2LL * n);          // keep the angle exact
            const double a = -kPi * (double) kk / (double) n;
            w[(size_t) k] = std::complex<float> ((float) std::cos (a), (float) std::sin (a));
        }
        B.assign ((size_t) m, std::complex<float> (0.0f, 0.0f));
        B[0] = std::conj (w[0]);
        for (int k = 1; k < n; ++k)
        {
            B[(size_t) k] = std::conj (w[(size_t) k]);
            B[(size_t) (m - k)] = std::conj (w[(size_t) k]);
        }
        fft.forward (B.data());
        A.assign ((size_t) m, std::complex<float> (0.0f, 0.0f));
    }

    int size() const noexcept { return n; }

    // X[k] = sum x[j] * exp(-2 pi i j k / n)
    void forward (const std::complex<float>* x, std::complex<float>* X)
    {
        for (int j = 0; j < n; ++j) A[(size_t) j] = x[j] * w[(size_t) j];
        for (int j = n; j < m; ++j) A[(size_t) j] = std::complex<float> (0.0f, 0.0f);
        fft.forward (A.data());
        for (int j = 0; j < m; ++j) A[(size_t) j] *= B[(size_t) j];
        fft.inverse (A.data());
        for (int k = 0; k < n; ++k) X[k] = A[(size_t) k] * w[(size_t) k];
    }

private:
    int n = 0, m = 0;
    FFT fft;
    std::vector<std::complex<float>> w, A, B;
};
} // namespace ez
