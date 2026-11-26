#pragma once

#include "Common.h"

#include <array>
#include <cstdint>
#include <vector>

namespace originpsy
{

/** Bass glide: the pitch of the sound slides between notes like the
    portamento of a psytrance bass, on chosen steps of a tempo-synced
    pattern. Each step has its own note offset and a Glide switch. A phrase
    Dive bends the whole sound down (or up) before the bar line.

    The pitch is changed by reading the input from a delay line at another
    speed (a tape-style varispeed). When the read position runs too far
    ahead or behind, it jumps by a grain to the place where the waveform
    matches best and crossfades there, so bass stays smooth. When the
    pitch is back at zero the module goes back to the live input. */
class Glide
{
public:
    enum Mode { slide = 0, bendIn, bendOut, scoop };
    static constexpr int numModes = 4;
    enum Dive { diveOff = 0, diveButton, dive1, dive2, dive4, dive8, dive16 };
    static constexpr int numDives = 7;

    static constexpr int maxSteps = 16;
    static constexpr int minNote = -24, maxNote = 24;
    static constexpr int numDrawnPoints = 64;
    static constexpr float minPitch = -48.0f, maxPitch = 24.0f;

    struct Length { const char* name; double quarters; };
    static constexpr std::array<Length, 6> diveLengths { {
        { "1/16", 0.25 }, { "1/8", 0.5 }, { "1/4", 1.0 }, { "1/2", 2.0 }, { "1 bar", 4.0 }, { "2 bars", 8.0 },
    } };

    struct Params
    {
        int mode = bendIn;
        int division = 2;            // index into noteDivisions: length of a step
        int steps = 16;
        uint32_t glides = 0x8888u;   // bit i set = step i glides
        std::array<int, maxSteps> notes {}; // semitones per step
        float interval = -7.0f;      // semitones the bend starts from / goes to
        float time = 0.5f;           // glide length as a fraction of a step (0.02..4)
        float curve = -0.4f;         // -1 = fast start then settles, 0 = straight, 1 = slow start then rushes in
        // Hand-drawn glide shape (numDrawnPoints values 0..1 = how far it has
        // travelled from the start to the target). Null = use the Curve knob.
        const float* drawn = nullptr;
        float grain = 0.08f;         // seconds before the read position jumps
        float mix = 1.0f;
        int dive = diveOff;
        bool diveHold = false;       // the Dive button
        int diveLength = 3;          // index into diveLengths
        float diveDepth = -24.0f;    // semitones
        float diveCurve = 0.3f;      // same as curve, for the dive
    };

    /** Shape of a glide from the knob: progress 0..1 -> travelled 0..1. */
    static float knobShape (float curve, double x) noexcept
    {
        x = std::clamp (x, 0.0, 1.0);
        const double c = std::clamp ((double) curve, -1.0, 1.0);
        if (c < 0.0) return (float) (1.0 - std::pow (1.0 - x, 1.0 - 4.0 * c));
        return (float) std::pow (x, 1.0 + 4.0 * c);
    }

    static float drawnAt (const float* points, double x) noexcept
    {
        const double pos = std::clamp (x, 0.0, 1.0) * (numDrawnPoints - 1);
        const int i = std::min ((int) pos, numDrawnPoints - 2);
        const double frac = pos - i;
        return std::clamp ((float) (points[i] + (points[i + 1] - points[i]) * frac), 0.0f, 1.0f);
    }

    static void fillDrawnFromKnob (float curve, float* points) noexcept
    {
        for (int i = 0; i < numDrawnPoints; ++i)
            points[i] = knobShape (curve, (double) i / (numDrawnPoints - 1));
    }

    static double diveQuarters (int index) noexcept
    {
        return diveLengths[(size_t) std::clamp (index, 0, (int) diveLengths.size() - 1)].quarters;
    }

    static constexpr double maxGrainSeconds = 0.4;

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        int size = 1;
        while (size < (int) (sampleRate * (maxGrainSeconds + 0.2)) + 16) size <<= 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        mono.assign ((size_t) size, 0.0f);
        mask = size - 1;
        mixSmooth.prepare (sr, 0.02);
        pitchCoeff = (float) std::exp (-1.0 / (0.0008 * sr));
        reset();
    }

    void reset()
    {
        for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f);
        std::fill (mono.begin(), mono.end(), 0.0f);
        writePos = 0;
        direct = true;
        lag = oldLag = 0.0;
        oldDirect = false;
        xfPos = xfLen = 0;
        steady = 0;
        lastStep = INT64_MIN;
        step = -1;
        rampPos = 1.0;
        rampFrom = rampTo = 0.0f;
        held = 0.0f;
        stepPitch = pitch = target = 0.0f;
        divePos = 0.0;
        diveOffset = 0.0f;
        mixSmooth.reset (params.mix);
    }

    void setParams (const Params& p) noexcept { params = p; }

    /** Pitch heard now in semitones (for the display). */
    float getPitch() const noexcept { return pitch; }
    /** Step playing now (for the display), -1 before the first. */
    int getStep() const noexcept { return step; }
    /** How far the dive has gone, in semitones. */
    float getDive() const noexcept { return diveOffset; }

    void process (float* left, float* right, int numSamples, const Transport& t) noexcept
    {
        const double spb = t.samplesPerBeat (sr);
        const double stepQ = divisionQuarters (params.division);
        const int steps = std::clamp (params.steps, 1, maxSteps);
        const double grainSamples = std::clamp ((double) params.grain, 0.01, maxGrainSeconds) * sr;
        mixSmooth.setTarget (std::clamp (params.mix, 0.0f, 1.0f));

        for (int n = 0; n < numSamples; ++n)
        {
            const double ppq = t.ppqAtBlockStart + n / spb;

            // Steps.
            const auto idx = (int64_t) std::floor (ppq / stepQ);
            if (idx != lastStep)
            {
                lastStep = idx;
                startStep ((int) (((idx % steps) + steps) % steps), stepQ * spb);
            }
            stepPitch = advanceRamp();

            // Phrase dive.
            diveOffset = diveAt (ppq, t.beatsPerBar, spb);

            target = std::clamp (stepPitch + diveOffset, minPitch, maxPitch);
            pitch = target + pitchCoeff * (pitch - target);

            // Audio.
            const float inL = left[n], inR = right[n];
            push (inL, inR);
            float wetL, wetR;
            render (wetL, wetR, grainSamples);

            const float m = mixSmooth.next();
            left[n] = inL + (wetL - inL) * m;
            right[n] = inR + (wetR - inR) * m;
        }
    }

private:
    //==============================================================================
    void startStep (int s, double stepSamples) noexcept
    {
        step = s;
        const float note = (float) std::clamp (params.notes[(size_t) s], minNote, maxNote);
        const bool glides = ((params.glides >> s) & 1u) != 0;
        const double len = std::max (1.0, std::clamp ((double) params.time, 0.02, 4.0) * stepSamples);
        rampInc = 1.0 / len;

        if (! glides)
        {
            kind = hold;
            held = note;
            rampPos = 1.0;
            return;
        }
        rampPos = 0.0;
        switch (std::clamp (params.mode, 0, numModes - 1))
        {
            case slide:   kind = travel; rampFrom = stepPitch; rampTo = note; break;
            case bendIn:  kind = travel; rampFrom = note + params.interval; rampTo = note; break;
            case bendOut: kind = travel; rampFrom = note; rampTo = note + params.interval; break;
            case scoop:   kind = outAndBack; rampFrom = note; rampTo = note + params.interval; break;
            default: break;
        }
    }

    float shape (double x) const noexcept
    {
        return params.drawn != nullptr ? drawnAt (params.drawn, x) : knobShape (params.curve, x);
    }

    float advanceRamp() noexcept
    {
        if (kind == hold) return held;
        rampPos = std::min (1.0, rampPos + rampInc);
        if (kind == travel)
            return rampFrom + (rampTo - rampFrom) * shape (rampPos);
        // Out to the interval in the first half, back in the second.
        const float y = rampPos < 0.5 ? shape (rampPos * 2.0) : shape ((1.0 - rampPos) * 2.0);
        return rampFrom + (rampTo - rampFrom) * y;
    }

    float diveAt (double ppq, double beatsPerBar, double spb) noexcept
    {
        static constexpr int bars[] = { 0, 0, 1, 2, 4, 8, 16 };
        const int d = std::clamp (params.dive, 0, numDives - 1);
        if (d == diveOff) { divePos = 0.0; return 0.0f; }
        double x = 0.0;
        if (d == diveButton)
        {
            if (params.diveHold)
                divePos = std::min (1.0, divePos + 1.0 / std::max (1.0, diveQuarters (params.diveLength) * spb));
            else
                divePos = 0.0;
            x = divePos;
        }
        else
        {
            const double period = bars[d] * std::max (0.5, beatsPerBar);
            const double len = std::min (diveQuarters (params.diveLength), period);
            const double pos = ppq - std::floor (ppq / period) * period;
            const double start = period - len;
            x = pos >= start ? (pos - start) / len : 0.0;
            divePos = x;
        }
        if (x <= 0.0) return 0.0f;
        return params.diveDepth * knobShape (params.diveCurve, x);
    }

    //==============================================================================
    void push (float l, float r) noexcept
    {
        buf[0][(size_t) writePos] = l;
        buf[1][(size_t) writePos] = r;
        mono[(size_t) writePos] = 0.5f * (l + r);
        writePos = (writePos + 1) & mask;
    }

    /** Sample `lag` samples before the newest one (lag 0 = the newest). */
    float readAt (const std::vector<float>& b, double lagSamples) const noexcept
    {
        const double l = std::clamp (lagSamples, 0.0, (double) mask - 8.0);
        const double pos = (double) (writePos - 1) - l;
        const int i = (int) std::floor (pos);
        const float f = (float) (pos - i);
        auto at = [&] (int k) { return b[(size_t) (k & mask)]; };
        if (l < 2.0)
            return at (i) + (at (i + 1) - at (i)) * f;
        const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }

    /** Lag near `centre` where the waveform looks most like the one at `from`. */
    double bestLag (double from, double centre, double range) const noexcept
    {
        const int window = std::max (32, (int) (0.01 * sr));
        constexpr int stride = 4;
        const int lo = (int) std::max (minLag + 1.0, centre - range);
        const int hi = (int) std::min ((double) mask - window - 16.0, centre + range);
        if (hi <= lo) return std::clamp (centre, minLag + 1.0, (double) mask - window - 16.0);
        const int ref = (int) std::lround (from);
        const int newest = writePos - 1;
        auto at = [&] (int lagSamples, int k) { return mono[(size_t) ((newest - lagSamples - k) & mask)]; };

        double best = -1.0e30;
        int bestAt = (int) centre;
        for (int c = lo; c <= hi; c += 2)
        {
            double dot = 0.0, energy = 1.0e-9;
            for (int k = 0; k < window; k += stride)
            {
                const float v = at (c, k);
                dot += (double) at (ref, k) * v;
                energy += (double) v * v;
            }
            // A slight preference for the middle of the range keeps the grain length steady.
            const double score = dot / std::sqrt (energy) - 1.0e-6 * std::abs (c - centre);
            if (score > best) { best = score; bestAt = c; }
        }
        return (double) bestAt;
    }

    void startCrossfade (double newLag, int length) noexcept
    {
        oldLag = lag;
        oldDirect = direct;
        lag = newLag;
        direct = false;
        xfLen = std::max (16, length);
        xfPos = 0;
    }

    void render (float& outL, float& outR, double grainSamples) noexcept
    {
        const double ratio = std::exp2 ((double) pitch / 12.0);
        const double slope = 1.0 - ratio;     // how the lag changes per sample
        const bool atZero = std::abs (pitch) < 0.005f && std::abs (target) < 0.005f;
        steady = atZero ? steady + 1 : 0;
        const int xfBase = std::max (32, (int) (0.012 * sr));
        const double searchRange = std::min (grainSamples * 0.25, 0.0125 * sr);

        if (xfLen == 0)
        {
            if (direct)
            {
                if (! atZero)
                {
                    if (slope < 0.0)
                    {
                        // Pitch up needs sound from the past: jump back by half a grain.
                        const double to = bestLag (0.0, grainSamples * 0.5, searchRange);
                        startCrossfade (to, std::min (xfBase, (int) (0.5 * (to - minLag) / -slope)));
                    }
                    else
                    {
                        direct = false; // pitch down: the lag simply grows from zero
                        lag = 0.0;
                    }
                }
            }
            else if (steady > (int) (0.004 * sr))
            {
                // Back at the original pitch: return to the live sound.
                oldLag = lag;
                oldDirect = false;
                lag = 0.0;
                direct = true;
                xfLen = xfBase;
                xfPos = 0;
            }
            else if (slope < 0.0 && lag + slope * xfBase < minLag)
            {
                const double to = bestLag (lag, lag + grainSamples * 0.5, searchRange);
                startCrossfade (to, std::min (xfBase, (int) (0.5 * (to - minLag) / -slope)));
            }
            else if (slope > 0.0 && lag > grainSamples)
            {
                const double to = bestLag (lag, lag - grainSamples * 0.5, searchRange);
                startCrossfade (to, xfBase);
            }
        }

        if (! direct) lag = std::clamp (lag + slope, 0.0, (double) mask - 8.0);
        float l = direct ? buf[0][(size_t) ((writePos - 1) & mask)] : readAt (buf[0], lag);
        float r = direct ? buf[1][(size_t) ((writePos - 1) & mask)] : readAt (buf[1], lag);

        if (xfLen > 0)
        {
            if (! oldDirect) oldLag = std::clamp (oldLag + slope, 0.0, (double) mask - 8.0);
            const float ol = oldDirect ? buf[0][(size_t) ((writePos - 1) & mask)] : readAt (buf[0], oldLag);
            const float orr = oldDirect ? buf[1][(size_t) ((writePos - 1) & mask)] : readAt (buf[1], oldLag);
            const float x = (float) ++xfPos / (float) xfLen;
            // Raised cosine: smooth start and end of the splice.
            const float g = 0.5f - 0.5f * std::cos ((float) pi * x);
            l = ol + (l - ol) * g;
            r = orr + (r - orr) * g;
            if (xfPos >= xfLen) xfLen = xfPos = 0;
        }
        outL = sanitize (l);
        outR = sanitize (r);
    }

    enum Kind { hold, travel, outAndBack };

    static constexpr double minLag = 3.0;

    Params params;
    double sr = 48000.0;
    std::array<std::vector<float>, 2> buf;
    std::vector<float> mono;
    int mask = 0, writePos = 0;

    bool direct = true, oldDirect = false;
    double lag = 0.0, oldLag = 0.0;
    int xfPos = 0, xfLen = 0, steady = 0;

    int64_t lastStep = INT64_MIN;
    int step = -1;
    Kind kind = hold;
    double rampPos = 1.0, rampInc = 0.0;
    float rampFrom = 0.0f, rampTo = 0.0f, held = 0.0f;
    float stepPitch = 0.0f, pitch = 0.0f, target = 0.0f, pitchCoeff = 0.0f;
    double divePos = 0.0;
    float diveOffset = 0.0f;
    Smoother mixSmooth;
};

} // namespace originpsy
