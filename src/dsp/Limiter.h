#pragma once

#include "Common.h"

namespace originpsy
{

/** Output safety limiter: instant attack, smooth release, no lookahead (so
    no latency). The gain is shared by both channels so the image stays put,
    and a final clamp guarantees nothing passes the ceiling. */
class Limiter
{
public:
    void prepare (double sampleRate) noexcept
    {
        release = (float) std::exp (-1.0 / (0.12 * sampleRate));
        reset();
    }

    void reset() noexcept { gain = 1.0f; minGain = 1.0f; }

    void process (float* left, float* right, int numSamples, float ceilingDb) noexcept
    {
        const float ceiling = dbToGain (std::clamp (ceilingDb, -24.0f, 0.0f));
        minGain = 1.0f;
        for (int n = 0; n < numSamples; ++n)
        {
            const float peak = std::max (std::abs (left[n]), right != nullptr ? std::abs (right[n]) : 0.0f);
            const float needed = peak > ceiling ? ceiling / peak : 1.0f;
            gain = needed < gain ? needed : needed + release * (gain - needed);
            minGain = std::min (minGain, gain);
            left[n] = std::clamp (left[n] * gain, -ceiling, ceiling);
            if (right != nullptr) right[n] = std::clamp (right[n] * gain, -ceiling, ceiling);
        }
    }

    /** Most gain reduction in the last block, in dB (0 or negative). */
    float getReductionDb() const noexcept { return 20.0f * std::log10 (std::max (1.0e-4f, minGain)); }

private:
    float release = 0.0f, gain = 1.0f, minGain = 1.0f;
};

} // namespace originpsy
