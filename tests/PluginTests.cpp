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
    check (all.size() >= 103, "all module parameters are exposed to the host");
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
    for (double rate : { 44100.0, 96000.0 })
    {
        o.prepareToPlay (rate, 512);
        check (healthy (run (o, rate, 512, (int) rate * 3)), "Orbit Doppler and Zap Resonator stay bounded");
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

void presets()
{
    OriginPsyProcessor p;
    PresetManager pm (p);
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("originpsy_preset_test");
    tmp.deleteRecursively();
    pm.setUserFolderForTests (tmp);

    const auto& factory = PresetManager::factoryPresets();
    check (factory.size() >= 8, "there are factory presets");
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
    allModulesAndFraza();
    state();
    modulation();
    presets();

    if (failures == 0) std::printf ("All plugin tests passed.\n");
    else std::printf ("%d plugin test(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
