#pragma once

#include "Filters.h"

#include <vector>

namespace originpsy
{

/** Monophonic pitch tracker (YIN) for bass and acid lines.

    The input is low-passed and decimated by 4, then YIN runs on a 1024
    sample window every 128 decimated samples (about 10 ms at 48 kHz). */
class PitchTracker
{
public:
    static constexpr int decimation = 4;
    static constexpr int window = 1024;
    static constexpr int hop = 128;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        dsr = sampleRate / decimation;
        ring.assign ((size_t) window, 0.0f);
        frame.assign ((size_t) window, 0.0f);
        diff.assign ((size_t) window / 2, 0.0f);
        for (auto& f : aa) f.set (false, std::min (2500.0, dsr * 0.4), sr);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        for (auto& f : aa) f.reset();
        writePos = 0; decimCount = 0; hopCount = 0;
        frequency = 0.0f; confidence = 0.0f; level = 0.0f;
    }

    /** Feed one input sample. Returns true when a new estimate is ready. */
    bool push (float x) noexcept
    {
        const float y = aa[1].process (aa[0].process (x));
        if (++decimCount < decimation) return false;
        decimCount = 0;

        ring[(size_t) writePos] = y;
        writePos = (writePos + 1) % window;
        if (++hopCount < hop) return false;
        hopCount = 0;
        analyse();
        return true;
    }

    float getFrequency() const noexcept { return frequency; }     // Hz, 0 = none
    float getConfidence() const noexcept { return confidence; }   // 0..1
    float getLevel() const noexcept { return level; }             // RMS of the window

    /** MIDI note number (fractional) of a frequency. */
    static float frequencyToNote (float hz) noexcept
    {
        return hz <= 0.0f ? -1.0f : 69.0f + 12.0f * std::log2 (hz / 440.0f);
    }

private:
    void analyse() noexcept
    {
        double energy = 0.0;
        for (int i = 0; i < window; ++i)
        {
            const float v = ring[(size_t) ((writePos + i) % window)];
            frame[(size_t) i] = v;
            energy += (double) v * v;
        }
        level = (float) std::sqrt (energy / window);
        if (level < 1.0e-3f) { confidence = 0.0f; return; }

        const int maxTau = window / 2;
        const int minTau = std::max (2, (int) (dsr / 1200.0));   // up to 1.2 kHz
        const int maxSearch = std::min (maxTau - 1, (int) (dsr / 30.0)); // down to 30 Hz

        // Difference function.
        for (int tau = 1; tau < maxTau; ++tau)
        {
            double sum = 0.0;
            for (int i = 0; i < maxTau; ++i)
            {
                const double d = (double) frame[(size_t) i] - frame[(size_t) (i + tau)];
                sum += d * d;
            }
            diff[(size_t) tau] = (float) sum;
        }

        // Cumulative mean normalised difference.
        diff[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau < maxTau; ++tau)
        {
            running += diff[(size_t) tau];
            diff[(size_t) tau] = running > 0.0 ? (float) (diff[(size_t) tau] * tau / running) : 1.0f;
        }

        int best = -1;
        for (int tau = minTau; tau <= maxSearch; ++tau)
            if (diff[(size_t) tau] < threshold)
            {
                while (tau + 1 <= maxSearch && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau;
                break;
            }

        if (best < 0) { confidence = 0.0f; return; }

        // Parabolic interpolation around the minimum.
        double refined = best;
        if (best > 1 && best < maxTau - 1)
        {
            const double a = diff[(size_t) best - 1], b = diff[(size_t) best], c = diff[(size_t) best + 1];
            const double den = a - 2.0 * b + c;
            if (std::abs (den) > 1.0e-12) refined = best + 0.5 * (a - c) / den;
        }

        frequency = (float) (dsr / refined);
        confidence = std::clamp (1.0f - diff[(size_t) best], 0.0f, 1.0f);
    }

    static constexpr float threshold = 0.15f;

    double sr = 48000.0, dsr = 12000.0;
    std::vector<float> ring, frame, diff;
    Biquad aa[2];
    int writePos = 0, decimCount = 0, hopCount = 0;
    float frequency = 0.0f, confidence = 0.0f, level = 0.0f;
};

} // namespace originpsy
