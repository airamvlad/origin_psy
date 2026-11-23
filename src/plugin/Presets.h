#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

class OriginPsyProcessor;

namespace originpsy
{

/** Factory presets (built into the plugin) and user presets (files in
    Documents/AMV/OriginPsy Presets). A preset is the whole state: parameters,
    drawn gate steps and the modulation matrix. */
class PresetManager
{
public:
    struct Factory
    {
        const char* name;
        const char* category;
        std::function<void (OriginPsyProcessor&)> apply;
    };

    struct Entry
    {
        juce::String name, category;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    explicit PresetManager (OriginPsyProcessor& p) : proc (p) {}

    static const std::vector<Factory>& factoryPresets();
    static juce::File userFolder();
    static constexpr const char* extension = ".originpsy";

    /** Factory presets by category, then the user's presets (subfolders included). */
    std::vector<Entry> list() const;

    bool load (const Entry&);
    bool loadFactory (int index);
    bool loadFile (const juce::File&);
    /** Saves the current state as a user preset; returns the file written. */
    juce::File save (const juce::String& name);

    /** Every parameter to its default, no modulation, default gate steps. */
    static void resetToDefaults (OriginPsyProcessor&);

    /** The macro wiring every factory preset and a new instance start with:
        M1 Space (echo), M2 Talk (vokal), M3 Chop (gate). M4 is free. */
    static void wireMacros (OriginPsyProcessor&);

    /** New random settings for the modules in the chain (mutate = only nudge
        them a little). Slots, output, macros, modulation and the Brake button
        are left alone; mixes and feedback stay in a usable range. */
    static void randomize (OriginPsyProcessor&, bool mutateOnly, juce::Random&);

    void setUserFolderForTests (const juce::File& f) { overrideFolder = f; }

private:
    juce::File folder() const { return overrideFolder != juce::File() ? overrideFolder : userFolder(); }

    OriginPsyProcessor& proc;
    juce::File overrideFolder;
};

} // namespace originpsy
