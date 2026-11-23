// End-to-end checks of the plugin processor, without a DAW.
// `originpsy_plugin_tests --screenshot out.png [module]` saves the editor as a PNG.

#include "plugin/Parameters.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/Presets.h"

#include <cmath>
#include <cstdio>

using namespace originpsy;

namespace
{
int failures = 0;

void check (bool condition, const char* what)
{
    if (! condition)
    {
        std::printf ("FAIL: %s\n", what);
        ++failures;
    }
}

void setValue (OriginPsyProcessor& p, const juce::String& id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

/** Plays a 110 Hz saw through the processor; returns the left output. */
std::vector<float> run (OriginPsyProcessor& p, double sr, int block, int totalSamples)
{
    std::vector<float> out;
    out.reserve ((size_t) totalSamples);
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    double phase = 0.0;
    for (int start = 0; start < totalSamples; start += block)
    {
        const int n = std::min (block, totalSamples - start);
        buffer.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            const float v = 0.5f * (float) (2.0 * phase - 1.0);
            phase += 110.0 / sr;
            phase -= std::floor (phase);
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
        }
        p.processBlock (buffer, midi);
        for (int i = 0; i < n; ++i) out.push_back (buffer.getSample (0, i));
    }
    return out;
}

bool healthy (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x) || std::abs (x) > 8.0f) return false;
    return true;
}

void parameters()
{
    OriginPsyProcessor p;
    const auto& all = p.getParameters();
    check (all.size() >= 128, "all module parameters are exposed to the host");
    juce::StringArray ids;
    for (auto* prm : all)
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (prm))
        {
            check (! ids.contains (withId->paramID), "parameter IDs are unique");
            ids.add (withId->paramID);
            check (prm->isAutomatable(), "every parameter can be automated");
        }
}

void sampleRatesAndBlocks()
{
    for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
        for (int block : { 32, 256, 1024 })
        {
            OriginPsyProcessor p;
            p.setRateAndBufferSizeDetails (sr, block);
            p.prepareToPlay (sr, block);
            const auto out = run (p, sr, block, (int) sr);
            check (healthy (out), "output is clean at every sample rate and block size");
        }

    // Hosts may send a shorter block than announced.
    OriginPsyProcessor p;
    p.prepareToPlay (48000.0, 512);
    const auto out = run (p, 48000.0, 100, 48000);
    check (healthy (out), "shorter blocks than announced work");
}

void emptyChainIsTransparent()
{
    OriginPsyProcessor p;
    for (int i = 0; i < Chain::numSlots; ++i) setValue (p, params::slotModule (i), 0.0f);
    p.prepareToPlay (48000.0, 256);
    const auto out = run (p, 48000.0, 256, 24000);
    double phase = 0.0, err = 0.0;
    for (size_t i = 0; i < out.size(); ++i)
    {
        const float v = 0.5f * (float) (2.0 * phase - 1.0);
        phase += 110.0 / 48000.0;
        phase -= std::floor (phase);
        err = std::max (err, (double) std::abs (out[i] - v));
    }
    check (err < 1.0e-5, "empty chain with 0 dB output is a null test");
}

/** Peak of a run after the first half second (filters and smoothers settled). */
float settledPeak (const std::vector<float>& v)
{
    float m = 0.0f;
    for (size_t i = 24000; i < v.size(); ++i) m = std::max (m, std::abs (v[i]));
    return m;
}

void slotMixSoloAndLimiter()
{
    // Dry/Wet 0 on every slot turns the full chain into a null test.
    {
        OriginPsyProcessor p;
        p.applyDropPreset();
        for (int i = 0; i < Chain::numSlots; ++i) setValue (p, params::slotMix (i), 0.0f);
        p.prepareToPlay (48000.0, 256);
        const auto out = run (p, 48000.0, 256, 48000);
        double phase = 0.0, err = 0.0;
        for (size_t i = 0; i < out.size(); ++i)
        {
            const float v = 0.5f * (float) (2.0 * phase - 1.0);
            phase += 110.0 / 48000.0;
            phase -= std::floor (phase);
            if (i > 4800) err = std::max (err, (double) std::abs (out[i] - v));
        }
        check (err < 1.0e-4, "slot Dry/Wet at 0 passes the dry signal");
    }

    // Solo on one slot sounds the same as that slot alone in the chain.
    {
        OriginPsyProcessor solo, plain;
        solo.applyDropPreset();
        plain.applyDropPreset();
        int usedSlot = -1;
        for (int i = 0; i < Chain::numSlots && usedSlot < 0; ++i)
            if ((int) solo.apvts.getRawParameterValue (params::slotModule (i))->load() != 0) usedSlot = i;
        check (usedSlot >= 0, "Drop uses a slot");
        setValue (solo, params::slotSolo (usedSlot), 1.0f);
        for (int i = 0; i < Chain::numSlots; ++i)
            if (i != usedSlot) setValue (plain, params::slotModule (i), 0.0f);
        solo.prepareToPlay (48000.0, 256);
        plain.prepareToPlay (48000.0, 256);
        const auto a = run (solo, 48000.0, 256, 48000);
        const auto b = run (plain, 48000.0, 256, 48000);
        double err = 0.0;
        for (size_t i = 0; i < a.size(); ++i) err = std::max (err, (double) std::abs (a[i] - b[i]));
        check (err < 1.0e-4, "Solo plays only the soloed slot");
    }

    // Spectral Ratchet at slot Dry/Wet 0: the input, late by the plugin's latency (no comb).
    {
        OriginPsyProcessor p;
        for (int i = 0; i < Chain::numSlots; ++i) setValue (p, params::slotModule (i), 0.0f);
        setValue (p, params::slotModule (0), (float) ModuleType::spectralRatchet);
        setValue (p, params::slotMix (0), 0.0f);
        p.prepareToPlay (48000.0, 256);
        const auto out = run (p, 48000.0, 256, 48000);
        const int lat = SpectralRatchet::latency;
        double phase = 0.0, err = 0.0;
        std::vector<float> in (out.size());
        for (auto& v : in) { v = 0.5f * (float) (2.0 * phase - 1.0); phase += 110.0 / 48000.0; phase -= std::floor (phase); }
        for (size_t i = (size_t) lat + 4800; i < out.size(); ++i) err = std::max (err, (double) std::abs (out[i] - in[i - (size_t) lat]));
        check (err < 1.0e-3, "Spectral Ratchet at slot Dry/Wet 0 is the delayed input");
    }

    // The limiter keeps a +12 dB output under the ceiling; off, it passes.
    for (bool limit : { true, false })
    {
        OriginPsyProcessor p;
        for (int i = 0; i < Chain::numSlots; ++i) setValue (p, params::slotModule (i), 0.0f);
        setValue (p, params::outGain, 12.0f);
        setValue (p, params::outLimit, limit ? 1.0f : 0.0f);
        setValue (p, params::outCeiling, -1.0f);
        p.prepareToPlay (48000.0, 256);
        const auto peak = settledPeak (run (p, 48000.0, 256, 48000));
        if (limit) check (peak <= std::pow (10.0f, -1.0f / 20.0f) + 1.0e-5f, "the limiter holds the ceiling");
        else check (peak > 1.5f, "with the limiter off the output is not limited");
    }
}

void allModulesAndFraza()
{
    OriginPsyProcessor p;
    p.applyDropPreset();
    setValue (p, params::echoGenerations, 4.0f);
    setValue (p, params::echoFeedback, 1.0f);
    setValue (p, params::vokalResonance, 1.0f);
    setValue (p, params::vokalCharacter, 2.0f);
    setValue (p, params::slotModule (3), (float) ModuleType::zapResonator);
    setValue (p, params::zapThreshold, -60.0f);
    setValue (p, params::zapMix, 1.0f);
    p.prepareToPlay (48000.0, 256);
    const auto out = run (p, 48000.0, 256, 48000 * 8);
    check (healthy (out), "all modules with Drop and maximum settings stay bounded");
    check (p.apvts.getRawParameterValue (params::frazaTargets[0].id)->load() > 0.1f, "Drop sets the Phrase amounts");

    OriginPsyProcessor o;
    setValue (o, params::slotModule (0), (float) ModuleType::orbitDoppler);
    setValue (o, params::slotModule (1), (float) ModuleType::zapResonator);
    setValue (o, params::slotModule (2), (float) ModuleType::fraktalEcho);
    setValue (o, params::orbitRadius, 30.0f);
    setValue (o, params::orbitDoppler, 1.0f);
    setValue (o, params::orbitRate, 0.0f);
    setValue (o, params::slotModule (3), (float) ModuleType::tapeStop);
    setValue (o, params::tapeTrigger, 1.0f); // every 2 bars
    setValue (o, params::tapeMode, 1.0f);    // stop & start
    for (double rate : { 44100.0, 96000.0 })
    {
        o.prepareToPlay (rate, 512);
        check (healthy (run (o, rate, 512, (int) rate * 3)), "Orbit Doppler, Zap Resonator and Tape Stop stay bounded");
    }
}

void state()
{
    OriginPsyProcessor a;
    setValue (a, params::echoSpread, -3.0f);
    setValue (a, params::slotModule (3), (float) ModuleType::fraktalEcho);
    setValue (a, params::vokalVowel (9), 4.0f);
    a.setGatePattern (1, 0xA5A5u);

    juce::MemoryBlock data;
    a.getStateInformation (data);

    OriginPsyProcessor b;
    b.setStateInformation (data.getData(), (int) data.getSize());
    check ((int) b.apvts.getRawParameterValue (params::echoSpread)->load() == -3, "state restores echo spread");
    check ((int) b.apvts.getRawParameterValue (params::slotModule (3))->load() == (int) ModuleType::fraktalEcho,
           "state restores the chain");
    check ((int) b.apvts.getRawParameterValue (params::vokalVowel (9))->load() == 4, "state restores the vowel map");
    check (b.getGatePattern (1) == 0xA5A5u, "state restores drawn gate patterns");

    b.toggleGateStep (1, 0);
    check (b.getGatePattern (1) == 0xA5A4u, "clicking a step toggles it");
}


void modulation()
{
    OriginPsyProcessor p;
    setValue (p, params::slotModule (0), (float) ModuleType::fraktalEcho);
    setValue (p, params::echoMix, 0.0f);
    const int dest = p.modulatableIndex (params::echoMix);
    check (dest >= 0, "echo mix can be modulated");
    check (p.modulatableIndex (params::outGain) < 0 && p.modulatableIndex (params::macro (0)) < 0,
           "output gain and macros are not modulation targets");
    check (p.matrix.get (0).source == ModSource::macro1, "a new instance starts with the macros wired");
    p.matrix.clearAll();
    p.matrix.connect (ModSource::macro1, dest, 1.0f);
    setValue (p, params::macro (0), 0.6f);
    p.prepareToPlay (48000.0, 256);
    const auto out = run (p, 48000.0, 256, 4800);
    check (healthy (out), "modulated processor stays healthy");
    check (std::abs (p.modulatedNormalised (dest) - 0.6f) < 0.02f, "macro 1 at 60 % moves echo mix by 60 %");
    check (p.apvts.getRawParameterValue (params::echoMix)->load() < 0.001f, "modulation leaves the knob where it is");

    juce::MemoryBlock data;
    p.getStateInformation (data);
    OriginPsyProcessor b;
    b.setStateInformation (data.getData(), (int) data.getSize());
    const auto slot = b.matrix.get (0);
    check (slot.used() && slot.source == ModSource::macro1 && slot.destination == b.modulatableIndex (params::echoMix)
               && std::abs (slot.depth - 1.0f) < 1e-6f,
           "state restores the modulation matrix");

    // Direction is saved too.
    OriginPsyProcessor dir;
    dir.matrix.clearAll();
    const int ds = dir.matrix.connect (ModSource::lfo2, dir.modulatableIndex (params::echoDamp), -0.3f);
    dir.matrix.setBipolar (ds, false);
    juce::MemoryBlock dirData;
    dir.getStateInformation (dirData);
    OriginPsyProcessor dir2;
    dir2.setStateInformation (dirData.getData(), (int) dirData.getSize());
    check (! dir2.matrix.get (0).bipolar && dir2.matrix.get (0).source == ModSource::lfo2, "state restores the direction");

    // PHRASE as a source: the default curve rises through the phrase.
    OriginPsyProcessor ph;
    ph.matrix.clearAll();
    setValue (ph, params::frazaBars, 0.0f); // 8 bars = 13.7 s at 140 BPM
    setValue (ph, params::echoMix, 0.0f);
    const int phIdx = ph.modulatableIndex (params::echoMix);
    ph.matrix.connect (ModSource::fraza, phIdx, 1.0f);
    ph.prepareToPlay (48000.0, 512);
    run (ph, 48000.0, 512, 48000 * 7);
    const float phraseNow = ph.meters.frazaValue.load();
    check (phraseNow > 0.4f && phraseNow < 0.6f, "half way through the phrase the curve is at half");
    check (std::abs (ph.modulatedNormalised (phIdx) - phraseNow) < 0.02f, "PHRASE moves the knob it is dropped on");

    // Every modulatable parameter at once, with every source, stays bounded.
    OriginPsyProcessor all;
    all.applyDropPreset();
    setValue (all, params::slotModule (1), (float) ModuleType::vokalAcid);
    setValue (all, params::slotModule (2), (float) ModuleType::polimetarGate);
    const auto n = (int) all.getModulatableParameters().size();
    for (int i = 0; i < ModMatrix::numSlots; ++i)
        all.matrix.connect ((ModSource) (1 + i % (numModSources - 1)), (i * 7) % n, i % 2 == 0 ? 1.0f : -1.0f);
    for (int i = 0; i < 4; ++i) setValue (all, params::macro (i), 1.0f);
    all.prepareToPlay (44100.0, 512);
    check (healthy (run (all, 44100.0, 512, 44100 * 4)), "full modulation matrix stays bounded");
}

void reorder()
{
    OriginPsyProcessor p;
    setValue (p, params::slotModule (0), (float) ModuleType::vokalAcid);
    setValue (p, params::slotModule (2), (float) ModuleType::fraktalEcho);
    setValue (p, params::slotBypass (2), 1.0f);
    setValue (p, params::slotMix (2), 0.4f);
    setValue (p, params::slotSolo (0), 1.0f);
    p.swapSlots (0, 2);
    check (std::abs (p.apvts.getRawParameterValue (params::slotMix (0))->load() - 0.4f) < 0.01f
               && p.apvts.getRawParameterValue (params::slotSolo (2))->load() > 0.5f
               && p.apvts.getRawParameterValue (params::slotSolo (0))->load() < 0.5f,
           "Dry/Wet and Solo move with the module");
    check ((int) p.apvts.getRawParameterValue (params::slotModule (0))->load() == (int) ModuleType::fraktalEcho
               && (int) p.apvts.getRawParameterValue (params::slotModule (2))->load() == (int) ModuleType::vokalAcid,
           "swapping slots swaps the modules");
    check (p.apvts.getRawParameterValue (params::slotBypass (0))->load() > 0.5f
               && p.apvts.getRawParameterValue (params::slotBypass (2))->load() < 0.5f,
           "the on/off state moves with the module");
    p.prepareToPlay (48000.0, 256);
    check (healthy (run (p, 48000.0, 256, 24000)), "a reordered chain plays");
}

void randomizer()
{
    OriginPsyProcessor p;
    juce::Random rng (42);
    // Default chain: Vocal Acid, Polymeter Gate, Fractal Echo. Zap is not in it.
    const float zapBefore = p.apvts.getRawParameterValue (params::zapSweep)->load();
    const float slotBefore = p.apvts.getRawParameterValue (params::slotModule (0))->load();
    const float outBefore = p.apvts.getRawParameterValue (params::outGain)->load();
    PresetManager::randomize (p, false, rng);
    check (p.apvts.getRawParameterValue (params::zapSweep)->load() == zapBefore, "randomize leaves modules outside the chain alone");
    check (p.apvts.getRawParameterValue (params::slotModule (0))->load() == slotBefore
               && p.apvts.getRawParameterValue (params::outGain)->load() == outBefore,
           "randomize keeps the chain and the output level");
    const float mix = p.apvts.getRawParameterValue (params::echoMix)->load();
    check (mix >= 0.25f && mix <= 0.8f, "randomize keeps the mix usable");
    check (p.apvts.getRawParameterValue (params::echoFeedback)->load() <= 0.7f + 1e-4f, "randomize keeps feedback short of runaway");
    check (p.currentPresetName == "Random", "randomize names the result");

    const float damp = p.apvts.getRawParameterValue (params::echoDamp)->load();
    PresetManager::randomize (p, true, rng);
    check (std::abs (p.apvts.getRawParameterValue (params::echoDamp)->load() - damp) <= 0.081f, "mutate only nudges a setting");
    check (p.currentPresetName == "Random *", "mutate marks the name");

    // Many rolls with every module in the chain stay healthy.
    OriginPsyProcessor all;
    setValue (all, params::slotModule (0), (float) ModuleType::zapResonator);
    setValue (all, params::slotModule (1), (float) ModuleType::orbitDoppler);
    setValue (all, params::slotModule (2), (float) ModuleType::fraktalEcho);
    setValue (all, params::slotModule (3), (float) ModuleType::tapeStop);
    all.prepareToPlay (48000.0, 512);
    bool ok = true;
    // Spectral Ratchet too.
    OriginPsyProcessor ratchet;
    setValue (ratchet, params::slotModule (0), (float) ModuleType::spectralRatchet);
    const float ratchetMixBefore = ratchet.apvts.getRawParameterValue (params::ratchetMix)->load();
    const float ratchetSmearBefore = ratchet.apvts.getRawParameterValue (params::ratchetSmear)->load();
    PresetManager::randomize (ratchet, false, rng);
    check (ratchet.apvts.getRawParameterValue (params::ratchetMix)->load() != ratchetMixBefore
               || ratchet.apvts.getRawParameterValue (params::ratchetSmear)->load() != ratchetSmearBefore,
           "randomize reaches Spectral Ratchet");
    for (int i = 0; i < 12; ++i)
    {
        PresetManager::randomize (all, i % 3 == 2, rng);
        ok = ok && healthy (run (all, 48000.0, 512, 24000));
    }
    check (ok, "random settings stay healthy");
}

void latencyAndRatchet()
{
    OriginPsyProcessor p;
    p.prepareToPlay (48000.0, 256);
    check (p.getLatencySamples() == 0, "no latency without Spectral Ratchet");
    setValue (p, params::slotModule (3), (float) ModuleType::spectralRatchet);
    p.prepareToPlay (48000.0, 256);
    check (p.getLatencySamples() == SpectralRatchet::latency, "Spectral Ratchet reports its latency");
    check (healthy (run (p, 48000.0, 256, 48000 * 2)), "Spectral Ratchet in the chain stays bounded");

    p.setRatchetPattern (0x1234u);
    juce::MemoryBlock data;
    p.getStateInformation (data);
    OriginPsyProcessor b;
    b.setStateInformation (data.getData(), (int) data.getSize());
    check (b.getRatchetPattern() == 0x1234u, "state restores the ratchet steps");
}

void presets()
{
    OriginPsyProcessor p;
    PresetManager pm (p);
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("originpsy_preset_test");
    tmp.deleteRecursively();
    pm.setUserFolderForTests (tmp);

    const auto& factory = PresetManager::factoryPresets();
    check (factory.size() >= 24, "there are factory presets");
    for (const char* genre : { "Full-on", "Dark", "Prog" })
    {
        int count = 0;
        for (const auto& f : factory) count += juce::String (f.category) == genre ? 1 : 0;
        check (count >= 4, "every genre has at least four presets");
    }
    check (pm.list().size() == factory.size(), "no user presets in an empty folder");
    p.prepareToPlay (48000.0, 256);
    for (size_t i = 0; i < factory.size(); ++i)
    {
        check (pm.loadFactory ((int) i), "factory preset loads");
        check (p.currentPresetName == factory[i].name, "loading a preset sets its name");
        bool macro1Wired = false;
        for (int s = 0; s < ModMatrix::numSlots; ++s)
            macro1Wired = macro1Wired || p.matrix.get (s).source == ModSource::macro1;
        check (macro1Wired, "factory presets wire macro 1");
        const auto out = run (p, 48000.0, 256, 48000 * 2);
        if (! healthy (out)) std::printf ("  unhealthy preset: %s\n", factory[i].name);
        check (healthy (out), "factory preset output stays bounded");
    }

    // User preset round trip.
    pm.loadFactory (3);
    setValue (p, params::echoSpread, 5.0f);
    p.setGatePattern (2, 0xF0F0u);
    p.matrix.connect (ModSource::lfo2, p.modulatableIndex (params::vokalDrive), -0.4f);
    const auto file = pm.save ("My: Preset");
    check (file.existsAsFile(), "saving writes a preset file");
    check (file.getParentDirectory() == tmp, "user presets go to the user folder");

    pm.loadFactory (0);
    check ((int) p.apvts.getRawParameterValue (params::echoSpread)->load() != 5, "Init resets the parameters");
    const auto entries = pm.list();
    check (entries.size() == factory.size() + 1 && ! entries.back().factory, "the saved preset is listed as a user preset");
    check (pm.load (entries.back()), "user preset loads");
    check ((int) p.apvts.getRawParameterValue (params::echoSpread)->load() == 5, "user preset restores parameters");
    check (p.getGatePattern (2) == 0xF0F0u, "user preset restores gate steps");
    bool found = false;
    for (int s = 0; s < ModMatrix::numSlots; ++s)
    {
        const auto slot = p.matrix.get (s);
        found = found || (slot.source == ModSource::lfo2 && slot.destination == p.modulatableIndex (params::vokalDrive)
                          && std::abs (slot.depth + 0.4f) < 1e-6f);
    }
    check (found, "user preset restores modulation");
    check (p.currentPresetName == entries.back().name, "user preset sets its name");
    tmp.deleteRecursively();
}

void midiControl()
{
    OriginPsyProcessor p;
    check (p.acceptsMidi(), "the plugin takes MIDI");
    for (int i = 0; i < Chain::numSlots; ++i) setValue (p, params::slotModule (i), 0.0f);
    setValue (p, params::slotModule (0), (float) ModuleType::tapeStop);
    setValue (p, params::frazaOn, 0.0f);
    p.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buffer (2, 256);
    auto block = [&] (const juce::MidiBuffer& m)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 256; ++i) buffer.setSample (ch, i, 0.3f * std::sin ((float) i * 0.05f));
        juce::MidiBuffer copy (m);
        p.processBlock (buffer, copy);
        check (copy.isEmpty(), "MIDI is not passed on");
    };

    // Hold C1: the tape slows down; let go: it comes back.
    juce::MidiBuffer on, off, none;
    on.addEvent (juce::MidiMessage::noteOn (1, OriginPsyProcessor::midiNoteBrake, (juce::uint8) 100), 0);
    off.addEvent (juce::MidiMessage::noteOff (1, OriginPsyProcessor::midiNoteBrake), 0);
    block (on);
    for (int i = 0; i < 400; ++i) block (none);
    check (p.meters.midiBrake.load() && p.meters.tapeSpeed.load() < 0.5f, "holding C1 brakes the tape");
    block (off);
    for (int i = 0; i < 400; ++i) block (none);
    check (! p.meters.midiBrake.load() && p.meters.tapeSpeed.load() > 0.99f, "letting go of C1 brings the tape back");

    // D1: DROP.
    juce::MidiBuffer drop;
    drop.addEvent (juce::MidiMessage::noteOn (3, OriginPsyProcessor::midiNoteDrop, (juce::uint8) 100), 10);
    block (drop);
    p.flushMidiActions();
    check (p.apvts.getRawParameterValue (params::frazaOn)->load() > 0.5f, "D1 fires DROP");

    // Program change and next / previous preset.
    PresetManager pm (p);
    const auto entries = pm.list();
    juce::MidiBuffer pc;
    pc.addEvent (juce::MidiMessage::programChange (1, 2), 0);
    block (pc);
    p.flushMidiActions();
    check (p.currentPresetName == entries[2].name, "program change 2 loads the third preset");
    juce::MidiBuffer next;
    next.addEvent (juce::MidiMessage::noteOn (1, OriginPsyProcessor::midiNoteNext, (juce::uint8) 100), 0);
    block (next);
    p.flushMidiActions();
    check (p.currentPresetName == entries[3].name, "F1 loads the next preset");
    juce::MidiBuffer prev;
    prev.addEvent (juce::MidiMessage::noteOn (1, OriginPsyProcessor::midiNotePrev, (juce::uint8) 100), 0);
    prev.addEvent (juce::MidiMessage::noteOn (1, OriginPsyProcessor::midiNotePrev, (juce::uint8) 100), 5);
    block (prev);
    p.flushMidiActions();
    check (p.currentPresetName == entries[1].name, "two E1 notes step back two presets");
    check (p.meters.midiEvents.load() >= 6, "MIDI activity is counted for the display");
}

void sidechain()
{
    OriginPsyProcessor p;
    auto layout = p.getBusesLayout();
    check (layout.inputBuses.size() == 2, "there is a sidechain input");
    layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    check (p.setBusesLayout (layout), "the sidechain can be switched on");
    for (int i = 0; i < Chain::numSlots; ++i) setValue (p, params::slotModule (i), 0.0f);
    setValue (p, params::slotModule (0), (float) ModuleType::polimetarGate);
    for (int b = 0; b < 3; ++b)
    {
        setValue (p, params::gateBand (b, "mode"), 2.0f);
        setValue (p, params::gateBand (b, "depth"), 1.0f);
    }
    p.prepareToPlay (48000.0, 256);

    // Steady sine in, a kick every beat (120 BPM internal clock is irrelevant here) on the sidechain.
    juce::AudioBuffer<float> buffer (4, 256);
    juce::MidiBuffer midi;
    std::vector<float> out;
    float scPeak = 0.0f;
    for (int start = 0; start < 48000; start += 256)
    {
        for (int i = 0; i < 256; ++i)
        {
            const int n = start + i;
            const float v = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * n / 48000.0);
            const int k = n % 24000;
            const float kick = k < 6000 ? 0.9f * (float) std::exp (-k / 1500.0) * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 60.0 * k / 48000.0) : 0.0f;
            buffer.setSample (0, i, v);
            buffer.setSample (1, i, v);
            buffer.setSample (2, i, kick);
            buffer.setSample (3, i, kick);
        }
        p.processBlock (buffer, midi);
        scPeak = std::max (scPeak, p.meters.source[(size_t) ModSource::sidechain].load());
        for (int i = 0; i < 256; ++i) out.push_back (buffer.getSample (0, i));
    }
    auto rmsOf = [&out] (int from, int to)
    {
        double sum = 0.0;
        for (int i = from; i < to; ++i) sum += (double) out[(size_t) i] * out[(size_t) i];
        return std::sqrt (sum / (to - from));
    };
    check (p.meters.sidechainConnected.load(), "the processor sees the sidechain");
    check (rmsOf (24200, 25500) < 0.05, "a kick on the sidechain ducks the gate");
    check (rmsOf (16000, 23000) > 0.25, "the gate opens between kicks");
    check (scPeak > 0.5f, "the SC modulation source follows the kick");

    // Mono track with a stereo sidechain: the sidechain channels are left alone.
    OriginPsyProcessor mono;
    auto monoLayout = mono.getBusesLayout();
    monoLayout.inputBuses.getReference (0) = juce::AudioChannelSet::mono();
    monoLayout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
    monoLayout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    check (mono.setBusesLayout (monoLayout), "mono with a stereo sidechain is supported");
    setValue (mono, params::outGain, 12.0f);
    mono.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> mb (3, 256);
    for (int ch = 0; ch < 3; ++ch)
        for (int i = 0; i < 256; ++i) mb.setSample (ch, i, 0.5f * std::sin ((float) i * 0.1f));
    const float scBefore = mb.getSample (1, 10);
    for (int i = 0; i < 20; ++i) mono.processBlock (mb, midi);
    check (mb.getSample (1, 10) == scBefore, "the sidechain input is not processed as audio");
}

int screenshot (const juce::String& path, int module)
{
    OriginPsyProcessor p;
    p.prepareToPlay (48000.0, 256);
    run (p, 48000.0, 256, 48000);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    if (module > 0)
        setValue (p, params::slotModule (0), (float) module);
    editor->setSize (1040, 790);
    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    juce::File file (path);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat png;
    return png.writeImageToStream (image, stream) ? 0 : 1;
}
} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juce;

    if (argc >= 3 && juce::String (argv[1]) == "--screenshot")
        return screenshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]).getFullPathName(),
                           argc >= 4 ? juce::String (argv[3]).getIntValue() : 0);

    parameters();
    sampleRatesAndBlocks();
    emptyChainIsTransparent();
    slotMixSoloAndLimiter();
    allModulesAndFraza();
    state();
    modulation();
    presets();
    midiControl();
    sidechain();
    reorder();
    randomizer();
    latencyAndRatchet();

    if (failures == 0) std::printf ("All plugin tests passed.\n");
    else std::printf ("%d plugin test(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
