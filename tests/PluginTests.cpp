// End-to-end checks of the plugin processor, without a DAW.
// `originpsy_plugin_tests --screenshot out.png [module]` saves the editor as a PNG.

#include "plugin/Parameters.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

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
    check (all.size() >= 77, "all module parameters are exposed to the host");
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
    p.prepareToPlay (48000.0, 256);
    const auto out = run (p, 48000.0, 256, 48000 * 8);
    check (healthy (out), "all modules with Drop and maximum settings stay bounded");
    check (p.apvts.getRawParameterValue (params::frazaTargets[0].id)->load() > 0.1f, "Drop sets the Fraza amounts");
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

int screenshot (const juce::String& path, int module)
{
    OriginPsyProcessor p;
    p.prepareToPlay (48000.0, 256);
    run (p, 48000.0, 256, 48000);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    if (module > 0)
        setValue (p, params::slotModule (0), (float) module);
    editor->setSize (1040, 700);
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

    if (failures == 0) std::printf ("All plugin tests passed.\n");
    else std::printf ("%d plugin test(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
