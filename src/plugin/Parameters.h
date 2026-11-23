#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace originpsy::params
{

// Parameter IDs. Kept in one place so the processor, editor and tests agree.
inline const juce::String outGain = "out_gain";

inline juce::String slotModule (int i) { return "slot" + juce::String (i + 1) + "_module"; }
inline juce::String slotBypass (int i) { return "slot" + juce::String (i + 1) + "_bypass"; }
inline juce::String slotMix (int i) { return "slot" + juce::String (i + 1) + "_mix"; }
inline juce::String slotSolo (int i) { return "slot" + juce::String (i + 1) + "_solo"; }
inline const juce::String outLimit = "out_limit";
inline const juce::String outCeiling = "out_ceiling";

inline const juce::String echoTime = "echo_time";
inline const juce::String echoRatio = "echo_ratio";
inline const juce::String echoGenerations = "echo_generations";
inline const juce::String echoScale = "echo_scale";
inline const juce::String echoSpread = "echo_spread";
inline const juce::String echoFeedback = "echo_feedback";
inline const juce::String echoDamp = "echo_damp";
inline const juce::String echoWidth = "echo_width";
inline const juce::String echoMix = "echo_mix";

inline juce::String vokalVowel (int pitchClass) { return "vokal_vowel_" + juce::String (pitchClass); }
inline const juce::String vokalGlide = "vokal_glide";
inline const juce::String vokalShift = "vokal_shift";
inline const juce::String vokalResonance = "vokal_resonance";
inline const juce::String vokalAccent = "vokal_accent";
inline const juce::String vokalCharacter = "vokal_character";
inline const juce::String vokalDrive = "vokal_drive";
inline const juce::String vokalMix = "vokal_mix";

inline const juce::String gateLow = "gate_low_hz";
inline const juce::String gateHigh = "gate_high_hz";
inline const juce::String gateMix = "gate_mix";
inline juce::String gateBand (int band, const char* name) { return "gate_b" + juce::String (band + 1) + "_" + name; }

inline const juce::String zapThreshold = "zap_threshold";
inline const juce::String zapScale = "zap_scale";
inline const juce::String zapRoot = "zap_root";
inline const juce::String zapOctave = "zap_octave";
inline const juce::String zapOrder = "zap_order";
inline const juce::String zapSweep = "zap_sweep";
inline const juce::String zapSweepTime = "zap_sweep_time";
inline const juce::String zapDecay = "zap_decay";
inline const juce::String zapResonance = "zap_resonance";
inline const juce::String zapWidth = "zap_width";
inline const juce::String zapMix = "zap_mix";

inline const juce::String orbitPath = "orbit_path";
inline const juce::String orbitRate = "orbit_rate";
inline const juce::String orbitRadius = "orbit_radius";
inline const juce::String orbitDoppler = "orbit_doppler";
inline const juce::String orbitTone = "orbit_tone";
inline const juce::String orbitWidth = "orbit_width";
inline const juce::String orbitMix = "orbit_mix";

inline const juce::String tapeMode = "tape_mode";
inline const juce::String tapeTrigger = "tape_trigger";
inline const juce::String tapeBrake = "tape_brake";
inline const juce::String tapeLength = "tape_length";
inline const juce::String tapeCurve = "tape_curve";
inline const juce::String tapeMix = "tape_mix";

inline juce::String lfoRate (int i) { return "lfo" + juce::String (i + 1) + "_rate"; }
inline juce::String lfoShape (int i) { return "lfo" + juce::String (i + 1) + "_shape"; }
inline juce::String macro (int i) { return "macro" + juce::String (i + 1); }

inline const juce::String frazaOn = "fraza_on";
inline const juce::String frazaBars = "fraza_bars";
inline const juce::String frazaOffset = "fraza_offset";
inline const juce::String frazaCurve = "fraza_curve";

/** Where the Fraza curve can go, and how far (amount -1..1 of the range). */
struct FrazaTarget
{
    const char* id;
    const char* label;
    float dropAmount; // what the Drop button sets
};

inline constexpr FrazaTarget frazaTargets[] = {
    { "fraza_to_echo_mix",      "Echo Mix",      0.45f },
    { "fraza_to_echo_feedback", "Echo Feedback", 0.40f },
    { "fraza_to_echo_damp",     "Echo Damp",    -0.50f },
    { "fraza_to_vokal_shift",   "Vocal Shift",   0.35f },
    { "fraza_to_vokal_res",     "Vocal Res",     0.40f },
    { "fraza_to_gate_depth",    "Gate Depth",    0.60f },
};
inline constexpr int numFrazaTargets = (int) (sizeof (frazaTargets) / sizeof (frazaTargets[0]));

juce::StringArray divisionNames();
juce::StringArray scaleNames();
juce::StringArray vowelNames();
juce::StringArray moduleNames();
juce::StringArray frazaCurveNames();
juce::StringArray lfoRateNames();
juce::StringArray lfoShapeNames();
juce::StringArray noteNames();
juce::StringArray zapOrderNames();
juce::StringArray orbitPathNames();

/** Parameters that the modulation matrix may not target (the output level,
    the macros themselves and the Fraza amounts). */
bool isModulatable (const juce::RangedAudioParameter&);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace originpsy::params
