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
        // Hand-drawn slow down instead of the Curve knob: numDrawnPoints speeds
        // (1 = full speed) from the start of the slow down to its end. Spin-ups
        // play it backwards. Null = use the knob.
        const float* drawn = nullptr;
        // Extra effects heard only while the tape is stopping, stopped or
        // spinning up; they cut out the moment the stop ends. 0 = off.
        float drive = 0.0f;     // tape saturation
        float crush = 0.0f;     // fewer bits and a lower sample rate
        float filter = 0.0f;    // low pass that closes as the tape slows
        float wobble = 0.0f;    // wow and flutter: the pitch wobbles like a worn tape
    };

    static constexpr int numDrawnPoints = 64;

    /** The knob's slow-down shape as speeds 0..1 over the length, so a drawn
        curve can start from it. */
    static void fillDrawnFromKnob (float curve, float* points) noexcept
    {
        const double shape = 0.35 + 2.65 * std::clamp ((double) curve, 0.0, 1.0);
        for (int i = 0; i < numDrawnPoints; ++i)
            points[i] = (float) (1.0 - std::pow ((double) i / (numDrawnPoints - 1), shape));
    }

    /** Drawn speed at a position 0..1 of the slow down. */
    static float drawnAt (const float* points, double x) noexcept
    {
        const double pos = std::clamp (x, 0.0, 1.0) * (numDrawnPoints - 1);
        const int i = std::min ((int) pos, numDrawnPoints - 2);
        const double frac = pos - i;
        return std::clamp ((float) (points[i] + (points[i + 1] - points[i]) * frac), 0.0f, 1.0f);
    }

    static constexpr double maxLagSeconds = 12.0;

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        for (auto& d : lines) d.prepare ((int) (sampleRate * maxLagSeconds) + 16);
        xfadeSamples = std::max (1, (int) (0.02 * sr));
        mixSmooth.prepare (sr, 0.02);
        fxGateSmooth.prepare (sr, 0.003);
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
        fxGateSmooth.reset (0.0f);
        lp1 = lp2 = {};
        crushHold = {};
        crushCount = 0;
        wowPhase = flutterPhase = 0.0;
    }

    void setParams (const Params& p) noexcept { params = p; }

    /** Playback speed now, 0..1 (for the display). */
    float getSpeed() const noexcept { return speed; }
    bool isActive() const noexcept { return stage != live; }
    /** How much of the extra effects is heard now, 0..1 (for the display). */
    float getFxAmount() const noexcept { return fxGateSmooth.current(); }

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
        const bool anyFx = params.drive > 0.001f || params.crush > 0.001f || params.filter > 0.001f;
        const float driveGain = 1.0f + 24.0f * params.drive * params.drive;
        const float driveNorm = 1.0f / std::tanh (driveGain);
        const float crushSteps = std::pow (2.0f, 15.0f - 11.0f * std::clamp (params.crush, 0.0f, 1.0f));
        const int crushHoldLen = 1 + (int) (std::clamp (params.crush, 0.0f, 1.0f) * 15.0f * (float) (sr / 48000.0));

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
                    speed = holdSpeed + (1.0f - holdSpeed)
                                * (params.drawn != nullptr ? drawnAt (params.drawn, rampPos)
                                                           : (float) (1.0 - std::pow (rampPos, shape)));
                    if (rampPos >= 1.0) stage = held;
                    break;
                case held: speed = holdSpeed + (1.0f - holdSpeed) * drawnEnd(); break;
                case spinningUp:
                    rampPos = std::min (1.0, rampPos + rampStep);
                    speed = holdSpeed + (1.0f - holdSpeed)
                                * (params.drawn != nullptr ? drawnAt (params.drawn, 1.0 - rampPos)
                                                           : (float) std::pow (rampPos, 1.0 / shape));
                    if (rampPos >= 1.0) { speed = 1.0f; stage = catchingUp; xfade = 0; }
                    break;
                case catchingUp:
                case live: speed = 1.0f; break;
            }

            const float inL = left[n], inR = right[n];
            lines[0].push (inL);
            lines[1].push (inR);

            // Extra effects, only while the stop is happening.
            const float fxGate = fxGateSmooth.next (stage == slowing || stage == held || stage == spinningUp ? 1.0f : 0.0f);

            float wetL = inL, wetR = inR;
            if (stage != live)
            {
                lag = std::min (maxLag, lag + (1.0 - speed));
                double d = 1.0 + lag;
                if (params.wobble > 0.001f)
                {
                    // Slow wow plus fast flutter, up to about 6 ms of tape movement.
                    wowPhase += 0.9 / sr;
                    flutterPhase += 7.0 / sr;
                    wowPhase -= std::floor (wowPhase);
                    flutterPhase -= std::floor (flutterPhase);
                    const double wob = 0.75 * (1.0 - std::cos (twoPi * wowPhase)) + 0.25 * (1.0 - std::cos (twoPi * flutterPhase));
                    d += wob * 0.003 * sr * params.wobble * fxGate;
                }
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

            if (anyFx && fxGate > 0.0001f)
            {
                float fx[2] = { wetL, wetR };
                // Low pass from about 18 kHz down to 200 Hz as the speed falls.
                const double cutoff = 18000.0 * std::pow (200.0 / 18000.0, (double) params.filter * (1.0 - (double) speed));
                const float a = (float) (1.0 - std::exp (-twoPi * std::min (cutoff, 0.45 * sr) / sr));
                for (int ch = 0; ch < 2; ++ch)
                {
                    float x = fx[ch];
                    if (params.drive > 0.001f) x = std::tanh (x * driveGain) * driveNorm;
                    if (params.crush > 0.001f)
                    {
                        if (crushCount == 0) crushHold[(size_t) ch] = std::round (x * crushSteps) / crushSteps;
                        x = crushHold[(size_t) ch];
                    }
                    if (params.filter > 0.001f)
                    {
                        lp1[(size_t) ch] += a * (x - lp1[(size_t) ch]);
                        lp2[(size_t) ch] += a * (lp1[(size_t) ch] - lp2[(size_t) ch]);
                        x = lp2[(size_t) ch];
                    }
                    fx[ch] = x;
                }
                crushCount = (crushCount + 1) % crushHoldLen;
                wetL += (fx[0] - wetL) * fxGate;
                wetR += (fx[1] - wetR) * fxGate;
            }
            else
            {
                // Keep the filter following the signal so it starts clean next time.
                lp1 = lp2 = { wetL, wetR };
                crushCount = 0;
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
                wowPhase = flutterPhase = 0.0;
                rampPos = 0.0;
                speed = 0.0f;
            }
            else if (stage == live || stage == catchingUp)
            {
                if (stage == live) wowPhase = flutterPhase = 0.0; // wobble starts from the centre
                if (stage == catchingUp) lag = 0.0;
                stage = slowing;
                rampPos = 0.0;
            }
            else if (stage == spinningUp)
            {
                // Pressed again while spinning up: slow down from the speed it has now.
                stage = slowing;
                rampPos = params.drawn != nullptr ? 1.0 - rampPos : speedToRamp (speed);
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
                if (params.drawn != nullptr)
                {
                    // The spin-up plays the drawing backwards: continue from the same point.
                    rampPos = 1.0 - rampPos;
                }
                else
                {
                    const float holdSpeed = params.mode == slowDown ? 0.5f : 0.0f;
                    const double shape = 0.35 + 2.65 * std::clamp ((double) params.curve, 0.0, 1.0);
                    const double frac = std::clamp ((double) ((speed - holdSpeed) / (1.0f - holdSpeed)), 0.0, 1.0);
                    rampPos = std::pow (frac, shape);
                }
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

    float drawnEnd() const noexcept { return params.drawn != nullptr ? drawnAt (params.drawn, 1.0) : 0.0f; }

    Params params;
    double sr = 48000.0;
    std::array<DelayLine, 2> lines;
    Stage stage = live;
    double lag = 0.0, rampPos = 0.0;
    float speed = 1.0f;
    int xfade = 0, xfadeSamples = 960;
    bool engaged = false;
    Smoother mixSmooth;

    /** Smoother with the target given per sample. */
    struct Gate
    {
        void prepare (double sampleRate, double seconds) noexcept { coeff = (float) std::exp (-1.0 / (seconds * sampleRate)); }
        void reset (float v) noexcept { value = v; }
        float next (float target) noexcept { value = target + coeff * (value - target); return value; }
        float current() const noexcept { return value; }
        float coeff = 0.0f, value = 0.0f;
    } fxGateSmooth;
    std::array<float, 2> lp1 {}, lp2 {}, crushHold {};
    int crushCount = 0;
    double wowPhase = 0.0, flutterPhase = 0.0;
};

} // namespace originpsy
