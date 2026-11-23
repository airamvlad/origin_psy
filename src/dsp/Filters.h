#pragma once

#include "Common.h"

namespace originpsy
{

/** Second-order filter section (transposed direct form II). */
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = sanitize (b1 * x - a1 * y + z2);
        z2 = sanitize (b2 * x - a2 * y);
        return y;
    }

    /** Butterworth low-pass or high-pass. */
    void set (bool highpass, double frequency, double sampleRate) noexcept
    {
        const double fc = std::clamp (frequency, 10.0, sampleRate * 0.45);
        const double w = twoPi * fc / sampleRate;
        const double cosw = std::cos (w);
        const double alpha = std::sin (w) / (2.0 * 0.70710678118654752);
        const double a0 = 1.0 + alpha;

        const double edge = highpass ? (1.0 + cosw) : (1.0 - cosw);
        b0 = (float) (edge * 0.5 / a0);
        b1 = (float) ((highpass ? -edge : edge) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * cosw / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }
};

/** Linkwitz-Riley 24 dB/oct split. Low + high add up with a flat level. */
class BandSplit
{
public:
    void reset() noexcept
    {
        for (auto& f : lp) f.reset();
        for (auto& f : hp) f.reset();
    }

    void setFrequency (double frequency, double sampleRate) noexcept
    {
        for (auto& f : lp) f.set (false, frequency, sampleRate);
        for (auto& f : hp) f.set (true, frequency, sampleRate);
    }

    void process (float x, float& low, float& high) noexcept
    {
        low = lp[1].process (lp[0].process (x));
        high = hp[1].process (hp[0].process (x));
    }

private:
    Biquad lp[2], hp[2];
};

/** Low, mid and high bands that add back up to the input with a flat level. */
class ThreeBandSplit
{
public:
    void reset() noexcept
    {
        lowSplit.reset();
        highSplit.reset();
        lowAlign.reset();
    }

    void setFrequencies (double lowHz, double highHz, double sampleRate) noexcept
    {
        highHz = std::max (highHz, lowHz * 1.5);
        lowSplit.setFrequency (lowHz, sampleRate);
        highSplit.setFrequency (highHz, sampleRate);
        lowAlign.setFrequency (highHz, sampleRate);
    }

    void process (float x, float& low, float& mid, float& high) noexcept
    {
        float lowRaw = 0.0f, rest = 0.0f;
        lowSplit.process (x, lowRaw, rest);
        highSplit.process (rest, mid, high);

        // The upper split shifts the phase of mid and high; give the low band
        // the same shift so the three still add up flat.
        float a = 0.0f, b = 0.0f;
        lowAlign.process (lowRaw, a, b);
        low = a + b;
    }

private:
    BandSplit lowSplit, highSplit, lowAlign;
};

/** Topology preserving state variable filter (Zavalishin / Simper). Stable
    under fast modulation, which the formant filter needs. */
class Svf
{
public:
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    void set (double frequency, double q, double sampleRate) noexcept
    {
        const double fc = std::clamp (frequency, 20.0, sampleRate * 0.45);
        g = (float) std::tan (pi * fc / sampleRate);
        k = (float) (1.0 / std::max (0.3, q));
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    struct Out { float low, band, high; };

    Out process (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = sanitize (2.0f * v1 - ic1);
        ic2 = sanitize (2.0f * v2 - ic2);
        return { v2, v1, x - k * v1 - v2 };
    }

    /** Band-pass with unity gain at the centre frequency. */
    float bandpass (float x) noexcept { return process (x).band * k; }

private:
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

/** One-pole low-pass, used for damping and envelopes. */
class OnePole
{
public:
    void reset (float v = 0.0f) noexcept { z = v; }

    void setCutoff (double frequency, double sampleRate) noexcept
    {
        const double fc = std::clamp (frequency, 1.0, sampleRate * 0.49);
        a = (float) std::exp (-twoPi * fc / sampleRate);
    }

    float process (float x) noexcept { z = sanitize (x + a * (z - x)); return z; }

private:
    float a = 0.0f, z = 0.0f;
};

} // namespace originpsy
