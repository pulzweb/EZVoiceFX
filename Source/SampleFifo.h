#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// Lock-free single-producer / single-consumer sample FIFO (audio thread -> GUI)
class SampleFifo
{
public:
    SampleFifo() : fifo (kSize) { buffer.assign ((size_t) kSize, 0.0f); }

    void push (const float* data, int n) noexcept
    {
        if (fifo.getFreeSpace() < n) return;      // GUI not reading: drop
        int s1, z1, s2, z2;
        fifo.prepareToWrite (n, s1, z1, s2, z2);
        for (int i = 0; i < z1; ++i) buffer[(size_t) (s1 + i)] = data[i];
        for (int i = 0; i < z2; ++i) buffer[(size_t) (s2 + i)] = data[z1 + i];
        fifo.finishedWrite (z1 + z2);
    }

    // returns number of samples copied into dest (max maxN)
    int pull (float* dest, int maxN) noexcept
    {
        const int n = std::min (maxN, fifo.getNumReady());
        if (n <= 0) return 0;
        int s1, z1, s2, z2;
        fifo.prepareToRead (n, s1, z1, s2, z2);
        for (int i = 0; i < z1; ++i) dest[i] = buffer[(size_t) (s1 + i)];
        for (int i = 0; i < z2; ++i) dest[z1 + i] = buffer[(size_t) (s2 + i)];
        fifo.finishedRead (z1 + z2);
        return z1 + z2;
    }

private:
    static constexpr int kSize = 32768;
    juce::AbstractFifo fifo;
    std::vector<float> buffer;
};
