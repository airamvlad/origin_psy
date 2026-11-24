#pragma once

#include "Common.h"

#include <vector>

namespace originpsy
{

/** Global Dry/Wet for the whole plugin. Keeps a copy of the input, delayed by
    the chain's latency (Spectral Ratchet), so dry and wet line up and the
    blend does not comb. */
class DryWet
{
public:
    void prepare (double sampleRate, int maxDelaySamples, int maxBlockSize)
    {
        maxDelay = std::max (0, maxDelaySamples);
        const size_t size = (size_t) (maxDelay + std::max (1, maxBlockSize) + 1);
        for (auto& b : buffer) b.assign (size, 0.0f);
        writePos = 0;
        smooth.prepare (sampleRate, 0.02);
        smooth.reset (mix);
    }

    void reset() noexcept
    {
        for (auto& b : buffer) std::fill (b.begin(), b.end(), 0.0f);
        smooth.reset (mix);
    }

    /** 0 = only the input, 1 = only the effect. */
    void setMix (float m) noexcept { mix = std::clamp (m, 0.0f, 1.0f); }
    /** Samples the wet signal is late by. */
    void setDelay (int samples) noexcept { delay = std::clamp (samples, 0, maxDelay); }

    /** True when a block this long fits (else call prepare again). */
    bool fits (int numSamples) const noexcept { return (size_t) (maxDelay + numSamples) < buffer[0].size(); }

    /** Call with the input before the chain runs (right may be null). */
    void pushDry (const float* left, const float* right, int numSamples) noexcept
    {
        dryStart = writePos;
        const size_t size = buffer[0].size();
        for (int n = 0; n < numSamples; ++n)
        {
            buffer[0][writePos] = left[n];
            buffer[1][writePos] = right != nullptr ? right[n] : left[n];
            writePos = (writePos + 1) % size;
        }
    }

    /** Call with the chain's output; blends in the dry copy pushed for this block. */
    void blend (float* left, float* right, int numSamples) noexcept
    {
        smooth.setTarget (mix);
        if (mix >= 1.0f && smooth.current() >= 0.99999f)
        {
            smooth.reset (1.0f);
            return; // fully wet: nothing to do
        }
        const size_t size = buffer[0].size();
        size_t read = (dryStart + size - (size_t) delay) % size;
        for (int n = 0; n < numSamples; ++n)
        {
            const float m = smooth.next();
            left[n] = buffer[0][read] * (1.0f - m) + left[n] * m;
            if (right != nullptr) right[n] = buffer[1][read] * (1.0f - m) + right[n] * m;
            read = (read + 1) % size;
        }
    }

private:
    std::array<std::vector<float>, 2> buffer;
    size_t writePos = 0, dryStart = 0;
    int delay = 0, maxDelay = 0;
    float mix = 1.0f;
    Smoother smooth;
};

} // namespace originpsy
