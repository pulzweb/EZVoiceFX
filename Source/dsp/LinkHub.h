// EZ Voice FX - process-wide link between plugin instances
//
// All EZ Voice FX instances in one DAW are the same DLL loaded once, so they can
// share this static hub. Used for:
//   - Beat Link: an instance in "Beat" role publishes the beat spectrum, vocal
//     instances use it to fit the vocal; vocals publish their spectrum so the beat
//     instance can make room for the voice (unmask).
//   - Group analysis: pressing Learn / Magic in one instance starts it in every
//     instance of the same group (A-D).
#pragma once

#include "Common.h"
#include <juce_core/juce_core.h>

namespace ez
{
class LinkHub
{
public:
    static constexpr int kSlots = 32;
    static constexpr int kBands = 8;
    static constexpr int kGroups = 5;            // 0 = no group, 1..4 = A..D
    enum Role { Free = 0, Reserved = 1, Vocal = 2, Beat = 3 };

    struct Slot
    {
        std::atomic<int> role { Free };
        std::atomic<uint32_t> stamp { 0 };
        std::atomic<float> bands[kBands];
        std::atomic<float> lufs { -70.0f };
        Slot() { for (auto& b : bands) b.store (0.0f); }
    };

    static LinkHub& get()
    {
        static LinkHub* hub = new LinkHub();     // never destroyed (instances may outlive statics)
        return *hub;
    }

    int acquire() noexcept
    {
        for (int i = 0; i < kSlots; ++i)
        {
            int expected = Free;
            if (slots[i].role.compare_exchange_strong (expected, Reserved)) return i;
        }
        return -1;
    }

    void release (int i) noexcept { if (i >= 0 && i < kSlots) slots[i].role.store (Free); }

    // audio thread: publish band energies (linear RMS) for this instance
    void publish (int i, Role role, const float* bands, float lufs) noexcept
    {
        if (i < 0 || i >= kSlots) return;
        auto& s = slots[i];
        for (int b = 0; b < kBands; ++b) s.bands[b].store (bands[b]);
        s.lufs.store (lufs);
        s.role.store (role);
        s.stamp.store (juce::Time::getMillisecondCounter());
    }

    void setIdle (int i) noexcept { if (i >= 0 && i < kSlots) slots[i].role.store (Reserved); }

    // sum of all active instances with this role (except 'self'); returns how many
    int gather (Role role, int self, float* outBands, float* outLufs = nullptr) const noexcept
    {
        const uint32_t now = juce::Time::getMillisecondCounter();
        int count = 0;
        double lsum = 0.0;
        for (int b = 0; b < kBands; ++b) outBands[b] = 0.0f;
        for (int i = 0; i < kSlots; ++i)
        {
            if (i == self) continue;
            const auto& s = slots[i];
            if (s.role.load() != role || now - s.stamp.load() > 600) continue;
            for (int b = 0; b < kBands; ++b) outBands[b] += s.bands[b].load();
            lsum += std::pow (10.0, s.lufs.load() / 10.0);
            ++count;
        }
        if (outLufs != nullptr) *outLufs = count > 0 ? (float) (10.0 * std::log10 (lsum + 1.0e-12)) : -70.0f;
        return count;
    }

    // group learn broadcast
    void broadcastStart (int group) noexcept { if (group > 0 && group < kGroups) startSeq[group].fetch_add (1); }
    void broadcastStop (int group) noexcept  { if (group > 0 && group < kGroups) stopSeq[group].fetch_add (1); }
    int getStartSeq (int group) const noexcept { return group > 0 && group < kGroups ? startSeq[group].load() : 0; }
    int getStopSeq (int group) const noexcept  { return group > 0 && group < kGroups ? stopSeq[group].load() : 0; }

    int countRole (Role role) const noexcept
    {
        const uint32_t now = juce::Time::getMillisecondCounter();
        int c = 0;
        for (const auto& s : slots)
            if (s.role.load() == role && now - s.stamp.load() <= 600) ++c;
        return c;
    }

private:
    LinkHub() = default;
    Slot slots[kSlots];
    std::atomic<int> startSeq[kGroups] {}, stopSeq[kGroups] {};
};

//==============================================================================
// 8-band level meter (mono): 60, 150, 300, 600, 1.2k, 2.5k, 5k, 10k Hz
class BandMeter
{
public:
    static constexpr float kFreqs[LinkHub::kBands] = { 60.0f, 150.0f, 300.0f, 600.0f, 1200.0f, 2500.0f, 5000.0f, 10000.0f };

    void prepare (double fs)
    {
        for (int b = 0; b < LinkHub::kBands; ++b)
        {
            bp[b].setBandpass (fs, std::min ((double) kFreqs[b], fs * 0.4), 1.2);
            bp[b].reset();
            ms[b] = 0.0f;
        }
        alpha = smoothAlpha (fs, 0.3);
    }

    inline void process (float x) noexcept
    {
        for (int b = 0; b < LinkHub::kBands; ++b)
        {
            const float y = bp[b].process (x);
            ms[b] += alpha * (y * y - ms[b]);
        }
    }

    void getRms (float* out) const noexcept
    {
        for (int b = 0; b < LinkHub::kBands; ++b) out[b] = std::sqrt (std::max (0.0f, ms[b]));
    }

private:
    Biquad bp[LinkHub::kBands];
    float ms[LinkHub::kBands] {};
    float alpha = 0.001f;
};
} // namespace ez
