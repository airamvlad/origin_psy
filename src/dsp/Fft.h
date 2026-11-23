#pragma once

#include "Common.h"

#include <complex>
#include <vector>

namespace originpsy
{

/** Plain radix-2 complex FFT. Tables are built in prepare(), the transform
    itself does not allocate. */
class Fft
{
public:
    void prepare (int order)
    {
        size = 1 << order;
        twiddle.resize ((size_t) size / 2);
        for (int i = 0; i < size / 2; ++i)
            twiddle[(size_t) i] = std::polar (1.0f, (float) (-twoPi * i / size));
        reversed.resize ((size_t) size);
        for (int i = 0; i < size; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b)
                if (i & (1 << b)) r |= 1 << (order - 1 - b);
            reversed[(size_t) i] = r;
        }
    }

    int getSize() const noexcept { return size; }

    /** In place. inverse = true runs the inverse transform (scaled by 1/size). */
    void perform (std::complex<float>* data, bool inverse) const noexcept
    {
        for (int i = 0; i < size; ++i)
        {
            const int r = reversed[(size_t) i];
            if (r > i) std::swap (data[i], data[r]);
        }
        for (int len = 2; len <= size; len <<= 1)
        {
            const int half = len / 2, step = size / len;
            for (int start = 0; start < size; start += len)
                for (int k = 0; k < half; ++k)
                {
                    auto w = twiddle[(size_t) (k * step)];
                    if (inverse) w = std::conj (w);
                    const auto a = data[start + k];
                    const auto b = data[start + k + half] * w;
                    data[start + k] = a + b;
                    data[start + k + half] = a - b;
                }
        }
        if (inverse)
        {
            const float scale = 1.0f / (float) size;
            for (int i = 0; i < size; ++i) data[i] *= scale;
        }
    }

private:
    int size = 0;
    std::vector<std::complex<float>> twiddle;
    std::vector<int> reversed;
};

} // namespace originpsy
