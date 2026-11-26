#pragma once

#include "Parameters.h"
#include "dsp/Chain.h"
#include "dsp/DryWet.h"
#include "dsp/Fraza.h"
#include "dsp/Limiter.h"
#include "dsp/Modulation.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <unordered_map>
#include <vector>

class OriginPsyProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    OriginPsyProcessor();
    ~OriginPsyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Drawn gate patterns (Steps mode). Not host parameters: a 32 step
    // pattern does not fit a float. Saved with the project.
    uint32_t getGatePattern (int band) const noexcept { return gatePatterns[(size_t) band].load(); }
    void setGatePattern (int band, uint32_t pattern);
    void toggleGateStep (int band, int step);

    /** Swaps two slots of the chain (module and on/off), e.g. after a drag. */
    void swapSlots (int a, int b);
    // Spectral Ratchet freeze steps (bit set = freeze), saved like the gate patterns.
    uint32_t getRatchetPattern() const noexcept { return ratchetPattern.load(); }
    void setRatchetPattern (uint32_t pattern) noexcept { ratchetPattern = pattern & 0xffffu; }
    void toggleRatchetStep (int step) noexcept { if (step >= 0 && step < 16) ratchetPattern ^= (1u << step); }

    // Hand-drawn Phrase curve (the Custom curve). Not host parameters: saved
    // with the project and with presets, like the gate patterns.
    float getFrazaCustomPoint (int i) const noexcept { return frazaCustom[(size_t) i].load(); }
    void setFrazaCustomPoint (int i, float v) noexcept;
    void getFrazaCustom (float* out) const noexcept;
    void setFrazaCustom (const float* in) noexcept;
    /** Straight rise from 0 to 1, the shape a new Custom curve starts with. */
    void resetFrazaCustom() noexcept;

    // Hand-drawn Tape Stop slow down (speeds 1..0 over the length), used when
    // Tape Drawn Curve is on. Saved with the project and presets.
    float getTapeDrawnPoint (int i) const noexcept { return tapeDrawn[(size_t) i].load(); }
    void setTapeDrawnPoint (int i, float v) noexcept;
    /** Back to the default slow-down shape. */
    void resetTapeDrawn() noexcept;
    /** If the drawn curve was never used, draws the shape the old Curve knob gave. */
    void adoptTapeCurveKnob() noexcept;

    // Glide pattern: which steps glide (bit set = glide) and each step's note
    // in semitones. Saved with the project and presets.
    uint32_t getGlidePattern() const noexcept { return glidePattern.load(); }
    void setGlidePattern (uint32_t pattern) noexcept { glidePattern = pattern & 0xffffu; }
    void toggleGlideStep (int step) noexcept { if (step >= 0 && step < 16) glidePattern ^= (1u << step); }
    int getGlideNote (int step) const noexcept { return glideNotes[(size_t) step].load(); }
    void setGlideNote (int step, int semitones) noexcept;
    // Hand-drawn glide shape, used when Glide Drawn Curve is on.
    float getGlideDrawnPoint (int i) const noexcept { return glideDrawn[(size_t) i].load(); }
    void setGlideDrawnPoint (int i, float v) noexcept;
    /** Back to the default Curve knob shape. */
    void resetGlideDrawn() noexcept;

    /** Sets the Fraza amounts to a ready made build-up (the Drop button). */
    void applyDropPreset();

    // MIDI control (any channel). Notes: C1 (36) held = BRAKE, D1 (38) = DROP,
    // E1 (40) = previous preset, F1 (41) = next preset, G1 (43) held = Glide
    // DIVE. Program Change n loads the n-th preset of the list (factory
    // presets first, then the user's).
    static constexpr int midiNoteBrake = 36, midiNoteDrop = 38, midiNotePrev = 40, midiNoteNext = 41, midiNoteDive = 43;
    /** Runs DROP and preset changes asked for by MIDI now (normally they run
        on the message thread right after the block). */
    void flushMidiActions() { midiActions.handleUpdateNowIfNeeded(); }

    // Read by the editor for its displays.
    struct Meters
    {
        std::atomic<int> vokalNote { -1 };
        std::atomic<float> vokalVowel { 0.0f };
        std::atomic<float> vokalAccent { 0.0f };
        std::array<std::atomic<int>, 3> gateStep { { -1, -1, -1 } };
        std::atomic<float> tapeSpeed { 1.0f };
        std::atomic<int> ratchetStep { -1 };
        std::atomic<bool> ratchetFreezing { false };
        std::array<std::atomic<float>, originpsy::SpectralRatchet::numBands> ratchetBands {};
        std::atomic<int> zapNote { -1 };
        std::atomic<uint32_t> zapHits { 0 };
        std::atomic<float> zapLevel { 0.0f };
        std::atomic<float> orbitX { 0.0f }, orbitY { 1.0f };
        std::atomic<double> frazaPosition { 0.0 };
        std::atomic<float> frazaValue { 0.0f };
        std::atomic<double> bpm { 140.0 };
        std::atomic<float> limiterDb { 0.0f };
        std::atomic<float> tapeFx { 0.0f };
        std::atomic<bool> hostPlaying { false };
        std::atomic<uint32_t> midiEvents { 0 };   // counts MIDI messages that did something
        std::atomic<bool> midiBrake { false };
        std::atomic<bool> midiDive { false };
        std::atomic<float> glidePitch { 0.0f };
        std::atomic<float> glideDive { 0.0f };
        std::atomic<int> glideStep { -1 };
        std::atomic<bool> sidechainConnected { false };
        std::array<std::atomic<float>, originpsy::numModSources> source {};
    } meters;

    //==============================================================================
    // Modulation matrix. Destinations are indices into getModulatableParameters().
    originpsy::ModMatrix matrix;
    const std::vector<juce::RangedAudioParameter*>& getModulatableParameters() const noexcept { return modParams; }
    int modulatableIndex (const juce::String& paramId) const
    {
        const auto it = modIndex.find (paramId);
        return it == modIndex.end() ? -1 : it->second;
    }
    /** The value a modulatable parameter really has after modulation (0..1). */
    float modulatedNormalised (int index) const noexcept { return modNorm[(size_t) index].load (std::memory_order_relaxed); }

    //==============================================================================
    // Presets: the whole state, including gate patterns and modulation.
    juce::ValueTree createStateTree();
    void loadStateTree (const juce::ValueTree& state);
    juce::String currentPresetName { "Init" };

    /** The tree of echoes as it is set now (for the editor's drawing). */
    int getEchoTaps (std::array<originpsy::FraktalEcho::Tap, originpsy::FraktalEcho::maxTaps>& out) const;

private:
    originpsy::Transport readTransport (int numSamples);
    void updateModules();
    void computeModulation (const originpsy::Transport&, float fraza, const float* left, const float* right, int numSamples,
                            const float* key);
    float value (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }
    float m (const juce::String& id) const; // value after modulation
    void handleMidi (const juce::MidiBuffer&);

    /** DROP and preset changes touch many parameters, so MIDI only asks for
        them and they run on the message thread. */
    struct MidiActions : juce::AsyncUpdater
    {
        explicit MidiActions (OriginPsyProcessor& p) : proc (p) {}
        void handleAsyncUpdate() override;
        OriginPsyProcessor& proc;
        std::atomic<bool> drop { false };
        std::atomic<int> program { -1 }, step { 0 };
    } midiActions { *this };
    bool midiBrake = false, midiDive = false;

    originpsy::Chain chain;
    originpsy::InternalClock clock;
    double sampleRate = 48000.0;
    juce::SmoothedValue<float> outGain;
    std::vector<float> monoScratch;
    originpsy::EnvelopeFollower envelope, keyEnvelope;
    std::vector<float> keyScratch;
    originpsy::Limiter limiter;
    originpsy::DryWet dryWet;
    std::vector<juce::RangedAudioParameter*> modParams;
    std::unordered_map<juce::String, int> modIndex;
    std::unique_ptr<std::atomic<float>[]> modNorm;
    std::vector<int> frazaTargetOf;
    std::array<std::atomic<uint32_t>, 3> gatePatterns { { 0x1111u, 0x15u, 0x1249u } };
    std::atomic<uint32_t> ratchetPattern { 0xEEEEu };
    std::array<std::atomic<float>, originpsy::Fraza::numCustomPoints> frazaCustom {};
    std::array<std::atomic<float>, originpsy::TapeStop::numDrawnPoints> tapeDrawn {};
    std::array<float, originpsy::TapeStop::numDrawnPoints> tapeDrawnBlock {}; // audio thread copy
    std::atomic<uint32_t> glidePattern { 0x8888u };
    std::array<std::atomic<int>, originpsy::Glide::maxSteps> glideNotes {};
    std::array<std::atomic<float>, originpsy::Glide::numDrawnPoints> glideDrawn {};
    std::array<float, originpsy::Glide::numDrawnPoints> glideDrawnBlock {}; // audio thread copy
    std::atomic<int> wantedLatency { 0 };
    void handleAsyncUpdate() override { setLatencySamples (wantedLatency.load()); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OriginPsyProcessor)
};
