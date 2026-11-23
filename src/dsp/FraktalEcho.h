#pragma once

#include "DelayLine.h"
#include "Filters.h"
#include "Scales.h"

#include <array>

namespace originpsy
{

/** Fraktal Echo: a delay whose echoes branch like a tree.

    Every echo continues at the main stride and also sprouts a shorter
    branch (stride x ratio). Each sprout moves the pitch by a number of scale
    degrees, so the tail turns into a melody in the key instead of a copy of
    the input that fades away.

    All taps read from one stereo buffer; a tap that changes pitch uses two
    crossfaded read heads that slide through a short window (a granular
    pitch shifter without extra buffers). */
class FraktalEcho
{
public:
    static constexpr int maxGenerations = 4;
    static constexpr int maxTaps = (1 << maxGenerations) - 1; // 15
    static constexpr double maxStrideSeconds = 3.0;

    struct Params
    {
        int division = 5;          // index into noteDivisions (1/8)
        float ratio = 0.618f;      // sprout stride / main stride
        int generations = 3;       // 1..4
        int scale = 0;             // index into scales
        int spread = 2;            // scale degrees per sprout (-7..7)
        float feedback = 0.35f;    // 0..0.95, whole tree fed back
        float damp = 0.4f;         // 0..1, darker repeats
        float width = 0.7f;        // 0..1, stereo spread of the branches
        float mix = 0.35f;         // 0..1
    };

    struct Tap
    {
        double delayBeats = 0.0;  // in quarter notes
        int degrees = 0;          // scale degrees from the input
        int generation = 1;
        float pan = 0.0f;         // -1..1
    };

    FraktalEcho() { setParams (params); }

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        // Longest tree: 4 strides of at most 3 s, plus the grain window.
        const int maxSamples = (int) (sampleRate * (4.0 * maxStrideSeconds + 0.5));
        for (auto& d : lines) d.prepare (maxSamples);
        mixSmooth.prepare (sr, 0.02);
        fbSmooth.prepare (sr, 0.02);
        reset();
    }

    void reset()
    {
        for (auto& d : lines) d.reset();
        for (auto& v : voices) v = {};
        for (auto& f : dampL) f.reset();
        for (auto& f : hpFb) f.reset();
        fbL = fbR = 0.0f;
        mixSmooth.reset (params.mix);
        fbSmooth.reset (params.feedback);
        stride.reset ((float) (sr * 0.25));
        stride.prepare (sr, 0.15);
        strideInit = false;
    }

    void setParams (const Params& p) noexcept
    {
        params = p;
        params.generations = std::clamp (p.generations, 1, maxGenerations);
        params.ratio = std::clamp (p.ratio, 0.25f, 0.9f);
        params.feedback = std::clamp (p.feedback, 0.0f, 0.95f);
        numTaps = buildTree (params, taps);
    }

    /** The current tree, for drawing in the editor. */
    int getTaps (std::array<Tap, maxTaps>& out) const noexcept
    {
        out = taps;
        return numTaps;
    }

    /** Builds the tree: tap n has children 2n+1 (continue) and 2n+2 (sprout). */
    static int buildTree (const Params& p, std::array<Tap, maxTaps>& out) noexcept
    {
        const int count = (1 << p.generations) - 1;
        out[0] = { 1.0, 0, 1, 0.0f };
        for (int n = 0; n < count; ++n)
        {
            const auto parent = out[(size_t) n];
            const double childStride = std::pow ((double) p.ratio, (double) parent.generation);
            const int c = 2 * n + 1, s = 2 * n + 2;
            if (c < count)
                out[(size_t) c] = { parent.delayBeats + 1.0, parent.degrees, parent.generation + 1,
                                    parent.pan * 0.6f };
            if (s < count)
            {
                const float side = (n % 2 == 0) ? -1.0f : 1.0f;
                out[(size_t) s] = { parent.delayBeats + childStride, parent.degrees + p.spread,
                                    parent.generation + 1,
                                    std::clamp (parent.pan + side * 0.5f, -1.0f, 1.0f) };
            }
        }
        // delayBeats above is in strides; the caller scales it by the division.
        return count;
    }

    void process (float* left, float* right, int numSamples, const Transport& t) noexcept
    {
        const double strideSamples = std::clamp (divisionQuarters (params.division) * t.samplesPerBeat (sr),
                                                 64.0, sr * maxStrideSeconds);
        if (! strideInit) { stride.reset ((float) strideSamples); strideInit = true; }
        stride.setTarget ((float) strideSamples);

        const double dampHz = 18000.0 * std::pow (0.04, (double) params.damp);
        for (auto& f : dampL) f.setCutoff (dampHz, sr);
        for (auto& f : hpFb) f.setCutoff (120.0, sr);

        mixSmooth.setTarget (params.mix);
        fbSmooth.setTarget (params.feedback);

        // Per tap gain: later generations quieter, normalised so the sum of a
        // full tree stays near unity.
        std::array<float, maxTaps> gain {}, ratio {}, panL {}, panR {};
        float total = 0.0f;
        for (int i = 0; i < numTaps; ++i)
        {
            gain[(size_t) i] = std::pow (0.72f, (float) (taps[(size_t) i].generation - 1));
            total += gain[(size_t) i];
        }
        const float norm = 1.0f / std::sqrt (std::max (1.0f, total));
        for (int i = 0; i < numTaps; ++i)
        {
            const auto& tap = taps[(size_t) i];
            gain[(size_t) i] *= norm;
            ratio[(size_t) i] = semitonesToRatio ((float) degreesToSemitones (params.scale, tap.degrees));
            const float pan = std::clamp (tap.pan * params.width, -1.0f, 1.0f);
            const float angle = (pan + 1.0f) * 0.25f * (float) pi;
            panL[(size_t) i] = std::cos (angle) * 1.41421356f;
            panR[(size_t) i] = std::sin (angle) * 1.41421356f;
        }

        const double window = sr * 0.07; // grain window of the pitch shifter

        for (int n = 0; n < numSamples; ++n)
        {
            const double s = stride.next();
            const float inL = left[n], inR = right[n];
            const float fb = fbSmooth.next();

            lines[0].push (sanitize (inL + fbL * fb));
            lines[1].push (sanitize (inR + fbR * fb));

            float outL = 0.0f, outR = 0.0f;
            for (int i = 0; i < numTaps; ++i)
            {
                const double d = taps[(size_t) i].delayBeats * s;
                auto& v = voices[(size_t) i];
                float l, r;
                if (std::abs (ratio[(size_t) i] - 1.0f) < 1.0e-4f)
                {
                    l = lines[0].read (d);
                    r = lines[1].read (d);
                }
                else
                {
                    // Two heads half a window apart, sliding at (1 - ratio).
                    v.phase += (1.0 - ratio[(size_t) i]) / window;
                    v.phase -= std::floor (v.phase);
                    const double p2 = v.phase + 0.5 - std::floor (v.phase + 0.5);
                    const float w1 = (float) std::sin (pi * v.phase);
                    const float w2 = (float) std::sin (pi * p2);
                    const double d1 = d + v.phase * window, d2 = d + p2 * window;
                    l = lines[0].read (d1) * w1 * w1 + lines[0].read (d2) * w2 * w2;
                    r = lines[1].read (d1) * w1 * w1 + lines[1].read (d2) * w2 * w2;
                }
                const float mono = 0.5f * (l + r);
                // Branches keep a bit of the original stereo and move by pan.
                const float g = gain[(size_t) i];
                outL += g * (0.5f * l + 0.5f * mono * panL[(size_t) i]);
                outR += g * (0.5f * r + 0.5f * mono * panR[(size_t) i]);
            }

            // Feedback path: damped, low-cut, soft-limited.
            fbL = std::tanh (hpHigh (hpFb[0], dampL[0].process (outL)));
            fbR = std::tanh (hpHigh (hpFb[1], dampL[1].process (outR)));

            const float m = mixSmooth.next();
            left[n] = inL * (1.0f - m) + outL * m;
            right[n] = inR * (1.0f - m) + outR * m;
        }
    }

private:
    // One-pole high-pass from a one-pole low-pass.
    static float hpHigh (OnePole& lp, float x) noexcept { return x - lp.process (x); }

    struct Voice { double phase = 0.0; };

    double sr = 48000.0;
    Params params;
    std::array<Tap, maxTaps> taps {};
    int numTaps = 7;
    std::array<Voice, maxTaps> voices {};
    std::array<DelayLine, 2> lines;
    std::array<OnePole, 2> dampL, hpFb;
    float fbL = 0.0f, fbR = 0.0f;
    Smoother mixSmooth, fbSmooth, stride;
    bool strideInit = false;
};

} // namespace originpsy
