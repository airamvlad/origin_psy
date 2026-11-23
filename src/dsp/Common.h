#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace originpsy
{

constexpr double pi = 3.14159265358979323846;
constexpr double twoPi = 2.0 * pi;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float semitonesToRatio (float semis) noexcept { return std::exp2 (semis / 12.0f); }

/** Removes denormals and non-finite values so a bad sample can never
    poison a feedback loop. */
inline float sanitize (float x) noexcept
{
    if (! std::isfinite (x)) return 0.0f;
    return std::abs (x) < 1.0e-15f ? 0.0f : x;
}

/** One-pole parameter smoother. setTime() in seconds. */
class Smoother
{
public:
    void prepare (double sampleRate, double seconds) noexcept
    {
        coeff = seconds <= 0.0 ? 0.0f : (float) std::exp (-1.0 / (seconds * sampleRate));
    }

    void reset (float v) noexcept { value = target = v; }
    void setTarget (float t) noexcept { target = t; }
    float next() noexcept { value = target + coeff * (value - target); return value; }
    float current() const noexcept { return value; }

private:
    float coeff = 0.0f, value = 0.0f, target = 0.0f;
};

/** Tempo and bar position for one block, read from the host (or from the
    internal clock when the host is stopped or there is no host). */
struct Transport
{
    double bpm = 140.0;
    double ppqAtBlockStart = 0.0;   // quarter notes since song start
    double beatsPerBar = 4.0;       // in quarter notes (7/8 -> 3.5)
    bool playing = false;

    double samplesPerBeat (double sampleRate) const noexcept
    {
        return sampleRate * 60.0 / std::max (20.0, bpm);
    }
};

/** Free running clock used when the host is not playing, so tempo-synced
    modules still move while you tweak them. */
class InternalClock
{
public:
    void reset() noexcept { ppq = 0.0; }

    Transport next (double bpm, double beatsPerBar, int numSamples, double sampleRate) noexcept
    {
        Transport t;
        t.bpm = bpm;
        t.beatsPerBar = beatsPerBar;
        t.ppqAtBlockStart = ppq;
        t.playing = false;
        ppq += numSamples / t.samplesPerBeat (sampleRate);
        if (ppq > 1.0e6) ppq = std::fmod (ppq, 4096.0);
        return t;
    }

private:
    double ppq = 0.0;
};

/** Tempo divisions offered by the synced controls, in quarter notes. */
struct NoteDivision
{
    const char* name;
    double quarters;
};

inline constexpr std::array<NoteDivision, 12> noteDivisions { {
    { "1/32", 0.125 },
    { "1/16T", 1.0 / 6.0 },
    { "1/16", 0.25 },
    { "1/16D", 0.375 },
    { "1/8T", 1.0 / 3.0 },
    { "1/8", 0.5 },
    { "1/8D", 0.75 },
    { "1/4T", 2.0 / 3.0 },
    { "1/4", 1.0 },
    { "1/4D", 1.5 },
    { "1/2", 2.0 },
    { "1 bar", 4.0 },
} };

inline double divisionQuarters (int index) noexcept
{
    return noteDivisions[(size_t) std::clamp (index, 0, (int) noteDivisions.size() - 1)].quarters;
}

} // namespace originpsy
