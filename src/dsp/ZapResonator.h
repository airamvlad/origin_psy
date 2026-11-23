#pragma once

#include "Common.h"
#include "Filters.h"
#include "Scales.h"

#include <array>

namespace originpsy
{

/** Every hit on the input fires a short "laser" zap: a ringing tone that
    starts away from a scale note and glides onto it. The note comes from the
    scale (up, down, random, or by how hard the hit was). Four voices, so fast
    hats can overlap. */
class ZapResonator
{
public:
    enum Order { up = 0, down, random, velocity };
    static constexpr int numVoices = 4;

    struct Params
    {
        float thresholdDb = -30.0f;  // a hit must be louder than this
        int scale = 0;
        int root = 4;                // 0 = C
        int octave = 5;              // octave of the lowest zap note
        int order = up;
        float sweep = 24.0f;         // semitones the zap starts away from its note (-48..48)
        float sweepMs = 80.0f;       // how fast it lands on the note
        float decayMs = 300.0f;      // how long it rings
        float resonance = 0.8f;      // 0 = filtered input (noisy), 1 = pure ringing tone
        float width = 0.6f;          // voices spread across the stereo field
        float mix = 0.5f;            // 0..0.5 adds zaps, 0.5..1 fades the dry signal out
        bool listenToKey = false;    // hits come from the sidechain key instead of the input
    };

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        fastAttack = (float) std::exp (-1.0 / (0.0005 * sr));
        fastRelease = (float) std::exp (-1.0 / (0.03 * sr)); // holds across the cycles of a low note
        slowCoeff = (float) std::exp (-1.0 / (0.15 * sr));
        holdoffSamples = (int) (0.035 * sr);
        mixSmooth.prepare (sr, 0.02);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v = {};
        fast = slow = 0.0f;
        holdoff = 0;
        armed = true;
        counter = 0;
        nextVoice = 0;
        lastNote = -1;
        hits = 0;
        mixSmooth.reset (params.mix);
    }

    void setParams (const Params& p) noexcept { params = p; }

    /** Last note fired (MIDI), -1 before the first hit. */
    int getLastNote() const noexcept { return lastNote; }
    /** Counts hits, so a display can flash on each one. */
    uint32_t getHitCount() const noexcept { return hits; }
    /** Loudest voice right now, 0..1. */
    float getLevel() const noexcept
    {
        float m = 0.0f;
        for (const auto& v : voices) m = std::max (m, v.active ? v.amp * v.velocity : 0.0f);
        return m;
    }

    /** Scale notes the zaps can land on: two octaves from root + octave. */
    static int noteForStep (const Params& p, int step) noexcept
    {
        const int count = notesInRange (p);
        const int i = ((step % count) + count) % count;
        return 12 * (std::clamp (p.octave, 1, 8) + 1) + std::clamp (p.root, 0, 11) + degreesToSemitones (p.scale, i);
    }

    static int notesInRange (const Params& p) noexcept
    {
        return scales[(size_t) std::clamp (p.scale, 0, (int) scales.size() - 1)].size * 2;
    }

    /** `key` is the mono sidechain signal, or nullptr when there is none
        (the zaps then listen to the input even when listenToKey is set). */
    void process (float* left, float* right, int numSamples, const Transport&, const float* key = nullptr) noexcept
    {
        const float* detect = params.listenToKey ? key : nullptr;
        const float threshold = dbToGain (params.thresholdDb);
        const float sweepDecay = (float) std::exp (-1.0 / (std::max (1.0f, params.sweepMs) * 0.001 * sr * 0.25));
        const float ampDecay = (float) std::exp (-1.0 / (std::max (5.0f, params.decayMs) * 0.001 * sr * 0.25));
        const float res = std::clamp (params.resonance, 0.0f, 1.0f);
        mixSmooth.setTarget (std::clamp (params.mix, 0.0f, 1.0f));

        for (int n = 0; n < numSamples; ++n)
        {
            const float inL = left[n], inR = right[n];
            const float mono = 0.5f * (inL + inR);
            const float a = std::abs (detect != nullptr ? detect[n] : mono);

            // Transient: the fast envelope jumps well above the slow one.
            fast = a + (a > fast ? fastAttack : fastRelease) * (fast - a);
            slow = fast + slowCoeff * (slow - fast);
            fast = sanitize (fast);
            slow = sanitize (slow);
            // After a hit, wait until the level settles (or a short holdoff
            // passes and it falls back) before the next one can fire.
            if (holdoff > 0) --holdoff;
            else if (! armed && (fast < slow * 1.3f || fast < threshold)) armed = true;
            if (armed && holdoff == 0 && fast > threshold && fast > slow * 2.0f + 1.0e-4f)
            {
                trigger (std::clamp (fast / std::max (threshold, 1.0e-4f), 1.0f, 8.0f));
                holdoff = holdoffSamples;
                armed = false;
            }

            float zapL = 0.0f, zapR = 0.0f;
            for (auto& v : voices)
            {
                if (! v.active) continue;
                // Pitch and level move in small steps (every 4 samples is plenty).
                if ((counter & 3) == 0)
                {
                    v.sweepPos *= sweepDecay;
                    v.amp *= ampDecay;
                    const double hz = std::min (sr * 0.45, v.noteHz * std::exp2 (params.sweep * v.sweepPos / 12.0f));
                    const double w = twoPi * hz / sr;
                    v.c = (float) std::cos (w);
                    v.s = (float) std::sin (w);
                    v.filter.set (hz, 1.5 + 6.0 * (1.0 - res), sr);
                    if (v.amp < 1.0e-4f) { v.active = false; continue; }
                }
                // Rotating phasor: a clean sine whose pitch can glide freely.
                const float x = v.x * v.c - v.y * v.s;
                const float y = v.x * v.s + v.y * v.c;
                // Keep the phasor on the unit circle.
                const float norm = 1.5f - 0.5f * (x * x + y * y);
                v.x = x * norm;
                v.y = y * norm;

                const float noisy = v.filter.bandpass (mono) * 4.0f;
                const float out = v.velocity * v.amp * (res * v.y + (1.0f - res) * std::tanh (noisy));
                zapL += out * v.panL;
                zapR += out * v.panR;
            }
            ++counter;

            const float m = mixSmooth.next();
            const float dry = m < 0.5f ? 1.0f : 2.0f * (1.0f - m);
            const float wet = m < 0.5f ? 2.0f * m : 1.0f;
            left[n] = inL * dry + sanitize (zapL) * 0.5f * wet;
            right[n] = inR * dry + sanitize (zapR) * 0.5f * wet;
        }
    }

private:
    struct Voice
    {
        bool active = false;
        double noteHz = 440.0;
        float sweepPos = 1.0f;   // 1 at the hit, falls to 0 (on the note)
        float amp = 0.0f;
        float velocity = 0.0f;
        float x = 1.0f, y = 0.0f, c = 1.0f, s = 0.0f;
        float panL = 0.707f, panR = 0.707f;
        Svf filter;
    };

    void trigger (float strength) noexcept
    {
        const int count = notesInRange (params);
        int step = 0;
        switch (params.order)
        {
            case down: step = count - 1 - (int) (hits % (uint32_t) count); break;
            case random:
            {
                uint32_t x = (hits + 1) * 2654435761u;
                x ^= x >> 15; x *= 2246822519u; x ^= x >> 13;
                step = (int) (x % (uint32_t) count);
                break;
            }
            case velocity: step = std::clamp ((int) ((strength - 1.0f) / 3.0f * (float) count), 0, count - 1); break;
            case up:
            default: step = (int) (hits % (uint32_t) count); break;
        }
        const int note = noteForStep (params, step);

        auto& v = voices[(size_t) nextVoice];
        const float side = (nextVoice % 2 == 0 ? -1.0f : 1.0f) * (nextVoice < 2 ? 0.5f : 1.0f);
        const float angle = (std::clamp (side * params.width, -1.0f, 1.0f) + 1.0f) * 0.25f * (float) pi;
        v.active = true;
        v.noteHz = 440.0 * std::exp2 ((note - 69) / 12.0);
        v.sweepPos = 1.0f;
        v.amp = 1.0f;
        v.velocity = std::clamp (0.35f + 0.2f * strength, 0.0f, 1.0f);
        v.x = 1.0f;  // the output (y) starts at 0: no click
        v.y = 0.0f;
        v.panL = std::cos (angle);
        v.panR = std::sin (angle);
        v.filter.reset();

        nextVoice = (nextVoice + 1) % numVoices;
        lastNote = note;
        ++hits;
    }

    Params params;
    double sr = 48000.0;
    std::array<Voice, numVoices> voices;
    float fast = 0.0f, slow = 0.0f, fastAttack = 0.0f, fastRelease = 0.0f, slowCoeff = 0.0f;
    bool armed = true;
    int holdoff = 0, holdoffSamples = 1000, nextVoice = 0, lastNote = -1;
    uint32_t counter = 0, hits = 0;
    Smoother mixSmooth;
};

} // namespace originpsy
