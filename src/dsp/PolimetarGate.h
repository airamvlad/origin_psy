#pragma once

#include "Filters.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace originpsy
{

/** Euclidean rhythm: `hits` spread as evenly as possible over `steps`,
    turned right by `rotate`. Bit i set = step i plays. */
inline uint32_t euclid (int steps, int hits, int rotate) noexcept
{
    steps = std::clamp (steps, 1, 32);
    hits = std::clamp (hits, 0, steps);
    uint32_t mask = 0;
    for (int i = 0; i < steps; ++i)
        if (((i * hits) % steps) < hits) mask |= 1u << i;
    rotate = ((rotate % steps) + steps) % steps;
    uint32_t rotated = 0;
    for (int i = 0; i < steps; ++i)
        if (mask & (1u << i)) rotated |= 1u << ((i + rotate) % steps);
    return rotated;
}

/** Polimetar Gate: three bands, each with its own gate rhythm and its own
    pattern length. With lengths like 16, 7 and 13 the bands line up again
    only after many bars, so one sustained sound turns into a moving texture. */
class PolimetarGate
{
public:
    static constexpr int numBands = 3;

    struct Band
    {
        bool euclidMode = true;
        int steps = 16;          // 1..32
        int hits = 5;            // euclid mode
        int rotate = 0;
        uint32_t pattern = 0x5555; // steps mode, bit i = step i
        int division = 2;        // index into noteDivisions (1/16)
        float depth = 1.0f;      // 0..1
        float length = 0.6f;     // gate open time as part of the step, 0.05..1
        float attackMs = 1.0f;   // 0.1..50
        float releaseMs = 20.0f; // 0.1..200
    };

    struct Params
    {
        std::array<Band, numBands> bands {};
        float lowHz = 200.0f, highHz = 2500.0f;
        float mix = 1.0f;

        Params()
        {
            bands[0].steps = 16; bands[0].hits = 4; bands[0].division = 2;   // low: 4 on the floor feel
            bands[1].steps = 7;  bands[1].hits = 3; bands[1].division = 2;   // mid: 7 step cycle
            bands[2].steps = 13; bands[2].hits = 5; bands[2].division = 2;   // high: euclid 5 of 13
        }
    };

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        mixSmooth.prepare (sr, 0.02);
        reset();
    }

    void reset()
    {
        for (auto& s : split) s.reset();
        for (auto& g : env) g = 1.0f;
        mixSmooth.reset (params.mix);
        configured = false;
    }

    void setParams (const Params& p) noexcept { params = p; }

    /** Mask the band actually plays (euclid or drawn), for the editor too. */
    static uint32_t activeMask (const Band& b) noexcept
    {
        const int steps = std::clamp (b.steps, 1, 32);
        const uint32_t all = steps == 32 ? 0xffffffffu : ((1u << steps) - 1u);
        return (b.euclidMode ? euclid (steps, b.hits, b.rotate) : b.pattern) & all;
    }

    /** Current step of each band, for the editor (-1 before the first block). */
    int getStep (int band) const noexcept { return currentStep[(size_t) band].load (std::memory_order_relaxed); }

    /** Gain the band's gate wants at a ppq position (0..1 before depth). */
    static bool isOpen (const Band& b, double ppq, int* stepOut = nullptr) noexcept
    {
        const int steps = std::clamp (b.steps, 1, 32);
        const double stepLen = divisionQuarters (b.division);
        const double stepPos = ppq / stepLen;
        const long long stepIndex = (long long) std::floor (stepPos);
        const int step = (int) (((stepIndex % steps) + steps) % steps);
        if (stepOut) *stepOut = step;
        const double within = stepPos - (double) stepIndex;
        return (activeMask (b) & (1u << step)) != 0 && within < std::clamp (b.length, 0.05f, 1.0f);
    }

    void process (float* left, float* right, int numSamples, const Transport& t) noexcept
    {
        if (! configured || std::abs (params.lowHz - lastLow) > 0.01f || std::abs (params.highHz - lastHigh) > 0.01f)
        {
            for (auto& s : split) s.setFrequencies (params.lowHz, params.highHz, sr);
            lastLow = params.lowHz; lastHigh = params.highHz;
            configured = true;
        }

        mixSmooth.setTarget (params.mix);
        const double beatsPerSample = 1.0 / t.samplesPerBeat (sr);

        std::array<float, numBands> att {}, rel {}, floorGain {};
        for (int b = 0; b < numBands; ++b)
        {
            const auto& band = params.bands[(size_t) b];
            att[(size_t) b] = (float) std::exp (-1.0 / (std::max (0.1f, band.attackMs) * 0.001 * sr));
            rel[(size_t) b] = (float) std::exp (-1.0 / (std::max (0.1f, band.releaseMs) * 0.001 * sr));
            floorGain[(size_t) b] = 1.0f - std::clamp (band.depth, 0.0f, 1.0f);
        }

        for (int n = 0; n < numSamples; ++n)
        {
            const double ppq = t.ppqAtBlockStart + n * beatsPerSample;
            float bandsL[numBands], bandsR[numBands];
            split[0].process (left[n], bandsL[0], bandsL[1], bandsL[2]);
            split[1].process (right[n], bandsR[0], bandsR[1], bandsR[2]);

            float outL = 0.0f, outR = 0.0f;
            for (int b = 0; b < numBands; ++b)
            {
                int step = 0;
                const bool open = isOpen (params.bands[(size_t) b], ppq, &step);
                if (n == 0) currentStep[(size_t) b].store (step, std::memory_order_relaxed);
                const float target = open ? 1.0f : floorGain[(size_t) b];
                auto& e = env[(size_t) b];
                const float c = target > e ? att[(size_t) b] : rel[(size_t) b];
                e = target + c * (e - target);
                outL += bandsL[b] * e;
                outR += bandsR[b] * e;
            }

            const float m = mixSmooth.next();
            // Dry goes through the same split so mix 0..1 has no phase comb.
            const float dryL = bandsL[0] + bandsL[1] + bandsL[2];
            const float dryR = bandsR[0] + bandsR[1] + bandsR[2];
            left[n] = dryL * (1.0f - m) + outL * m;
            right[n] = dryR * (1.0f - m) + outR * m;
        }
    }

private:
    double sr = 48000.0;
    Params params;
    std::array<ThreeBandSplit, 2> split;
    std::array<float, numBands> env {};
    std::array<std::atomic<int>, numBands> currentStep { { -1, -1, -1 } };
    Smoother mixSmooth;
    float lastLow = 0.0f, lastHigh = 0.0f;
    bool configured = false;
};

} // namespace originpsy
