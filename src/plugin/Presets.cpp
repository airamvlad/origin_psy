#include "Presets.h"

#include "Parameters.h"
#include "PluginProcessor.h"

namespace originpsy
{

namespace
{
void set (OriginPsyProcessor& p, const juce::String& id, float value)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void connect (OriginPsyProcessor& p, ModSource source, const juce::String& id, float depth)
{
    const int dest = p.modulatableIndex (id);
    if (dest >= 0) p.matrix.connect (source, dest, depth);
}

void slots (OriginPsyProcessor& p, ModuleType a, ModuleType b, ModuleType c, ModuleType d = ModuleType::empty)
{
    const ModuleType types[] = { a, b, c, d };
    for (int i = 0; i < 4; ++i) set (p, params::slotModule (i), (float) types[i]);
}

void macros (OriginPsyProcessor& p) { PresetManager::wireMacros (p); }
} // namespace

void PresetManager::wireMacros (OriginPsyProcessor& p)
{
    connect (p, ModSource::macro1, params::echoMix, 0.5f);
    connect (p, ModSource::macro1, params::echoFeedback, 0.35f);
    connect (p, ModSource::macro2, params::vokalResonance, 0.45f);
    connect (p, ModSource::macro3, params::gateBand (1, "depth"), 0.6f);
    connect (p, ModSource::macro3, params::gateBand (2, "depth"), 0.6f);
}

const std::vector<PresetManager::Factory>& PresetManager::factoryPresets()
{
    static const std::vector<Factory> presets {
        { "Init", "Init", [] (OriginPsyProcessor& p) { macros (p); } },

        { "Talking Acid", "Bass", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::vokalAcid, ModuleType::fraktalEcho, ModuleType::empty);
              set (p, params::vokalGlide, 40.0f);
              set (p, params::vokalResonance, 0.65f);
              set (p, params::vokalAccent, 0.8f);
              set (p, params::vokalMix, 1.0f);
              set (p, params::echoTime, 3.0f);   // 1/16D
              set (p, params::echoMix, 0.18f);
              set (p, params::echoSpread, 0.0f);
              macros (p);
              connect (p, ModSource::lfo2, params::vokalShift, 0.12f);
          } },

        { "Robot Rolling Bass", "Bass", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::vokalAcid, ModuleType::polimetarGate, ModuleType::empty);
              set (p, params::vokalCharacter, 2.0f);
              set (p, params::vokalDrive, 0.45f);
              set (p, params::vokalResonance, 0.55f);
              set (p, params::gateBand (0, "depth"), 0.35f);
              set (p, params::gateBand (0, "steps"), 4.0f);
              set (p, params::gateBand (0, "hits"), 3.0f);
              set (p, params::gateBand (0, "rotate"), 1.0f);
              set (p, params::gateBand (1, "depth"), 0.8f);
              set (p, params::gateBand (2, "depth"), 0.8f);
              macros (p);
              connect (p, ModSource::envelope, params::vokalShift, 0.2f);
          } },

        { "Fractal Lead Tail", "Lead FX", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::fraktalEcho, ModuleType::empty, ModuleType::empty);
              set (p, params::echoTime, 5.0f);   // 1/8
              set (p, params::echoGenerations, 4.0f);
              set (p, params::echoScale, 1.0f);  // Hijaz
              set (p, params::echoSpread, 2.0f);
              set (p, params::echoFeedback, 0.5f);
              set (p, params::echoDamp, 0.35f);
              set (p, params::echoWidth, 0.9f);
              set (p, params::echoMix, 0.4f);
              macros (p);
              connect (p, ModSource::lfo2, params::echoDamp, 0.3f);
          } },

        { "Psy Zap Cascade", "Lead FX", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::fraktalEcho, ModuleType::empty, ModuleType::empty);
              set (p, params::echoTime, 2.0f);   // 1/16
              set (p, params::echoRatio, 0.5f);
              set (p, params::echoGenerations, 4.0f);
              set (p, params::echoSpread, 4.0f);
              set (p, params::echoFeedback, 0.6f);
              set (p, params::echoDamp, 0.15f);
              set (p, params::echoMix, 0.5f);
              macros (p);
          } },

        { "Laser Hats", "Glitch", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::zapResonator, ModuleType::fraktalEcho, ModuleType::empty);
              set (p, params::zapThreshold, -36.0f);
              set (p, params::zapOrder, 2.0f);        // random
              set (p, params::zapSweep, 30.0f);
              set (p, params::zapSweepTime, 60.0f);
              set (p, params::zapDecay, 220.0f);
              set (p, params::zapMix, 0.45f);
              set (p, params::echoTime, 3.0f);        // 1/16D
              set (p, params::echoMix, 0.25f);
              macros (p);
              connect (p, ModSource::lfo2, params::zapSweep, 0.15f);
          } },

        { "Zap Rain", "Lead FX", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::zapResonator, ModuleType::orbitDoppler, ModuleType::fraktalEcho);
              set (p, params::zapScale, 1.0f);        // Hijaz
              set (p, params::zapSweep, -24.0f);      // rises onto the note
              set (p, params::zapSweepTime, 150.0f);
              set (p, params::zapDecay, 600.0f);
              set (p, params::zapResonance, 1.0f);
              set (p, params::zapMix, 0.5f);
              set (p, params::orbitPath, 2.0f);       // spiral
              set (p, params::orbitRate, 8.0f);       // 4 bars
              set (p, params::orbitMix, 0.7f);
              set (p, params::echoMix, 0.3f);
              macros (p);
          } },

        { "Orbit Fly-by", "Lead FX", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::orbitDoppler, ModuleType::fraktalEcho, ModuleType::empty);
              set (p, params::orbitPath, 3.0f);       // fly-by
              set (p, params::orbitRate, 7.0f);       // 2 bars
              set (p, params::orbitRadius, 12.0f);
              set (p, params::orbitDoppler, 0.7f);
              set (p, params::orbitMix, 1.0f);
              set (p, params::echoMix, 0.2f);
              macros (p);
              connect (p, ModSource::macro4, params::orbitDoppler, 0.3f);
          } },

        { "Polymeter Pad", "Pads", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::polimetarGate, ModuleType::fraktalEcho, ModuleType::empty);
              set (p, params::gateBand (0, "depth"), 0.3f);
              set (p, params::gateBand (1, "length"), 0.8f);
              set (p, params::gateBand (2, "length"), 0.4f);
              set (p, params::gateBand (1, "release"), 60.0f);
              set (p, params::echoMix, 0.25f);
              set (p, params::echoTime, 6.0f);   // 1/8D
              macros (p);
              connect (p, ModSource::lfo1, params::gateBand (2, "length"), 0.3f);
          } },

        { "Glitch Gate", "Glitch", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::polimetarGate, ModuleType::empty, ModuleType::empty);
              for (int b = 0; b < 3; ++b)
              {
                  set (p, params::gateBand (b, "rate"), 0.0f); // 1/32
                  set (p, params::gateBand (b, "depth"), 1.0f);
                  set (p, params::gateBand (b, "release"), 3.0f);
              }
              set (p, params::gateBand (0, "steps"), 11.0f);
              set (p, params::gateBand (1, "steps"), 9.0f);
              set (p, params::gateBand (2, "steps"), 7.0f);
              set (p, params::lfoShape (0), 5.0f);  // random
              set (p, params::lfoRate (0), 6.0f);   // 1 bar
              macros (p);
              connect (p, ModSource::lfo1, params::gateBand (2, "rotate"), 0.4f);
          } },

        { "Build-up 16", "Build-up", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::vokalAcid, ModuleType::polimetarGate, ModuleType::fraktalEcho);
              p.applyDropPreset();
              set (p, params::frazaCurve, 4.0f);      // last 4 bars
              set (p, params::echoSpread, 3.0f);
              macros (p);
              connect (p, ModSource::fraza, params::gateBand (2, "length"), -0.4f);
          } },

        { "Tape Drop 8", "Build-up", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::fraktalEcho, ModuleType::tapeStop, ModuleType::empty);
              set (p, params::tapeMode, 0.0f);        // stop
              set (p, params::tapeTrigger, 3.0f);     // every 8 bars
              set (p, params::tapeLength, 4.0f);      // 1 bar
              set (p, params::tapeCurve, 0.65f);
              set (p, params::echoMix, 0.2f);
              macros (p);
          } },

        { "Long Rise 32", "Build-up", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::polimetarGate, ModuleType::fraktalEcho, ModuleType::empty);
              set (p, params::frazaBars, 2.0f);       // 32 bars
              set (p, params::frazaCurve, 1.0f);      // rise (late)
              set (p, params::frazaTargets[0].id, 0.5f);
              set (p, params::frazaTargets[1].id, 0.45f);
              set (p, params::frazaTargets[5].id, 0.8f);
              macros (p);
              connect (p, ModSource::fraza, params::echoSpread, 0.25f);
          } },
    };
    return presets;
}

void PresetManager::randomize (OriginPsyProcessor& p, bool mutateOnly, juce::Random& rng)
{
    // Parameter ID prefix of each module.
    const std::pair<ModuleType, const char*> prefixes[] = {
        { ModuleType::fraktalEcho, "echo_" }, { ModuleType::vokalAcid, "vokal_" }, { ModuleType::polimetarGate, "gate_" },
        { ModuleType::zapResonator, "zap_" }, { ModuleType::orbitDoppler, "orbit_" }, { ModuleType::tapeStop, "tape_" },
    };
    juce::StringArray active;
    for (int i = 0; i < 4; ++i)
    {
        const auto type = (ModuleType) (int) p.apvts.getRawParameterValue (params::slotModule (i))->load();
        const bool bypassed = p.apvts.getRawParameterValue (params::slotBypass (i))->load() > 0.5f;
        if (bypassed) continue;
        for (const auto& [t, prefix] : prefixes)
            if (t == type) active.add (prefix);
    }
    if (active.isEmpty())
        for (const auto& [t, prefix] : prefixes) active.add (prefix);

    for (auto* param : p.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr) continue;
        const auto& id = ranged->paramID;
        bool mine = false;
        for (const auto& prefix : active) mine = mine || id.startsWith (prefix);
        if (! mine || id == params::tapeBrake || id == params::tapeTrigger) continue;

        const bool isChoice = dynamic_cast<juce::AudioParameterChoice*> (ranged) != nullptr
                              || dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr;
        float norm = ranged->getValue();
        if (mutateOnly)
        {
            if (isChoice)
            {
                if (rng.nextFloat() < 0.2f)
                {
                    const int steps = std::max (2, ranged->getNumSteps());
                    norm = juce::jlimit (0.0f, 1.0f, norm + (rng.nextBool() ? 1.0f : -1.0f) / (float) (steps - 1));
                }
            }
            else
            {
                norm = juce::jlimit (0.0f, 1.0f, norm + (rng.nextFloat() - 0.5f) * 0.16f);
            }
        }
        else
        {
            norm = rng.nextFloat();
        }

        // Keep the sound usable: mixes in the middle, feedback and resonance short of runaway.
        float lo = 0.0f, hi = 1.0f;
        if (id.endsWith ("_mix")) { lo = 0.25f; hi = 0.8f; }
        else if (id == params::echoFeedback) hi = 0.7f;
        else if (id.endsWith ("_depth")) lo = 0.3f;
        const float real = juce::jlimit (ranged->convertFrom0to1 (lo), ranged->convertFrom0to1 (hi), ranged->convertFrom0to1 (norm));
        ranged->setValueNotifyingHost (ranged->convertTo0to1 (real));
    }

    if (mutateOnly)
    {
        if (! p.currentPresetName.endsWith ("*")) p.currentPresetName += " *";
    }
    else
    {
        p.currentPresetName = "Random";
    }
}

juce::File PresetManager::userFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("AMV")
        .getChildFile ("OriginPsy Presets");
}

std::vector<PresetManager::Entry> PresetManager::list() const
{
    std::vector<Entry> out;
    const char* order[] = { "Init", "Build-up", "Lead FX", "Bass", "Pads", "Glitch" };
    const auto& factory = factoryPresets();
    for (const char* category : order)
        for (size_t i = 0; i < factory.size(); ++i)
            if (juce::String (factory[i].category) == category)
                out.push_back ({ factory[i].name, category, true, (int) i, {} });

    auto files = folder().findChildFiles (juce::File::findFiles, true, juce::String ("*") + extension);
    files.sort();
    for (const auto& f : files)
        out.push_back ({ f.getFileNameWithoutExtension(), "User", false, -1, f });
    return out;
}

void PresetManager::resetToDefaults (OriginPsyProcessor& p)
{
    for (auto* param : p.getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());
    p.matrix.clearAll();
    p.setGatePattern (0, 0x1111u);
    p.setGatePattern (1, 0x15u);
    p.setGatePattern (2, 0x1249u);
}

bool PresetManager::loadFactory (int index)
{
    const auto& factory = factoryPresets();
    if (index < 0 || index >= (int) factory.size()) return false;
    resetToDefaults (proc);
    factory[(size_t) index].apply (proc);
    proc.currentPresetName = factory[(size_t) index].name;
    return true;
}

bool PresetManager::loadFile (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (proc.apvts.state.getType())) return false;
    proc.loadStateTree (state);
    proc.currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

bool PresetManager::load (const Entry& e)
{
    return e.factory ? loadFactory (e.factoryIndex) : loadFile (e.file);
}

juce::File PresetManager::save (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim().isEmpty() ? juce::String ("Preset") : name.trim());
    const auto dir = folder();
    dir.createDirectory();
    const auto file = dir.getChildFile (clean + extension);
    proc.currentPresetName = clean;
    if (auto xml = proc.createStateTree().createXml())
        if (xml->writeTo (file))
            return file;
    return {};
}

} // namespace originpsy
