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
