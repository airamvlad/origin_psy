#pragma once

#include <array>
#include <cmath>

namespace originpsy
{

struct Scale
{
    const char* name;
    int size;
    std::array<int, 12> steps; // semitones from the root, ascending
};

inline constexpr std::array<Scale, 9> scales { {
    { "Phrygian",        7, { 0, 1, 3, 5, 7, 8, 10 } },
    { "Hijaz",           7, { 0, 1, 4, 5, 7, 8, 10 } }, // Phrygian dominant
    { "Harmonic Minor",  7, { 0, 2, 3, 5, 7, 8, 11 } },
    { "Natural Minor",   7, { 0, 2, 3, 5, 7, 8, 10 } },
    { "Hijazkar",        7, { 0, 1, 4, 5, 7, 8, 11 } },
    { "Hungarian Minor", 7, { 0, 2, 3, 6, 7, 8, 11 } },
    { "Dorian",          7, { 0, 2, 3, 5, 7, 9, 10 } },
    { "Minor Pentatonic", 5, { 0, 3, 5, 7, 10 } },
    { "Chromatic",      12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
} };

/** Semitone interval of a number of scale degrees (may be negative).
    Degree 0 is the root; 7 degrees in a seven-note scale is an octave. */
inline int degreesToSemitones (int scaleIndex, int degrees) noexcept
{
    const auto& s = scales[(size_t) std::clamp (scaleIndex, 0, (int) scales.size() - 1)];
    const int octave = (int) std::floor ((double) degrees / s.size);
    const int within = degrees - octave * s.size;
    return octave * 12 + s.steps[(size_t) within];
}

/** Nearest scale note to a MIDI note number, for a given root (0 = C). */
inline int snapToScale (int scaleIndex, int root, int midiNote) noexcept
{
    const auto& s = scales[(size_t) std::clamp (scaleIndex, 0, (int) scales.size() - 1)];
    int best = midiNote, bestDistance = 99;
    for (int candidate = midiNote - 6; candidate <= midiNote + 6; ++candidate)
    {
        const int pc = ((candidate - root) % 12 + 12) % 12;
        for (int i = 0; i < s.size; ++i)
            if (s.steps[(size_t) i] == pc && std::abs (candidate - midiNote) < bestDistance)
            {
                best = candidate;
                bestDistance = std::abs (candidate - midiNote);
            }
    }
    return best;
}

} // namespace originpsy
