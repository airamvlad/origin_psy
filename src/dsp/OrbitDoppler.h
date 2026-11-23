#pragma once

#include "Common.h"
#include "DelayLine.h"
#include "Filters.h"
#include "Modulation.h"

#include <array>

namespace originpsy
{

/** The sound moves around the listener along a path, locked to the tempo.
    Distance to each ear sets a delay, so the pitch really rises as the sound
    comes closer and falls as it moves away (Doppler). The far ear is quieter
    and darker (head shadow), and distance makes the sound quieter and duller. */
class OrbitDoppler
{
public:
    enum Path { circle = 0, figureEight, spiral, flyBy };
    static constexpr int numPaths = 4;

    struct Params
    {
        int path = circle;
        int rate = 6;               // index into Lfo::rates (1 bar per lap)
        float radius = 6.0f;        // metres
        float doppler = 0.5f;       // 0 = no pitch change, 0.5 = real speed of sound, 1 = twice as strong
        float distanceTone = 0.5f;  // how much darker the sound gets far away
        float width = 0.8f;         // how far left and right it goes
        float mix = 0.8f;
    };

    static constexpr double speedOfSound = 343.0;
    static constexpr double earOffset = 0.09; // metres from the centre of the head
    static constexpr double maxDelaySeconds = 0.75;

    struct Point { float x, y; }; // metres; listener at 0,0 facing +y

    /** Position on a path at a phase 0..1, in parts of the radius. The paths
        are off centre, so the distance to the listener changes and the Doppler
        shift can be heard (a circle around the head would never get closer). */
    static Point pathPoint (int path, double phase) noexcept
    {
        const float a = (float) (twoPi * phase);
        switch (path)
        {
            case figureEight:
                // Crosses just in front of the listener, loops left and right.
                return { 1.3f * std::sin (a), 0.15f + 0.9f * std::sin (a) * std::cos (a) };
            case spiral:
            {
                // Three turns per lap, swinging in close and back out.
                const float r = 0.25f + 0.75f * (0.5f - 0.5f * std::cos (a));
                return { r * std::sin (3.0f * a), r * std::cos (3.0f * a) + 0.2f };
            }
            case flyBy:
            {
                // Flies past in front from left to right and back.
                const float tri = (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
                const float x = 2.2f * std::sin (0.5f * (float) pi * tri);
                return { x, 0.35f };
            }
            case circle:
            default:
                return { std::sin (a), 0.45f + std::cos (a) };
        }
    }

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        for (auto& d : lines) d.prepare ((int) (sampleRate * maxDelaySeconds) + 8);
        for (auto& s : delaySmooth) s.prepare (sr, 0.01);
        mixSmooth.prepare (sr, 0.02);
        reset();
    }

    void reset()
    {
        for (auto& d : lines) d.reset();
        for (auto& f : shadow) f.reset();
        for (auto& f : air) f.reset();
        delayInit = false;
        mixSmooth.reset (params.mix);
        gainL = gainR = 0.707f;
        position = { 0.0f, 1.0f };
    }

    void setParams (const Params& p) noexcept { params = p; }

    /** Where the sound is now, in parts of the radius (for the display). */
    Point getPosition() const noexcept { return position; }

    void process (float* left, float* right, int numSamples, const Transport& t) noexcept
    {
        mixSmooth.setTarget (std::clamp (params.mix, 0.0f, 1.0f));
        const double lap = Lfo::rateQuarters (params.rate);
        const double beatsPerSample = 1.0 / t.samplesPerBeat (sr);
        const float radius = std::clamp (params.radius, 0.5f, 40.0f);
        const double dopplerScale = 2.0 * std::clamp ((double) params.doppler, 0.0, 1.0);

        // Geometry is updated every 16 samples; delays and gains are smoothed in between.
        constexpr int chunk = 16;
        for (int start = 0; start < numSamples; start += chunk)
        {
            const int end = std::min (numSamples, start + chunk);
            const double ppq = t.ppqAtBlockStart + start * beatsPerSample;
            const double phase = ppq / lap - std::floor (ppq / lap);
            const auto p = pathPoint (params.path, phase);
            position = p;

            const double x = p.x * radius * std::clamp (params.width, 0.0f, 1.0f);
            const double y = p.y * radius;
            const double d = std::max (0.3, std::sqrt (x * x + y * y));
            const double dL = std::sqrt ((x + earOffset) * (x + earOffset) + y * y);
            const double dR = std::sqrt ((x - earOffset) * (x - earOffset) + y * y);

            // Common delay (Doppler) plus the small difference between the ears.
            const double common = d / speedOfSound * dopplerScale;
            const double targetL = std::clamp ((common + (dL - d) / speedOfSound + 0.0005) * sr, 2.0, sr * maxDelaySeconds - 4.0);
            const double targetR = std::clamp ((common + (dR - d) / speedOfSound + 0.0005) * sr, 2.0, sr * maxDelaySeconds - 4.0);
            if (! delayInit)
            {
                delaySmooth[0].reset ((float) targetL);
                delaySmooth[1].reset ((float) targetR);
                delayInit = true;
            }
            delaySmooth[0].setTarget ((float) targetL);
            delaySmooth[1].setTarget ((float) targetR);

            // Level and head shadow from the direction; distance from the radius.
            const double side = x / d;          // -1 left .. 1 right
            const double behind = y < 0.0 ? std::min (1.0, -y / d) : 0.0;
            const float angle = (float) ((side + 1.0) * 0.25 * pi);
            const float rel = (float) (d / radius);
            const float distGain = std::clamp (1.0f / (0.5f + 0.5f * rel), 0.35f, 1.6f);
            const float targetGainL = std::cos (angle) * 1.2f * distGain;
            const float targetGainR = std::sin (angle) * 1.2f * distGain;
            const double shadowL = 18000.0 * std::pow (0.12, std::max (0.0, side)) * (1.0 - 0.4 * behind);
            const double shadowR = 18000.0 * std::pow (0.12, std::max (0.0, -side)) * (1.0 - 0.4 * behind);
            shadow[0].setCutoff (shadowL, sr);
            shadow[1].setCutoff (shadowR, sr);
            const double far = std::clamp ((rel - 0.3) / 1.5, 0.0, 1.0);
            const double airHz = 20000.0 * std::pow (0.06, std::clamp ((double) params.distanceTone, 0.0, 1.0) * far);
            air[0].setCutoff (airHz, sr);
            air[1].setCutoff (airHz, sr);

            const float gStepL = (targetGainL - gainL) / (float) (end - start);
            const float gStepR = (targetGainR - gainR) / (float) (end - start);
            for (int n = start; n < end; ++n)
            {
                const float inL = left[n], inR = right[n];
                lines[0].push (inL);
                lines[1].push (inR);
                gainL += gStepL;
                gainR += gStepR;
                // The source is a point: both ears hear the mono sum, the original
                // stereo stays as a little width around it.
                const float srcL = lines[0].read (delaySmooth[0].next());
                const float srcR = lines[1].read (delaySmooth[1].next());
                const float mono = 0.5f * (srcL + srcR);
                const float wetL = air[0].process (shadow[0].process (0.8f * mono + 0.2f * srcL)) * gainL;
                const float wetR = air[1].process (shadow[1].process (0.8f * mono + 0.2f * srcR)) * gainR;
                const float m = mixSmooth.next();
                left[n] = inL * (1.0f - m) + wetL * m;
                right[n] = inR * (1.0f - m) + wetR * m;
            }
            gainL = targetGainL;
            gainR = targetGainR;
        }
    }

private:
    Params params;
    double sr = 48000.0;
    std::array<DelayLine, 2> lines;
    std::array<Smoother, 2> delaySmooth;
    std::array<OnePole, 2> shadow, air;
    Smoother mixSmooth;
    float gainL = 0.707f, gainR = 0.707f;
    bool delayInit = false;
    Point position { 0.0f, 1.0f };
};

} // namespace originpsy
