#include "Parameters.h"

#include "dsp/Chain.h"
#include "dsp/Fraza.h"
#include "dsp/Modulation.h"
#include "dsp/Scales.h"

namespace originpsy::params
{

juce::StringArray divisionNames()
{
    juce::StringArray s;
    for (const auto& d : noteDivisions) s.add (d.name);
    return s;
}

juce::StringArray scaleNames()
{
    juce::StringArray s;
    for (const auto& sc : scales) s.add (sc.name);
    return s;
}

juce::StringArray vowelNames() { return { "A", "E", "I", "O", "U" }; }

juce::StringArray moduleNames()
{
    juce::StringArray s;
    for (int i = 0; i < numModuleTypes; ++i) s.add (moduleName ((ModuleType) i));
    return s;
}

juce::StringArray frazaCurveNames()
{
    return { "Rise", "Rise (late)", "Last bar", "Last 2 bars", "Last 4 bars", "Fall", "Every 4th bar", "Custom" };
}

juce::StringArray lfoRateNames()
{
    juce::StringArray s;
    for (const auto& r : Lfo::rates) s.add (r.name);
    return s;
}

juce::StringArray lfoShapeNames() { return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Random" }; }

juce::StringArray noteNames() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
juce::StringArray zapOrderNames() { return { "Up", "Down", "Random", "Hit strength" }; }
juce::StringArray orbitPathNames() { return { "Circle", "Figure 8", "Spiral", "Fly-by" }; }
juce::StringArray glideModeNames() { return { "Slide", "Bend in", "Bend out", "Scoop" }; }
juce::StringArray glideDiveNames()
{
    return { "Off", "Dive button", "Every bar", "Every 2 bars", "Every 4 bars", "Every 8 bars", "Every 16 bars" };
}

bool isModulatable (const juce::RangedAudioParameter& p)
{
    const auto& id = p.paramID;
    if (id == outGain || id == outCeiling || id == tapeCurve || id.startsWith ("macro") || id.startsWith ("fraza_to_") || id.startsWith ("lfo"))
        return false;
    return dynamic_cast<const juce::AudioParameterFloat*> (&p) != nullptr
        || dynamic_cast<const juce::AudioParameterInt*> (&p) != nullptr;
}

namespace
{
using Range = juce::NormalisableRange<float>;

juce::String formatValue (float v, const juce::String& unit)
{
    if (unit == "Hz")
        return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
    if (unit == "ms")
        return juce::String (v, v < 10.0f ? 1 : 0) + " ms";
    if (unit == "st" || unit == "dB")
        return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " " + unit;
    if (unit == "m")
        return juce::String (v, 1) + " m";
    if (unit == "% step")
        return juce::String (juce::roundToInt (v * 100.0f)) + " % step";
    if (unit == "amount")
        return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %";
    return juce::String (v, 3);
}

std::unique_ptr<juce::AudioParameterFloat> flt (const juce::String& id, const juce::String& name, Range range,
                                                float def, const juce::String& unit = {})
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, range, def,
        juce::AudioParameterFloatAttributes()
            .withLabel (unit == "amount" || unit == "% step" ? juce::String() : unit)
            .withStringFromValueFunction ([unit] (float v, int) { return formatValue (v, unit); })
            .withValueFromStringFunction ([unit] (const juce::String& t) {
                const float v = t.getFloatValue();
                if (unit == "amount" || unit == "% step") return v / 100.0f;
                if (unit == "Hz" && t.containsIgnoreCase ("k")) return v * 1000.0f;
                return v;
            }));
}

std::unique_ptr<juce::AudioParameterFloat> percent (const juce::String& id, const juce::String& name, float def)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, Range (0.0f, 1.0f), def,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
            .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue() / 100.0f; }));
}

std::unique_ptr<juce::AudioParameterChoice> choice (const juce::String& id, const juce::String& name,
                                                    const juce::StringArray& items, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def);
}

std::unique_ptr<juce::AudioParameterInt> integer (const juce::String& id, const juce::String& name, int lo, int hi, int def)
{
    return std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, lo, hi, def);
}

Range skewed (float lo, float hi, float centre)
{
    Range r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (flt (outGain, "Output", Range (-24.0f, 12.0f, 0.1f), 0.0f, "dB"));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { outLimit, 1 }, "Output Limiter", true));
    layout.add (flt (outCeiling, "Output Ceiling", Range (-12.0f, 0.0f, 0.1f), -0.3f, "dB"));

    // Chain: Vokal Acid -> Polimetar Gate -> Fraktal Echo by default.
    const int defaults[] = { (int) ModuleType::vokalAcid, (int) ModuleType::polimetarGate,
                             (int) ModuleType::fraktalEcho, (int) ModuleType::empty };
    for (int i = 0; i < Chain::numSlots; ++i)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (
            "slot" + juce::String (i + 1), "Slot " + juce::String (i + 1), " | ");
        group->addChild (choice (slotModule (i), "Slot " + juce::String (i + 1) + " Module", moduleNames(), defaults[i]));
        group->addChild (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { slotBypass (i), 1 }, "Slot " + juce::String (i + 1) + " Bypass", false));
        group->addChild (percent (slotMix (i), "Slot " + juce::String (i + 1) + " Dry/Wet", 1.0f));
        group->addChild (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { slotSolo (i), 1 }, "Slot " + juce::String (i + 1) + " Solo", false));
        layout.add (std::move (group));
    }

    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("echo", "Fractal Echo", " | ");
        g->addChild (choice (echoTime, "Echo Time", divisionNames(), 5));
        g->addChild (flt (echoRatio, "Echo Branch Ratio", Range (0.25f, 0.9f, 0.001f), 0.618f));
        g->addChild (integer (echoGenerations, "Echo Generations", 1, 4, 3));
        g->addChild (choice (echoScale, "Echo Scale", scaleNames(), 0));
        g->addChild (integer (echoSpread, "Echo Pitch Spread", -7, 7, 2));
        g->addChild (percent (echoFeedback, "Echo Feedback", 0.35f));
        g->addChild (percent (echoDamp, "Echo Damp", 0.4f));
        g->addChild (percent (echoWidth, "Echo Width", 0.7f));
        g->addChild (percent (echoMix, "Echo Mix", 0.35f));
        layout.add (std::move (g));
    }

    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("vokal", "Vocal Acid", " | ");
        const char* notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int defaultVowel[] = { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1 };
        for (int pc = 0; pc < 12; ++pc)
            g->addChild (choice (vokalVowel (pc), juce::String ("Vocal Vowel ") + notes[pc], vowelNames(), defaultVowel[pc]));
        g->addChild (flt (vokalGlide, "Vocal Glide", skewed (5.0f, 500.0f, 60.0f), 60.0f, "ms"));
        g->addChild (flt (vokalShift, "Vocal Formant Shift", Range (-12.0f, 12.0f, 0.01f), 0.0f, "st"));
        g->addChild (percent (vokalResonance, "Vocal Resonance", 0.5f));
        g->addChild (percent (vokalAccent, "Vocal Accent", 0.5f));
        g->addChild (choice (vokalCharacter, "Vocal Character", { "Male", "Female", "Robot" }, 1));
        g->addChild (percent (vokalDrive, "Vocal Drive", 0.2f));
        g->addChild (percent (vokalMix, "Vocal Mix", 0.8f));
        layout.add (std::move (g));
    }

    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("gate", "Polymeter Gate", " | ");
        g->addChild (flt (gateLow, "Gate Low | Mid", skewed (40.0f, 1000.0f, 200.0f), 200.0f, "Hz"));
        g->addChild (flt (gateHigh, "Gate Mid | High", skewed (600.0f, 12000.0f, 2500.0f), 2500.0f, "Hz"));
        g->addChild (percent (gateMix, "Gate Mix", 1.0f));
        const char* bandNames[] = { "Low", "Mid", "High" };
        const int defSteps[] = { 16, 7, 13 }, defHits[] = { 4, 3, 5 };
        for (int b = 0; b < 3; ++b)
        {
            const juce::String n = juce::String ("Gate ") + bandNames[b] + " ";
            g->addChild (choice (gateBand (b, "mode"), n + "Mode", { "Steps", "Euclid", "Sidechain" }, 1));
            g->addChild (integer (gateBand (b, "steps"), n + "Steps", 1, 32, defSteps[b]));
            g->addChild (integer (gateBand (b, "hits"), n + "Hits", 0, 32, defHits[b]));
            g->addChild (integer (gateBand (b, "rotate"), n + "Rotate", 0, 31, 0));
            g->addChild (choice (gateBand (b, "rate"), n + "Rate", divisionNames(), 2));
            g->addChild (percent (gateBand (b, "depth"), n + "Depth", b == 0 ? 0.0f : 1.0f));
            g->addChild (percent (gateBand (b, "length"), n + "Length", 0.6f));
            g->addChild (flt (gateBand (b, "attack"), n + "Attack", skewed (0.1f, 50.0f, 5.0f), 1.0f, "ms"));
            g->addChild (flt (gateBand (b, "release"), n + "Release", skewed (0.1f, 200.0f, 20.0f), 20.0f, "ms"));
        }
        layout.add (std::move (g));
    }

    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("fraza", "Phrase", " | ");
        g->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { frazaOn, 1 }, "Phrase On", true));
        g->addChild (choice (frazaBars, "Phrase Length", { "8 bars", "16 bars", "32 bars" }, 1));
        g->addChild (integer (frazaOffset, "Phrase Offset", 0, 31, 0));
        g->addChild (choice (frazaCurve, "Phrase Curve", frazaCurveNames(), Fraza::rise));
        for (const auto& t : frazaTargets)
            g->addChild (flt (t.id, juce::String ("Phrase > ") + t.label, Range (-1.0f, 1.0f, 0.001f), 0.0f, "amount"));
        layout.add (std::move (g));
    }

    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("mod", "Modulation", " | ");
        const int defRate[] = { 4, 7 }; // 1/4 and 2 bars
        for (int i = 0; i < 2; ++i)
        {
            g->addChild (choice (lfoRate (i), "LFO " + juce::String (i + 1) + " Rate", lfoRateNames(), defRate[i]));
            g->addChild (choice (lfoShape (i), "LFO " + juce::String (i + 1) + " Shape", lfoShapeNames(), i == 0 ? 0 : 1));
        }
        for (int i = 0; i < 4; ++i)
            g->addChild (percent (macro (i), "Macro " + juce::String (i + 1), 0.0f));
        layout.add (std::move (g));
    }

    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("zap", "Zap Resonator", " | ");
        g->addChild (flt (zapThreshold, "Zap Threshold", Range (-60.0f, 0.0f, 0.1f), -30.0f, "dB"));
        g->addChild (choice (zapScale, "Zap Scale", scaleNames(), 0));
        g->addChild (choice (zapRoot, "Zap Root", noteNames(), 4));
        g->addChild (integer (zapOctave, "Zap Octave", 2, 7, 5));
        g->addChild (choice (zapOrder, "Zap Note Order", zapOrderNames(), 0));
        g->addChild (choice (zapKey, "Zap Listens To", { "Input", "Sidechain" }, 0));
        g->addChild (flt (zapSweep, "Zap Sweep", Range (-48.0f, 48.0f, 0.1f), 24.0f, "st"));
        g->addChild (flt (zapSweepTime, "Zap Sweep Time", skewed (5.0f, 1000.0f, 80.0f), 80.0f, "ms"));
        g->addChild (flt (zapDecay, "Zap Decay", skewed (20.0f, 2000.0f, 300.0f), 300.0f, "ms"));
        g->addChild (percent (zapResonance, "Zap Resonance", 0.8f));
        g->addChild (percent (zapWidth, "Zap Width", 0.6f));
        g->addChild (percent (zapMix, "Zap Mix", 0.5f));
        layout.add (std::move (g));
    }

    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("orbit", "Orbit Doppler", " | ");
        g->addChild (choice (orbitPath, "Orbit Path", orbitPathNames(), 0));
        g->addChild (choice (orbitRate, "Orbit Rate", lfoRateNames(), 6));
        g->addChild (flt (orbitRadius, "Orbit Radius", skewed (1.0f, 30.0f, 6.0f), 6.0f, "m"));
        g->addChild (percent (orbitDoppler, "Orbit Doppler", 0.5f));
        g->addChild (percent (orbitTone, "Orbit Distance Tone", 0.5f));
        g->addChild (percent (orbitWidth, "Orbit Width", 0.8f));
        g->addChild (percent (orbitMix, "Orbit Mix", 0.8f));
        layout.add (std::move (g));
    }

    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("tape", "Tape Stop", " | ");
        juce::StringArray lengthNames;
        for (const auto& l : TapeStop::lengths) lengthNames.add (l.name);
        g->addChild (choice (tapeMode, "Tape Mode", { "Stop", "Stop & Start", "Slow Down", "Start" }, 0));
        g->addChild (choice (tapeTrigger, "Tape Trigger", { "Brake button", "Every 2 bars", "Every 4 bars", "Every 8 bars", "Every 16 bars" }, 0));
        g->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { tapeBrake, 1 }, "Tape Brake", false));
        g->addChild (choice (tapeLength, "Tape Length", lengthNames, 3));
        // The slow down is drawn now; this only converts older projects and presets.
        g->addChild (percent (tapeCurve, "Tape Curve (old)", 0.5f));
        g->addChild (percent (tapeMix, "Tape Mix", 1.0f));
        g->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { tapeDrawn, 1 }, "Tape Drawn Curve", false));
        g->addChild (percent (tapeDrive, "Tape Stop Drive", 0.0f));
        g->addChild (percent (tapeCrush, "Tape Stop Crush", 0.0f));
        g->addChild (percent (tapeFilter, "Tape Stop Filter", 0.0f));
        g->addChild (percent (tapeWobble, "Tape Stop Wobble", 0.0f));
        layout.add (std::move (g));
    }

    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("ratchet", "Spectral Ratchet", " | ");
        g->addChild (choice (ratchetRate, "Ratchet Rate", divisionNames(), 2));
        g->addChild (integer (ratchetSteps, "Ratchet Steps", 1, 16, 8));
        g->addChild (choice (ratchetInterval, "Ratchet Interval",
                             { "Same", "Octaves up", "Octaves down", "Fifths up", "Scale up", "Scale down", "Random in scale" }, 4));
        g->addChild (choice (ratchetScale, "Ratchet Scale", scaleNames(), 0));
        g->addChild (choice (ratchetMask, "Ratchet Mask",
                             { "None", "Comb A", "Comb B", "Alternate combs", "Lows only", "Highs only", "Walking band" }, 0));
        g->addChild (percent (ratchetSmear, "Ratchet Smear", 0.15f));
        g->addChild (flt (ratchetTilt, "Ratchet Tilt", Range (-1.0f, 1.0f, 0.001f), 0.0f, "amount"));
        g->addChild (percent (ratchetMix, "Ratchet Mix", 1.0f));
        layout.add (std::move (g));
    }

    // Global Dry/Wet: added last so existing parameter order stays the same.
    layout.add (percent (outMix, "Dry/Wet", 1.0f));

    // Glide comes after everything that was there before it, for the same reason.
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("glide", "Glide", " | ");
        juce::StringArray lengthNames;
        for (const auto& l : Glide::diveLengths) lengthNames.add (l.name);
        g->addChild (choice (glideMode, "Glide Mode", glideModeNames(), Glide::bendIn));
        g->addChild (choice (glideRate, "Glide Step Rate", divisionNames(), 2));
        g->addChild (integer (glideSteps, "Glide Steps", 1, Glide::maxSteps, 16));
        g->addChild (flt (glideInterval, "Glide Interval", Range (-24.0f, 24.0f, 0.01f), -7.0f, "st"));
        g->addChild (flt (glideTime, "Glide Time", skewed (0.02f, 4.0f, 0.5f), 0.5f, "% step"));
        g->addChild (flt (glideCurve, "Glide Curve", Range (-1.0f, 1.0f, 0.001f), -0.4f, "amount"));
        g->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { glideDrawn, 1 }, "Glide Drawn Curve", false));
        g->addChild (flt (glideGrain, "Glide Grain", skewed (10.0f, 400.0f, 80.0f), 80.0f, "ms"));
        g->addChild (percent (glideMix, "Glide Mix", 1.0f));
        g->addChild (choice (glideDive, "Glide Dive", glideDiveNames(), Glide::diveOff));
        g->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { glideDiveHold, 1 }, "Glide Dive Hold", false));
        g->addChild (choice (glideDiveLength, "Glide Dive Length", lengthNames, 3));
        g->addChild (flt (glideDiveDepth, "Glide Dive Depth", Range (-48.0f, 24.0f, 0.01f), -24.0f, "st"));
        g->addChild (flt (glideDiveCurve, "Glide Dive Curve", Range (-1.0f, 1.0f, 0.001f), 0.3f, "amount"));
        layout.add (std::move (g));
    }

    return layout;
}

} // namespace originpsy::params
