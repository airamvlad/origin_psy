#pragma once

#include "Common.h"

#include <vector>

namespace originpsy
{

/** Circular buffer with fractional (cubic) reads. Memory is allocated in
    prepare() only, never on the audio thread. */
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 8) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    int capacity() const noexcept { return mask - 4; }

    void push (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    /** Sample written `delay` samples ago (delay >= 1 reads the last push). */
    float read (double delay) const noexcept
    {
        delay = std::clamp (delay, 1.0, (double) capacity());
        const double pos = (double) writePos - delay;
        const int i = (int) std::floor (pos);
        const float f = (float) (pos - i);

        const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        // 4-point, 3rd order Hermite.
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

private:
    float at (int i) const noexcept { return buffer[(size_t) (i & mask)]; }

    std::vector<float> buffer { 0.0f };
    int mask = 0, writePos = 0;
};

} // namespace originpsy
