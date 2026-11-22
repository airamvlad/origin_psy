#pragma once

#include "PitchTracker.h"

#include <array>

namespace originpsy
{

/** Simple 2x oversampled waveshaper: zero-stuffing, 4th order low-pass on the
    way up and down. Used for the drive stage so tanh does not alias. */
class Oversampled2xTanh
{
public:
    void prepare (double sampleRate)
    {
        for (auto& f : up) f.set (false, sampleRate * 0.42, sampleRate * 2.0);
        for (auto& f : down) f.set (false, sampleRate * 0.42, sampleRate * 2.0);
        reset();
    }

    void reset() noexcept
    {
        for (auto& f : up) f.reset();
        for (auto& f : down) f.reset();
    }

    float process (float x, float drive) noexcept
    {
        const float norm = 1.0f / std::tanh (drive);
        float a = up[1].process (up[0].process (2.0f * x));
        float b = up[1].process (up[0].process (0.0f));
        a = std::tanh (a * drive) * norm;
        b = std::tanh (b * drive) * norm;
        const float out = down[1].process (down[0].process (a));
        down[1].process (down[0].process (b));
        return out;
    }

private:
    Biquad up[2], down[2];
};

/** Vokal Acid: a formant filter whose vowel follows the pitch of the input.

    A pitch tracker finds the note the acid line plays, a 12 slot map turns
    the note (C..B) into a vowel, and three band-passes move to that vowel's
    formants with a glide. Accents (sudden jumps in level) push the resonance
    and the level up for a moment. */
class VokalAcid
{
public:
    enum Character { male = 0, female, robot };

    struct Params
    {
        std::array<float, 12> vowelMap { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1 }; // 0=A 1=E 2=I 3=O 4=U
        float glideMs = 60.0f;      // 5..500
        float shift = 0.0f;         // formant shift, semitones -12..12
        float resonance = 0.5f;     // 0..1
        float accent = 0.5f;        // accent sensitivity 0..1
        int character = female;
        float drive = 0.2f;         // 0..1
        float mix = 0.8f;           // 0..1
    };

    struct Formants { float f[3]; };

    static Formants vowelFormants (int character, int vowel) noexcept
    {
        static constexpr float maleTable[5][3] = {
            { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
        static constexpr float femaleTable[5][3] = {
            { 850, 1220, 2810 }, { 610, 2330, 2990 }, { 310, 2790, 3310 }, { 590, 920, 2710 }, { 370, 950, 2670 } };
        const auto& t = character == male ? maleTable : femaleTable;
        const int v = std::clamp (vowel, 0, 4);
        return { { t[v][0], t[v][1], t[v][2] } };
    }

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        tracker.prepare (sampleRate);
        for (auto& o : shaper) o.prepare (sampleRate);
        vowelPos.prepare (sr, params.glideMs * 0.001);
        mixSmooth.prepare (sr, 0.02);
        reset();
    }

    void reset()
    {
        tracker.reset();
        for (auto& o : shaper) o.reset();
        for (auto& ch : bands) for (auto& b : ch) b.reset();
        vowelPos.reset (params.vowelMap[0]);
        mixSmooth.reset (params.mix);
        fast = slow = accentEnv = 0.0f;
        currentNote = -1;
        updateCounter = 0;
    }

    void setParams (const Params& p) noexcept
    {
        params = p;
        vowelPos.prepare (sr, std::clamp (p.glideMs, 5.0f, 500.0f) * 0.001);
    }

    /** Last detected note (MIDI number, -1 = none) and the vowel position
        (0..4, fractional while gliding). For the editor. */
    int getDetectedNote() const noexcept { return currentNote; }
    float getVowelPosition() const noexcept { return vowelPos.current(); }
    float getAccent() const noexcept { return accentEnv; }

    void process (float* left, float* right, int numSamples, const Transport&) noexcept
    {
        mixSmooth.setTarget (params.mix);
        const float drive = 1.0f + params.drive * 7.0f;
        const float fastAtt = coeff (0.001), fastRel = coeff (0.03), slowCoef = coeff (0.15), accRel = coeff (0.12);

        for (int n = 0; n < numSamples; ++n)
        {
            const float inL = left[n], inR = right[n];
            const float mono = 0.5f * (inL + inR);

            if (tracker.push (mono))
            {
                if (tracker.getConfidence() > 0.5f && tracker.getFrequency() > 0.0f)
                {
                    currentNote = (int) std::lround (PitchTracker::frequencyToNote (tracker.getFrequency()));
                    vowelPos.setTarget (params.vowelMap[(size_t) (((currentNote % 12) + 12) % 12)]);
                }
            }

            // Accent: the fast envelope jumping above the slow one.
            const float a = std::abs (mono);
            fast = a > fast ? a + fastAtt * (fast - a) : a + fastRel * (fast - a);
            slow = fast + slowCoef * (slow - fast);
            const float jump = slow > 1.0e-4f ? fast / slow - 1.0f : 0.0f;
            const float hit = std::clamp (jump * (0.5f + 2.5f * params.accent), 0.0f, 1.0f);
            accentEnv = std::max (hit, accentEnv * accRel);

            const float pos = vowelPos.next();
            if (updateCounter-- <= 0)
            {
                updateFilters (pos);
                updateCounter = 16;
            }

            const float level = 1.0f + accentEnv * params.accent;
            float wetL = 0.0f, wetR = 0.0f;
            const float dl = shaper[0].process (inL, drive), dr = shaper[1].process (inR, drive);
            for (int b = 0; b < 3; ++b)
            {
                wetL += gains[(size_t) b] * bands[0][(size_t) b].bandpass (dl);
                wetR += gains[(size_t) b] * bands[1][(size_t) b].bandpass (dr);
            }
            wetL *= makeup * level;
            wetR *= makeup * level;

            const float m = mixSmooth.next();
            left[n] = inL * (1.0f - m) + wetL * m;
            right[n] = inR * (1.0f - m) + wetR * m;
        }
    }

private:
    float coeff (double seconds) const noexcept { return (float) std::exp (-1.0 / (seconds * sr)); }

    void updateFilters (float pos) noexcept
    {
        const int lo = std::clamp ((int) std::floor (pos), 0, 4), hi = std::min (lo + 1, 4);
        const float frac = std::clamp (pos - (float) lo, 0.0f, 1.0f);
        const int ch = params.character == robot ? female : params.character;
        const auto a = vowelFormants (ch, lo), b = vowelFormants (ch, hi);
        const float shiftRatio = semitonesToRatio (params.shift);
        const float q = (2.0f + params.resonance * 18.0f) * (1.0f + accentEnv * params.accent);

        const float f0 = tracker.getFrequency();
        for (int i = 0; i < 3; ++i)
        {
            float f = std::exp (std::log (a.f[i]) * (1.0f - frac) + std::log (b.f[i]) * frac) * shiftRatio;
            if (params.character == robot && f0 > 20.0f)
                f = std::max (1.0f, std::round (f / f0)) * f0; // lock formants onto harmonics
            for (auto& chBands : bands) chBands[(size_t) i].set (f, q * (i == 0 ? 1.0f : 1.3f), sr);
        }
        // Wider bands pass more energy; keep the level roughly constant.
        makeup = 1.2f * std::sqrt (q / 4.0f) / (1.0f + 0.25f * params.resonance);
    }

    double sr = 48000.0;
    Params params;
    PitchTracker tracker;
    std::array<Oversampled2xTanh, 2> shaper;
    std::array<std::array<Svf, 3>, 2> bands;
    std::array<float, 3> gains { 1.0f, 0.6f, 0.35f };
    Smoother vowelPos, mixSmooth;
    float fast = 0.0f, slow = 0.0f, accentEnv = 0.0f, makeup = 1.0f;
    int currentNote = -1, updateCounter = 0;
};

} // namespace originpsy
