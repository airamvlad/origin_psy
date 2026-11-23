#pragma once

#include "Common.h"
#include "Fft.h"
#include "Scales.h"

#include <array>
#include <complex>
#include <vector>

namespace originpsy
{

/** On the freeze steps of a pattern, holds the spectrum of the sound and
    repeats it, each repeat moved by another interval and/or with parts of
    the spectrum masked out. Pass steps let the sound through. The result is
    a stutter made of the colour of the sound rather than chopped audio.

    STFT: 2048 point FFT, hop 512, Hann windows. Latency = 2048 samples. */
class SpectralRatchet
{
public:
    static constexpr int order = 11;
    static constexpr int fftSize = 1 << order;
    static constexpr int hop = fftSize / 4;
    static constexpr int numBins = fftSize / 2 + 1;
    static constexpr int latency = fftSize;
    static constexpr int maxSteps = 16;
    static constexpr int numBands = 48; // spectrum summary for the display

    enum Interval { same = 0, octaveUp, octaveDown, fifthsUp, scaleUp, scaleDown, randomScale };
    static constexpr int numIntervals = 7;
    enum Mask { noMask = 0, combA, combB, alternate, lowsOnly, highsOnly, walk };
    static constexpr int numMasks = 7;

    struct Params
    {
        int division = 2;              // index into noteDivisions (1/16)
        int steps = 8;                 // 1..16
        uint32_t pattern = 0xEEEEu;    // bit set = freeze on that step
        int interval = scaleUp;
        int scale = 0;
        int mask = noMask;
        float smear = 0.15f;           // random phase: 0 = glassy, 1 = washed out
        float tilt = 0.0f;             // -1..1, darker or brighter repeats (6 dB/oct per unit)
        float mix = 1.0f;
    };

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        fft.prepare (order);
        window.resize ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (twoPi * i / fftSize);
        frame.resize ((size_t) fftSize);
        for (auto& c : ch)
        {
            c.input.assign ((size_t) fftSize, 0.0f);
            c.output.assign ((size_t) fftSize * 2, 0.0f);
            c.dry.assign ((size_t) latency, 0.0f);
            for (auto* v : { &c.phase, &c.prevPhase, &c.frozenMag, &c.frozenDelta, &c.outPhase, &c.mag })
                v->assign ((size_t) numBins, 0.0f);
        }
        mixSmooth.prepare (sr, 0.02);
        reset();
    }

    void reset()
    {
        for (auto& c : ch)
        {
            std::fill (c.input.begin(), c.input.end(), 0.0f);
            std::fill (c.output.begin(), c.output.end(), 0.0f);
            std::fill (c.dry.begin(), c.dry.end(), 0.0f);
            for (auto* v : { &c.phase, &c.prevPhase, &c.frozenMag, &c.frozenDelta, &c.outPhase, &c.mag })
                std::fill (v->begin(), v->end(), 0.0f);
        }
        inPos = outPos = dryPos = 0;
        hopCount = 0;
        lastStep = -1;
        freezing = false;
        repeat = 0;
        currentStep = -1;
        random = 12345u;
        mixSmooth.reset (params.mix);
        for (auto& b : bands) b = 0.0f;
    }

    void setParams (const Params& p) noexcept { params = p; }

    /** Pattern step playing now (-1 before the first frame) and whether it freezes. */
    int getStep() const noexcept { return currentStep; }
    bool isFreezing() const noexcept { return freezing; }
    /** Level per band of what it plays (0..1), for the display. */
    const std::array<float, numBands>& getBands() const noexcept { return bands; }

    /** Semitones of a repeat (0 = the first repeat, as captured). */
    static int intervalFor (const Params& p, int repeatIndex) noexcept
    {
        const int r = std::max (0, repeatIndex);
        switch (p.interval)
        {
            case octaveUp:   return std::min (24, 12 * (r % 3));
            case octaveDown: return -std::min (24, 12 * (r % 3));
            case fifthsUp:
            {
                static constexpr int f[] = { 0, 7, 12, 19 };
                return f[r % 4];
            }
            case scaleUp:    return degreesToSemitones (p.scale, r % 8);
            case scaleDown:  return -degreesToSemitones (p.scale, r % 8);
            case randomScale:
            {
                uint32_t x = (uint32_t) (r + 1) * 2654435761u;
                x ^= x >> 15; x *= 2246822519u; x ^= x >> 13;
                return degreesToSemitones (p.scale, (int) (x % 15u) - 7);
            }
            case same:
            default: return 0;
        }
    }

    void process (float* left, float* right, int numSamples, const Transport& t) noexcept
    {
        const double spb = t.samplesPerBeat (sr);
        mixSmooth.setTarget (std::clamp (params.mix, 0.0f, 1.0f));
        float* io[2] = { left, right };

        for (int n = 0; n < numSamples; ++n)
        {
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[(size_t) c];
                s.input[(size_t) inPos] = io[c][n];
            }
            inPos = (inPos + 1) % fftSize;

            if (++hopCount >= hop)
            {
                hopCount = 0;
                // Song position of the middle of this frame.
                const double ppq = t.ppqAtBlockStart + (n - fftSize / 2) / spb;
                advanceStep (ppq);
                for (int c = 0; c < 2; ++c) processFrame (ch[(size_t) c], c == 0);
            }

            const float m = mixSmooth.next();
            for (int c = 0; c < 2; ++c)
            {
                auto& s = ch[(size_t) c];
                const float wet = s.output[(size_t) outPos];
                s.output[(size_t) outPos] = 0.0f;
                // The dry signal is delayed by the same latency, so mixing stays in time.
                const float dry = s.dry[(size_t) dryPos];
                s.dry[(size_t) dryPos] = io[c][n];
                io[c][n] = dry * (1.0f - m) + sanitize (wet) * m;
            }
            outPos = (outPos + 1) % (fftSize * 2);
            dryPos = (dryPos + 1) % latency;
        }
    }

private:
    struct Channel
    {
        std::vector<float> input, output, dry;
        std::vector<float> phase, prevPhase, mag, frozenMag, frozenDelta, outPhase;
    };

    void advanceStep (double ppq) noexcept
    {
        const double div = divisionQuarters (params.division);
        const auto stepIndex = (long long) std::floor (ppq / div);
        const int steps = std::clamp (params.steps, 1, maxSteps);
        const int inPattern = (int) (((stepIndex % steps) + steps) % steps);
        const bool freeze = ((params.pattern >> inPattern) & 1u) != 0;
        capture = false;
        if (stepIndex != lastStep)
        {
            if (freeze)
            {
                // Grab a new spectrum when a freeze run starts (or the pattern restarts).
                if (! freezing || inPattern == 0) { capture = true; repeat = 0; }
                else ++repeat;
            }
            freezing = freeze;
            lastStep = stepIndex;
            currentStep = inPattern;
        }
    }

    float nextRandom() noexcept
    {
        random = random * 1664525u + 1013904223u;
        return (float) (random >> 8) / 16777216.0f - 0.5f;
    }

    static float wrap (float x) noexcept
    {
        return x - (float) twoPi * std::floor ((x + (float) pi) / (float) twoPi);
    }

    float maskGain (int bin) const noexcept
    {
        const double hz = bin * sr / fftSize;
        constexpr int tooth = 6;
        const bool even = ((bin / tooth) % 2) == 0;
        switch (params.mask)
        {
            case combA:     return even ? 1.0f : 0.0f;
            case combB:     return even ? 0.0f : 1.0f;
            case alternate: return (repeat % 2 == 0) == even ? 1.0f : 0.0f;
            case lowsOnly:  return hz < 700.0 ? 1.0f : 0.0f;
            case highsOnly: return hz >= 700.0 ? 1.0f : 0.0f;
            case walk:
            {
                const double lo = 150.0 * std::exp2 ((double) (repeat % 6));
                return hz >= lo && hz < lo * 2.0 ? 1.0f : 0.0f;
            }
            case noMask:
            default: return 1.0f;
        }
    }

    void processFrame (Channel& s, bool first) noexcept
    {
        // Analysis.
        for (int i = 0; i < fftSize; ++i)
            frame[(size_t) i] = { s.input[(size_t) ((inPos + i) % fftSize)] * window[(size_t) i], 0.0f };
        fft.perform (frame.data(), false);

        const float expectedStep = (float) (twoPi * hop / fftSize);
        for (int k = 0; k < numBins; ++k)
        {
            s.prevPhase[(size_t) k] = s.phase[(size_t) k];
            s.mag[(size_t) k] = std::abs (frame[(size_t) k]);
            s.phase[(size_t) k] = std::arg (frame[(size_t) k]);
        }

        if (capture)
        {
            for (int k = 0; k < numBins; ++k)
            {
                const float expected = expectedStep * (float) k;
                s.frozenMag[(size_t) k] = s.mag[(size_t) k];
                s.frozenDelta[(size_t) k] = expected + wrap (s.phase[(size_t) k] - s.prevPhase[(size_t) k] - expected);
                s.outPhase[(size_t) k] = s.phase[(size_t) k];
            }
        }

        if (freezing)
        {
            // Resynthesis of the held spectrum, moved by the interval of this repeat.
            const float ratio = semitonesToRatio ((float) intervalFor (params, repeat));
            const float smear = std::clamp (params.smear, 0.0f, 1.0f) * (float) twoPi;
            const float tilt = std::clamp (params.tilt, -1.0f, 1.0f);
            for (int j = 0; j < numBins; ++j)
            {
                const float src = (float) j / ratio;
                const int k0 = (int) src;
                const float frac = src - (float) k0;
                float mag = 0.0f, delta = expectedStep * (float) j;
                if (k0 + 1 < numBins)
                {
                    mag = s.frozenMag[(size_t) k0] * (1.0f - frac) + s.frozenMag[(size_t) k0 + 1] * frac;
                    delta = (s.frozenDelta[(size_t) k0] * (1.0f - frac) + s.frozenDelta[(size_t) k0 + 1] * frac) * ratio;
                }
                const float hz = std::max (20.0f, (float) (j * sr / fftSize));
                mag *= maskGain (j) * std::pow (hz / 1000.0f, tilt);
                s.outPhase[(size_t) j] = wrap (s.outPhase[(size_t) j] + delta + smear * nextRandom());
                frame[(size_t) j] = std::polar (mag, s.outPhase[(size_t) j]);
            }
        }
        // Pass steps keep the analysed spectrum as it is.

        for (int k = 1; k < fftSize / 2; ++k) frame[(size_t) (fftSize - k)] = std::conj (frame[(size_t) k]);
        frame[0] = { frame[0].real(), 0.0f };
        frame[(size_t) fftSize / 2] = { frame[(size_t) fftSize / 2].real(), 0.0f };

        if (first)
        {
            // Spectrum summary for the display, log spaced from 40 Hz.
            for (int b = 0; b < numBands; ++b)
            {
                const double lo = 40.0 * std::pow (450.0, (double) b / numBands);
                const double hi = 40.0 * std::pow (450.0, (double) (b + 1) / numBands);
                const int k0 = std::clamp ((int) (lo * fftSize / sr), 1, numBins - 1);
                const int k1 = std::clamp ((int) (hi * fftSize / sr), k0 + 1, numBins);
                float peak = 0.0f;
                for (int k = k0; k < k1; ++k) peak = std::max (peak, std::abs (frame[(size_t) k]));
                const float db = 20.0f * std::log10 (peak / (fftSize * 0.25f) + 1.0e-6f);
                bands[(size_t) b] = std::clamp ((db + 70.0f) / 70.0f, 0.0f, 1.0f);
            }
        }

        fft.perform (frame.data(), true);
        // Hann analysis x Hann synthesis at 75 % overlap sums to 1.5.
        constexpr float olaScale = 1.0f / 1.5f;
        for (int i = 0; i < fftSize; ++i)
        {
            auto& o = s.output[(size_t) ((outPos + 1 + i) % (fftSize * 2))];
            o += frame[(size_t) i].real() * window[(size_t) i] * olaScale;
        }
    }

    Params params;
    double sr = 48000.0;
    Fft fft;
    std::vector<float> window;
    std::vector<std::complex<float>> frame;
    std::array<Channel, 2> ch;
    int inPos = 0, outPos = 0, dryPos = 0, hopCount = 0;
    long long lastStep = -1;
    bool freezing = false, capture = false;
    int repeat = 0, currentStep = -1;
    uint32_t random = 12345u;
    Smoother mixSmooth;
    std::array<float, numBands> bands {};
};

} // namespace originpsy
