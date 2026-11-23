#pragma once

#include "Common.h"
#include "DelayLine.h"

#include <array>

namespace originpsy
{

/** Tape stop and tape start: the sound slows down like a tape machine that
    loses power (pitch and tempo fall together), or spins up from a stop.
    Fired by the Brake button (hold it, or automate it) or by the song
    position, so it lands exactly on the bar line. */
class TapeStop
{
public:
    enum Mode { stop = 0, stopStart, slowDown, start };
    static constexpr int numModes = 4;
    enum Trigger { manual = 0, every2, every4, every8, every16 };
    static constexpr int numTriggers = 5;

    struct Length { const char* name; double quarters; };
    static constexpr std::array<Length, 6> lengths { {
        { "1/16", 0.25 }, { "1/8", 0.5 }, { "1/4", 1.0 }, { "1/2", 2.0 }, { "1 bar", 4.0 }, { "2 bars", 8.0 },
    } };

    struct Params
    {
        int mode = stop;
        int trigger = manual;
        bool brake = false;     // the Brake button (manual trigger)
        int length = 3;         // index into lengths: how long the slow down takes
        float curve = 0.5f;     // 0 = falls fast then lingers, 1 = holds speed then drops late
        float mix = 1.0f;
    };

    static constexpr double maxLagSeconds = 12.0;

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        for (auto& d : lines) d.prepare ((int) (sampleRate * maxLagSeconds) + 16);
        xfadeSamples = std::max (1, (int) (0.02 * sr));
        mixSmooth.prepare (sr, 0.02);
        reset();
    }

    void reset()
    {
        for (auto& d : lines) d.reset();
        stage = live;
        lag = 0.0;
        speed = 1.0f;
        rampPos = 0.0;
        xfade = 0;
        engaged = false;
        mixSmooth.reset (params.mix);
    }

    void setParams (const Params& p) noexcept { params = p; }

    /** Playback speed now, 0..1 (for the display). */
    float getSpeed() const noexcept { return speed; }
    bool isActive() const noexcept { return stage != live; }

    /** Whether the song position asks for the effect (auto triggers). */
    static bool autoEngaged (const Params& p, double ppq, double beatsPerBar) noexcept
    {
        static constexpr int bars[] = { 0, 2, 4, 8, 16 };
        const int t = std::clamp (p.trigger, 0, numTriggers - 1);
        if (t == manual) return false;
        const double period = bars[t] * std::max (0.5, beatsPerBar);
        const double len = std::min (lengthQuarters (p.length), period * 0.5);
        const double pos = ppq - std::floor (ppq / period) * period;
        // Start spins up after the bar line; the others stop just before it.
        return p.mode == start ? pos < len : pos >= period - len;
    }

    static double lengthQuarters (int index) noexcept
    {
        return lengths[(size_t) std::clamp (index, 0, (int) lengths.size() - 1)].quarters;
    }

    void process (float* left, float* right, int numSamples, const Transport& t) noexcept
    {
        const double spb = t.samplesPerBeat (sr);
        const double rampSamples = std::max (64.0, lengthQuarters (params.length) * spb);
        const double rampStep = 1.0 / rampSamples;
        const double shape = 0.35 + 2.65 * std::clamp ((double) params.curve, 0.0, 1.0);
        const float holdSpeed = params.mode == slowDown ? 0.5f : 0.0f;
        const double maxLag = sr * maxLagSeconds - 8.0;
        mixSmooth.setTarget (std::clamp (params.mix, 0.0f, 1.0f));

        for (int n = 0; n < numSamples; ++n)
        {
            const bool want = params.trigger == manual
                                  ? params.brake
                                  : autoEngaged (params, t.ppqAtBlockStart + n / spb, t.beatsPerBar);
            if (want != engaged)
            {
                engaged = want;
                onEdge (want);
            }

            // Speed from the stage.
            switch (stage)
            {
                case slowing:
                    rampPos = std::min (1.0, rampPos + rampStep);
                    speed = holdSpeed + (1.0f - holdSpeed) * (float) (1.0 - std::pow (rampPos, shape));
                    if (rampPos >= 1.0) stage = held;
                    break;
                case held: speed = holdSpeed; break;
                case spinningUp:
                    rampPos = std::min (1.0, rampPos + rampStep);
                    speed = holdSpeed + (1.0f - holdSpeed) * (float) std::pow (rampPos, 1.0 / shape);
                    if (rampPos >= 1.0) { speed = 1.0f; stage = catchingUp; xfade = 0; }
                    break;
                case catchingUp:
                case live: speed = 1.0f; break;
            }

            const float inL = left[n], inR = right[n];
            lines[0].push (inL);
            lines[1].push (inR);

            float wetL = inL, wetR = inR;
            if (stage != live)
            {
                lag = std::min (maxLag, lag + (1.0 - speed));
                const double d = 1.0 + lag;
                // Very slow tape fades out instead of holding a DC level.
                const float g = std::min (1.0f, speed * 12.0f);
                wetL = lines[0].read (d) * g;
                wetR = lines[1].read (d) * g;
                if (stage == catchingUp)
                {
                    // Back to the live signal with a short crossfade.
                    const float x = (float) ++xfade / (float) xfadeSamples;
                    wetL = wetL * (1.0f - x) + inL * x;
                    wetR = wetR * (1.0f - x) + inR * x;
                    if (xfade >= xfadeSamples) { stage = live; lag = 0.0; }
                }
            }

            const float m = mixSmooth.next();
            left[n] = inL * (1.0f - m) + wetL * m;
            right[n] = inR * (1.0f - m) + wetR * m;
        }
    }

private:
    enum Stage { live, slowing, held, spinningUp, catchingUp };

    void onEdge (bool on) noexcept
    {
        if (on)
        {
            if (params.mode == start)
            {
                // Cut to a stop and spin up from there.
                stage = spinningUp;
                rampPos = 0.0;
                speed = 0.0f;
            }
            else if (stage == live || stage == catchingUp)
            {
                if (stage == catchingUp) lag = 0.0;
                stage = slowing;
                rampPos = 0.0;
            }
            else if (stage == spinningUp)
            {
                // Pressed again while spinning up: slow down from the speed it has now.
                stage = slowing;
                rampPos = speedToRamp (speed);
            }
            return;
        }

        if (params.mode == start) return; // Start runs to the end by itself
        if (stage == slowing || stage == held)
        {
            if (params.mode == stop)
            {
                stage = catchingUp;  // straight back to the live sound
                xfade = 0;
            }
            else
            {
                // Spin up from the speed it has now.
                const float holdSpeed = params.mode == slowDown ? 0.5f : 0.0f;
                const double shape = 0.35 + 2.65 * std::clamp ((double) params.curve, 0.0, 1.0);
                const double frac = std::clamp ((double) ((speed - holdSpeed) / (1.0f - holdSpeed)), 0.0, 1.0);
                rampPos = std::pow (frac, shape);
                stage = spinningUp;
            }
        }
    }

    /** Ramp position of the slow down that gives a speed (to continue smoothly). */
    double speedToRamp (float s) const noexcept
    {
        const float holdSpeed = params.mode == slowDown ? 0.5f : 0.0f;
        const double shape = 0.35 + 2.65 * std::clamp ((double) params.curve, 0.0, 1.0);
        const double frac = std::clamp ((double) (1.0f - (s - holdSpeed) / (1.0f - holdSpeed)), 0.0, 1.0);
        return std::pow (frac, 1.0 / shape);
    }

    Params params;
    double sr = 48000.0;
    std::array<DelayLine, 2> lines;
    Stage stage = live;
    double lag = 0.0, rampPos = 0.0;
    float speed = 1.0f;
    int xfade = 0, xfadeSamples = 960;
    bool engaged = false;
    Smoother mixSmooth;
};

} // namespace originpsy
