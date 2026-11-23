#pragma once

#include "FraktalEcho.h"
#include "OrbitDoppler.h"
#include "PolimetarGate.h"
#include "SpectralRatchet.h"
#include "TapeStop.h"
#include "VokalAcid.h"
#include "ZapResonator.h"

#include <array>
#include <vector>

namespace originpsy
{

enum class ModuleType : int { empty = 0, fraktalEcho, vokalAcid, polimetarGate, zapResonator, orbitDoppler, tapeStop, spectralRatchet };
constexpr int numModuleTypes = 8;

inline const char* moduleName (ModuleType t) noexcept
{
    switch (t)
    {
        case ModuleType::fraktalEcho:   return "Fractal Echo";
        case ModuleType::vokalAcid:     return "Vocal Acid";
        case ModuleType::polimetarGate: return "Polymeter Gate";
        case ModuleType::zapResonator:  return "Zap Resonator";
        case ModuleType::orbitDoppler:  return "Orbit Doppler";
        case ModuleType::tapeStop:      return "Tape Stop";
        case ModuleType::spectralRatchet: return "Spectral Ratchet";
        case ModuleType::empty:         break;
    }
    return "Empty";
}

/** Four slots, each holding one of the modules. A module can sit in one
    slot at a time; a second slot asking for the same module stays empty.
    Changing the layout fades out, swaps and fades back in, so there is no
    click. */
class Chain
{
public:
    static constexpr int numSlots = 4;

    struct Layout
    {
        std::array<ModuleType, numSlots> slots { ModuleType::vokalAcid, ModuleType::polimetarGate,
                                                 ModuleType::fraktalEcho, ModuleType::empty };
        std::array<bool, numSlots> bypass {};
        std::array<bool, numSlots> solo {};

        bool operator== (const Layout& o) const noexcept { return slots == o.slots && bypass == o.bypass && solo == o.solo; }
        bool operator!= (const Layout& o) const noexcept { return ! (*this == o); }

        /** The layout with duplicate modules and bypassed slots removed. When
            any slot is soloed, only the soloed slots play. */
        std::array<ModuleType, numSlots> effective() const noexcept
        {
            std::array<ModuleType, numSlots> out {};
            std::array<bool, numModuleTypes> used {};
            bool anySolo = false;
            for (int i = 0; i < numSlots; ++i) anySolo = anySolo || (solo[(size_t) i] && slots[(size_t) i] != ModuleType::empty);
            for (int i = 0; i < numSlots; ++i)
            {
                const auto t = slots[(size_t) i];
                const int ti = (int) t;
                if (t == ModuleType::empty || bypass[(size_t) i] || ti < 0 || ti >= numModuleTypes || used[(size_t) ti])
                    continue;
                if (anySolo && ! solo[(size_t) i]) continue;
                used[(size_t) ti] = true;
                out[(size_t) i] = t;
            }
            return out;
        }
    };

    FraktalEcho echo;
    VokalAcid vokal;
    PolimetarGate gate;
    ZapResonator zap;
    OrbitDoppler orbit;
    TapeStop tape;
    SpectralRatchet ratchet;

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        echo.prepare (sampleRate, maxBlock);
        vokal.prepare (sampleRate, maxBlock);
        gate.prepare (sampleRate, maxBlock);
        zap.prepare (sampleRate, maxBlock);
        orbit.prepare (sampleRate, maxBlock);
        tape.prepare (sampleRate, maxBlock);
        ratchet.prepare (sampleRate, maxBlock);
        fadeSamples = std::max (1, (int) (sampleRate * 0.008));
        blockSize = std::max (1, maxBlock);
        for (auto& d : dry) d.assign ((size_t) blockSize, 0.0f);
        for (auto& m : mixSmooth) m.prepare (sampleRate, 0.02);
        reset();
    }

    void reset()
    {
        echo.reset();
        vokal.reset();
        gate.reset();
        zap.reset();
        orbit.reset();
        tape.reset();
        ratchet.reset();
        fadeGain = 1.0f;
        fadingOut = false;
        active = pending;
        for (int i = 0; i < numSlots; ++i) mixSmooth[(size_t) i].reset (slotMix[(size_t) i]);
    }

    /** Dry/wet of each slot (0..1): how much of the slot's module is heard. */
    void setSlotMix (int slot, float mix) noexcept { slotMix[(size_t) slot] = std::clamp (mix, 0.0f, 1.0f); }

    void setLayout (const Layout& l) noexcept { pending = l; }

    /** Latency of a layout in samples (only Spectral Ratchet has any). */
    static int latencyOf (const Layout& l) noexcept
    {
        for (auto type : l.effective())
            if (type == ModuleType::spectralRatchet) return SpectralRatchet::latency;
        return 0;
    }
    const Layout& activeLayout() const noexcept { return active; }

    /** `key` is the mono sidechain signal, or nullptr when the host sends none. */
    void process (float* left, float* right, int numSamples, const Transport& t, const float* key = nullptr) noexcept
    {
        // The slot Dry/Wet needs a copy of the input, so long blocks go in pieces.
        for (int start = 0; start < numSamples; start += blockSize)
        {
            Transport part = t;
            part.ppqAtBlockStart = t.ppqAtBlockStart + start / t.samplesPerBeat (sr);
            processPart (left + start, right + start, std::min (blockSize, numSamples - start), part,
                         key != nullptr ? key + start : nullptr);
        }
    }

private:
    void processPart (float* left, float* right, int numSamples, const Transport& t, const float* key) noexcept
    {
        if (pending.effective() != active.effective() && ! fadingOut)
            fadingOut = true;
        else if (pending != active && pending.effective() == active.effective())
            active = pending; // nothing audible changes

        const auto playing = active.effective();
        for (int slot = 0; slot < numSlots; ++slot)
        {
            const auto type = playing[(size_t) slot];
            if (type == ModuleType::empty) continue;
            auto& ms = mixSmooth[(size_t) slot];
            ms.setTarget (slotMix[(size_t) slot]);
            // Spectral Ratchet is late by its latency, so blending it with the
            // undelayed input would comb. Its own (latency-matched) mix gets
            // the slot's Dry/Wet instead, set by the processor.
            const bool blend = type != ModuleType::spectralRatchet
                               && (slotMix[(size_t) slot] < 1.0f || ms.current() < 0.9999f);
            if (blend)
            {
                std::copy (left, left + numSamples, dry[0].begin());
                std::copy (right, right + numSamples, dry[1].begin());
            }
            switch (type)
            {
                case ModuleType::fraktalEcho:   echo.process (left, right, numSamples, t); break;
                case ModuleType::vokalAcid:     vokal.process (left, right, numSamples, t); break;
                case ModuleType::polimetarGate: gate.process (left, right, numSamples, t, key); break;
                case ModuleType::zapResonator:  zap.process (left, right, numSamples, t, key); break;
                case ModuleType::orbitDoppler:  orbit.process (left, right, numSamples, t); break;
                case ModuleType::tapeStop:      tape.process (left, right, numSamples, t); break;
                case ModuleType::spectralRatchet: ratchet.process (left, right, numSamples, t); break;
                case ModuleType::empty: break;
            }
            if (blend)
            {
                for (int n = 0; n < numSamples; ++n)
                {
                    const float m = ms.next();
                    left[n] = dry[0][(size_t) n] * (1.0f - m) + left[n] * m;
                    right[n] = dry[1][(size_t) n] * (1.0f - m) + right[n] * m;
                }
            }
            else
            {
                ms.reset (1.0f);
            }
        }

        const float step = 1.0f / (float) fadeSamples;
        for (int n = 0; n < numSamples; ++n)
        {
            if (fadingOut) fadeGain = std::max (0.0f, fadeGain - step);
            else           fadeGain = std::min (1.0f, fadeGain + step);
            left[n] *= fadeGain;
            right[n] *= fadeGain;
        }

        if (fadingOut && fadeGain <= 0.0f)
        {
            // Modules that join the chain start clean, without old tails.
            const auto before = active.effective();
            active = pending;
            for (auto type : active.effective())
                if (std::find (before.begin(), before.end(), type) == before.end())
                    resetModule (type);
            fadingOut = false;
        }
    }

    void resetModule (ModuleType t)
    {
        switch (t)
        {
            case ModuleType::fraktalEcho:   echo.reset(); break;
            case ModuleType::vokalAcid:     vokal.reset(); break;
            case ModuleType::polimetarGate: gate.reset(); break;
            case ModuleType::zapResonator:  zap.reset(); break;
            case ModuleType::orbitDoppler:  orbit.reset(); break;
            case ModuleType::tapeStop:      tape.reset(); break;
            case ModuleType::spectralRatchet: ratchet.reset(); break;
            case ModuleType::empty: break;
        }
    }

    double sr = 48000.0;
    Layout active, pending;
    int fadeSamples = 384, blockSize = 512;
    std::array<std::vector<float>, 2> dry;
    std::array<float, numSlots> slotMix { { 1.0f, 1.0f, 1.0f, 1.0f } };
    std::array<Smoother, numSlots> mixSmooth;
    float fadeGain = 1.0f;
    bool fadingOut = false;
};

} // namespace originpsy
