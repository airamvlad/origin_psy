#pragma once

#include "Common.h"

#include <array>
#include <atomic>

namespace originpsy
{

/** Modulation sources. LFOs swing both ways (-1..1); the others go one way (0..1). */
enum class ModSource : int { none = 0, lfo1, lfo2, envelope, macro1, macro2, macro3, macro4, fraza };
constexpr int numModSources = 9;

inline const char* modSourceName (ModSource s) noexcept
{
    switch (s)
    {
        case ModSource::lfo1:     return "LFO 1";
        case ModSource::lfo2:     return "LFO 2";
        case ModSource::envelope: return "ENV";
        case ModSource::macro1:   return "M1";
        case ModSource::macro2:   return "M2";
        case ModSource::macro3:   return "M3";
        case ModSource::macro4:   return "M4";
        case ModSource::fraza:    return "FRAZA";
        case ModSource::none:     break;
    }
    return "";
}

inline bool isBipolar (ModSource s) noexcept { return s == ModSource::lfo1 || s == ModSource::lfo2; }

/** Tempo-locked LFO. The phase comes straight from the song position, so the
    LFO lands in the same place every time the song plays. */
struct Lfo
{
    enum Shape { sine = 0, triangle, sawUp, sawDown, square, sampleHold };
    static constexpr int numShapes = 6;

    struct Rate { const char* name; double quarters; };
    static constexpr std::array<Rate, 11> rates { {
        { "1/16", 0.25 }, { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 }, { "1/4T", 2.0 / 3.0 }, { "1/4", 1.0 },
        { "1/2", 2.0 }, { "1 bar", 4.0 }, { "2 bars", 8.0 }, { "4 bars", 16.0 }, { "8 bars", 32.0 }, { "16 bars", 64.0 },
    } };

    static double rateQuarters (int index) noexcept
    {
        return rates[(size_t) std::clamp (index, 0, (int) rates.size() - 1)].quarters;
    }

    /** Value -1..1 at a song position. */
    static float value (int shape, int rateIndex, double ppq, float phaseOffset = 0.0f) noexcept
    {
        const double cycles = ppq / rateQuarters (rateIndex) + (double) phaseOffset;
        const double cycleIndex = std::floor (cycles);
        const float ph = (float) (cycles - cycleIndex);
        switch (shape)
        {
            case sine:     return std::sin ((float) twoPi * ph);
            case triangle: return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
            case sawUp:    return 2.0f * ph - 1.0f;
            case sawDown:  return 1.0f - 2.0f * ph;
            case square:   return ph < 0.5f ? 1.0f : -1.0f;
            case sampleHold:
            {
                // Hash of the cycle number: random looking, but the same on every playback.
                auto x = (uint32_t) (int64_t) cycleIndex * 2654435761u;
                x ^= x >> 15; x *= 2246822519u; x ^= x >> 13;
                return (float) (x & 0xffffu) / 32767.5f - 1.0f;
            }
            default: return 0.0f;
        }
    }
};

/** Peak follower on the input, mapped to 0..1 over a range below 0 dBFS. */
class EnvelopeFollower
{
public:
    void prepare (double sampleRate) noexcept
    {
        attack = (float) std::exp (-1.0 / (0.005 * sampleRate));
        release = (float) std::exp (-1.0 / (0.15 * sampleRate));
        level = 0.0f;
    }

    void process (const float* left, const float* right, int numSamples) noexcept
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const float a = std::max (std::abs (left[n]), right ? std::abs (right[n]) : 0.0f);
            level = a + (a > level ? attack : release) * (level - a);
        }
        level = sanitize (level);
    }

    /** 0..1: rangeDb below full scale maps to 0, 0 dBFS to 1. */
    float value (float rangeDb = 36.0f) const noexcept
    {
        if (level <= 1.0e-6f) return 0.0f;
        const float db = 20.0f * std::log10 (level);
        return std::clamp (1.0f + db / rangeDb, 0.0f, 1.0f);
    }

private:
    float attack = 0.0f, release = 0.0f, level = 0.0f;
};

/** Eight connections from a source to a parameter, each with a depth (-1..1,
    in parts of the parameter's range). Lock-free: the editor writes, the audio
    thread reads. */
class ModMatrix
{
public:
    static constexpr int numSlots = 8;

    struct Slot
    {
        ModSource source = ModSource::none;
        int destination = -1;   // index into the processor's list of modulatable parameters
        float depth = 0.0f;
        bool used() const noexcept { return source != ModSource::none && destination >= 0; }
    };

    Slot get (int i) const noexcept
    {
        const auto& s = slots[(size_t) i];
        return { (ModSource) s.source.load (std::memory_order_relaxed), s.destination.load (std::memory_order_relaxed),
                 s.depth.load (std::memory_order_relaxed) };
    }

    void set (int i, const Slot& v) noexcept
    {
        auto& s = slots[(size_t) i];
        s.depth.store (std::clamp (v.depth, -1.0f, 1.0f), std::memory_order_relaxed);
        s.destination.store (v.destination, std::memory_order_relaxed);
        s.source.store ((int) v.source, std::memory_order_relaxed);
    }

    void setDepth (int i, float depth) noexcept
    {
        slots[(size_t) i].depth.store (std::clamp (depth, -1.0f, 1.0f), std::memory_order_relaxed);
    }

    void clear (int i) noexcept { set (i, {}); }
    void clearAll() noexcept { for (int i = 0; i < numSlots; ++i) clear (i); }

    /** Adds a connection, or returns the existing one for the same pair.
        Returns the slot index, or -1 when all slots are used. */
    int connect (ModSource source, int destination, float depth) noexcept
    {
        for (int i = 0; i < numSlots; ++i)
        {
            const auto s = get (i);
            if (s.source == source && s.destination == destination) return i;
        }
        for (int i = 0; i < numSlots; ++i)
            if (! get (i).used())
            {
                set (i, { source, destination, depth });
                return i;
            }
        return -1;
    }

    /** Sum of depth x source value for one destination. */
    float offsetFor (int destination, const std::array<float, numModSources>& sourceValues) const noexcept
    {
        float sum = 0.0f;
        for (int i = 0; i < numSlots; ++i)
        {
            const auto s = get (i);
            if (s.used() && s.destination == destination)
                sum += s.depth * sourceValues[(size_t) s.source];
        }
        return sum;
    }

private:
    struct AtomicSlot
    {
        std::atomic<int> source { 0 };
        std::atomic<int> destination { -1 };
        std::atomic<float> depth { 0.0f };
    };
    std::array<AtomicSlot, numSlots> slots;
};

} // namespace originpsy
