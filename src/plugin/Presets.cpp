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

        // Full-on: bright, busy, Hijaz and Phrygian, everything on the 1/16 grid.
        { "Full-on Rolling Talk", "Full-on", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::vokalAcid, ModuleType::polimetarGate, ModuleType::fraktalEcho);
              set (p, params::vokalCharacter, 1.0f);  // female
              set (p, params::vokalGlide, 30.0f);
              set (p, params::vokalResonance, 0.7f);
              set (p, params::vokalAccent, 0.9f);
              set (p, params::gateBand (0, "depth"), 0.0f);
              set (p, params::gateBand (1, "steps"), 16.0f);
              set (p, params::gateBand (1, "hits"), 11.0f);
              set (p, params::gateBand (1, "depth"), 0.7f);
              set (p, params::gateBand (2, "steps"), 12.0f);
              set (p, params::gateBand (2, "hits"), 7.0f);
              set (p, params::gateBand (2, "depth"), 0.8f);
              set (p, params::echoTime, 3.0f);        // 1/16D
              set (p, params::echoScale, 1.0f);       // Hijaz
              set (p, params::echoMix, 0.15f);
              set (p, params::lfoRate (1), 5.0f);     // 1/2
              macros (p);
              connect (p, ModSource::lfo2, params::vokalShift, 0.15f);
          } },

        { "Full-on Lead Spiral", "Full-on", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::fraktalEcho, ModuleType::orbitDoppler, ModuleType::empty);
              set (p, params::echoTime, 6.0f);        // 1/8D
              set (p, params::echoScale, 1.0f);       // Hijaz
              set (p, params::echoGenerations, 4.0f);
              set (p, params::echoSpread, 3.0f);
              set (p, params::echoFeedback, 0.55f);
              set (p, params::echoDamp, 0.25f);
              set (p, params::echoWidth, 0.95f);
              set (p, params::echoMix, 0.4f);
              set (p, params::orbitPath, 2.0f);       // spiral
              set (p, params::orbitRate, 7.0f);       // 2 bars
              set (p, params::orbitMix, 0.5f);
              set (p, params::lfoRate (0), 6.0f);     // 1 bar
              macros (p);
              connect (p, ModSource::lfo1, params::echoDamp, 0.25f);
          } },

        { "Full-on Zap Groove", "Full-on", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::zapResonator, ModuleType::polimetarGate, ModuleType::fraktalEcho);
              set (p, params::zapScale, 1.0f);        // Hijaz
              set (p, params::zapThreshold, -34.0f);
              set (p, params::zapOrder, 0.0f);        // up
              set (p, params::zapSweep, 36.0f);
              set (p, params::zapSweepTime, 50.0f);
              set (p, params::zapDecay, 180.0f);
              set (p, params::zapMix, 0.4f);
              set (p, params::gateBand (0, "depth"), 0.0f);
              set (p, params::gateBand (1, "depth"), 0.4f);
              set (p, params::gateBand (2, "steps"), 16.0f);
              set (p, params::gateBand (2, "hits"), 9.0f);
              set (p, params::gateBand (2, "depth"), 0.9f);
              set (p, params::echoTime, 2.0f);        // 1/16
              set (p, params::echoMix, 0.2f);
              macros (p);
              connect (p, ModSource::lfo2, params::zapSweep, 0.12f);
          } },

        { "Full-on Riser Machine", "Full-on", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::vokalAcid, ModuleType::polimetarGate, ModuleType::fraktalEcho, ModuleType::tapeStop);
              p.applyDropPreset();
              set (p, params::frazaCurve, 4.0f);      // last 4 bars
              set (p, params::echoScale, 1.0f);       // Hijaz
              set (p, params::echoSpread, 4.0f);
              set (p, params::vokalAccent, 0.8f);
              set (p, params::tapeMode, 0.0f);        // stop
              set (p, params::tapeTrigger, 4.0f);     // every 16 bars
              set (p, params::tapeLength, 2.0f);      // 1/4
              set (p, params::tapeCurve, 0.7f);
              macros (p);
              connect (p, ModSource::fraza, params::gateBand (2, "length"), -0.35f);
          } },

        // Dark: night and forest. Low, dark, harmonic and Hungarian minor, robots and glitches.
        { "Dark Forest Echoes", "Dark", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::fraktalEcho, ModuleType::orbitDoppler, ModuleType::empty);
              set (p, params::echoTime, 1.0f);        // 1/16T
              set (p, params::echoScale, 5.0f);       // Hungarian Minor
              set (p, params::echoGenerations, 4.0f);
              set (p, params::echoSpread, -3.0f);
              set (p, params::echoFeedback, 0.65f);
              set (p, params::echoDamp, 0.7f);
              set (p, params::echoMix, 0.35f);
              set (p, params::orbitPath, 0.0f);       // circle
              set (p, params::orbitRate, 8.0f);       // 4 bars
              set (p, params::orbitRadius, 15.0f);
              set (p, params::orbitTone, 0.8f);
              set (p, params::orbitMix, 0.6f);
              macros (p);
              connect (p, ModSource::lfo2, params::echoSpread, -0.2f);
          } },

        { "Dark Robot Mantra", "Dark", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::vokalAcid, ModuleType::polimetarGate, ModuleType::empty);
              set (p, params::vokalCharacter, 2.0f);  // robot
              set (p, params::vokalDrive, 0.6f);
              set (p, params::vokalResonance, 0.75f);
              set (p, params::vokalShift, -5.0f);
              set (p, params::vokalGlide, 15.0f);
              set (p, params::gateBand (0, "depth"), 0.2f);
              for (int b = 1; b < 3; ++b)
              {
                  set (p, params::gateBand (b, "rate"), 0.0f);   // 1/32
                  set (p, params::gateBand (b, "depth"), 0.9f);
                  set (p, params::gateBand (b, "release"), 6.0f);
              }
              set (p, params::gateBand (1, "steps"), 13.0f);
              set (p, params::gateBand (1, "hits"), 8.0f);
              set (p, params::gateBand (2, "steps"), 11.0f);
              set (p, params::gateBand (2, "hits"), 5.0f);
              macros (p);
              connect (p, ModSource::envelope, params::vokalShift, 0.25f);
          } },

        { "Dark Glitch Swarm", "Dark", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::polimetarGate, ModuleType::zapResonator, ModuleType::empty);
              for (int b = 0; b < 3; ++b)
              {
                  set (p, params::gateBand (b, "rate"), 0.0f);   // 1/32
                  set (p, params::gateBand (b, "depth"), 1.0f);
                  set (p, params::gateBand (b, "release"), 2.0f);
              }
              set (p, params::gateBand (0, "steps"), 13.0f);
              set (p, params::gateBand (1, "steps"), 11.0f);
              set (p, params::gateBand (2, "steps"), 9.0f);
              set (p, params::zapScale, 2.0f);        // Harmonic Minor
              set (p, params::zapOctave, 3.0f);
              set (p, params::zapSweep, -36.0f);
              set (p, params::zapDecay, 120.0f);
              set (p, params::zapResonance, 0.4f);
              set (p, params::zapOrder, 2.0f);        // random
              set (p, params::zapMix, 0.35f);
              set (p, params::lfoShape (0), 5.0f);    // random
              set (p, params::lfoRate (0), 5.0f);     // 1/2
              macros (p);
              connect (p, ModSource::lfo1, params::gateBand (1, "rotate"), 0.5f);
              connect (p, ModSource::lfo1, params::gateBand (2, "rotate"), 0.3f);
          } },

        { "Dark Tape Collapse", "Dark", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::fraktalEcho, ModuleType::tapeStop, ModuleType::empty);
              set (p, params::echoTime, 6.0f);        // 1/8D
              set (p, params::echoScale, 2.0f);       // Harmonic Minor
              set (p, params::echoSpread, -2.0f);
              set (p, params::echoDamp, 0.65f);
              set (p, params::echoFeedback, 0.5f);
              set (p, params::echoMix, 0.3f);
              set (p, params::tapeMode, 2.0f);        // slow down
              set (p, params::tapeTrigger, 3.0f);     // every 8 bars
              set (p, params::tapeLength, 3.0f);      // 1/2
              set (p, params::tapeCurve, 0.3f);
              macros (p);
          } },

        // Prog: deep and smooth. Dorian and natural minor, long LFOs, long phrases.
        { "Prog Breathing Pad", "Prog", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::polimetarGate, ModuleType::fraktalEcho, ModuleType::empty);
              set (p, params::gateBand (0, "depth"), 0.2f);
              set (p, params::gateBand (1, "rate"), 5.0f);   // 1/8
              set (p, params::gateBand (1, "steps"), 16.0f);
              set (p, params::gateBand (1, "hits"), 5.0f);
              set (p, params::gateBand (1, "length"), 0.9f);
              set (p, params::gateBand (1, "release"), 120.0f);
              set (p, params::gateBand (1, "depth"), 0.6f);
              set (p, params::gateBand (2, "steps"), 12.0f);
              set (p, params::gateBand (2, "hits"), 7.0f);
              set (p, params::gateBand (2, "length"), 0.5f);
              set (p, params::gateBand (2, "depth"), 0.5f);
              set (p, params::echoTime, 9.0f);        // 1/4D
              set (p, params::echoScale, 6.0f);       // Dorian
              set (p, params::echoGenerations, 2.0f);
              set (p, params::echoDamp, 0.5f);
              set (p, params::echoMix, 0.25f);
              set (p, params::lfoRate (0), 8.0f);     // 4 bars
              macros (p);
              connect (p, ModSource::lfo1, params::gateBand (1, "length"), 0.2f);
          } },

        { "Prog Slow Orbit", "Prog", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::orbitDoppler, ModuleType::fraktalEcho, ModuleType::empty);
              set (p, params::orbitPath, 0.0f);       // circle
              set (p, params::orbitRate, 9.0f);       // 8 bars
              set (p, params::orbitRadius, 8.0f);
              set (p, params::orbitDoppler, 0.25f);
              set (p, params::orbitMix, 0.6f);
              set (p, params::echoTime, 6.0f);        // 1/8D
              set (p, params::echoScale, 3.0f);       // Natural Minor
              set (p, params::echoWidth, 1.0f);
              set (p, params::echoMix, 0.2f);
              macros (p);
          } },

        { "Prog Soft Talk", "Prog", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::vokalAcid, ModuleType::fraktalEcho, ModuleType::empty);
              set (p, params::vokalCharacter, 1.0f);  // female
              set (p, params::vokalGlide, 120.0f);
              set (p, params::vokalResonance, 0.4f);
              set (p, params::vokalAccent, 0.4f);
              set (p, params::vokalMix, 0.6f);
              set (p, params::echoTime, 8.0f);        // 1/4
              set (p, params::echoScale, 6.0f);       // Dorian
              set (p, params::echoMix, 0.2f);
              set (p, params::lfoRate (0), 9.0f);     // 8 bars
              macros (p);
              connect (p, ModSource::lfo1, params::vokalShift, 0.12f);
          } },

        { "Prog 32 Bar Journey", "Prog", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::polimetarGate, ModuleType::fraktalEcho, ModuleType::orbitDoppler);
              set (p, params::frazaBars, 2.0f);       // 32 bars
              set (p, params::frazaCurve, 1.0f);      // rise (late)
              set (p, params::frazaTargets[0].id, 0.35f);
              set (p, params::frazaTargets[1].id, 0.3f);
              set (p, params::frazaTargets[2].id, -0.4f);
              set (p, params::frazaTargets[5].id, 0.5f);
              set (p, params::echoScale, 3.0f);       // Natural Minor
              set (p, params::echoTime, 6.0f);        // 1/8D
              set (p, params::orbitRate, 9.0f);       // 8 bars
              set (p, params::orbitMix, 0.4f);
              macros (p);
          } },

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

        { "Spectral Stutter", "Glitch", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::spectralRatchet, ModuleType::fraktalEcho, ModuleType::empty);
              set (p, params::ratchetRate, 2.0f);     // 1/16
              set (p, params::ratchetSteps, 8.0f);
              set (p, params::ratchetInterval, 4.0f); // scale up
              set (p, params::ratchetMask, 3.0f);     // alternate combs
              set (p, params::ratchetSmear, 0.2f);
              set (p, params::echoMix, 0.2f);
              p.setRatchetPattern (0xEEEEu);
              macros (p);
          } },

        { "Frozen Choir", "Pads", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::spectralRatchet, ModuleType::orbitDoppler, ModuleType::empty);
              set (p, params::ratchetRate, 8.0f);     // 1/4
              set (p, params::ratchetSteps, 16.0f);
              set (p, params::ratchetInterval, 3.0f); // fifths up
              set (p, params::ratchetSmear, 0.6f);
              set (p, params::ratchetTilt, -0.3f);
              set (p, params::ratchetMix, 0.7f);
              set (p, params::orbitRate, 9.0f);       // 8 bars
              set (p, params::orbitMix, 0.6f);
              p.setRatchetPattern (0xFFFEu);
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

        // Glide: bass portamento on audio.
        { "Rolling Glide Bass", "Bass", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::glide, ModuleType::empty, ModuleType::empty);
              set (p, params::glideMode, 1.0f);       // bend in
              set (p, params::glideRate, 2.0f);       // 1/16
              set (p, params::glideInterval, -5.0f);
              set (p, params::glideTime, 0.35f);
              set (p, params::glideCurve, -0.5f);
              set (p, params::glideGrain, 60.0f);
              p.setGlidePattern (0x8888u);            // the last 1/16 of every beat
              macros (p);
          } },

        { "Acid Slide Line", "Bass", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::glide, ModuleType::empty, ModuleType::empty);
              set (p, params::glideMode, 0.0f);       // slide
              set (p, params::glideRate, 2.0f);       // 1/16
              set (p, params::glideTime, 0.6f);
              set (p, params::glideCurve, -0.3f);
              const int notes[] = { 0, 0, 0, 12, 0, 0, 7, 0, 0, 0, 0, 12, 0, 3, 0, -5 };
              for (int i = 0; i < 16; ++i) p.setGlideNote (i, notes[i]);
              p.setGlidePattern (0xE858u);            // slide into steps 4, 5, 7, 12, 14, 15 and 16
              macros (p);
          } },

        { "Dive Into Drop", "Build-up", [] (OriginPsyProcessor& p) {
              slots (p, ModuleType::glide, ModuleType::fraktalEcho, ModuleType::empty);
              p.setGlidePattern (0);
              set (p, params::glideDive, 5.0f);       // every 8 bars
              set (p, params::glideDiveLength, 5.0f); // 2 bars
              set (p, params::glideDiveDepth, -36.0f);
              set (p, params::glideDiveCurve, 0.5f);
              set (p, params::echoMix, 0.15f);
              macros (p);
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
        { ModuleType::spectralRatchet, "ratchet_" }, { ModuleType::glide, "glide_" },
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
        if (! mine || id == params::tapeBrake || id == params::tapeTrigger || id == params::tapeDrawn
            || id == params::glideDiveHold || id == params::glideDrawn || id == params::glideDive) continue;

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
        else if (id.startsWith ("gate_") && id.endsWith ("_mode")) hi = ranged->convertTo0to1 (1.0f); // Steps or Euclid, never Sidechain
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
    const char* order[] = { "Init", "Full-on", "Dark", "Prog", "Build-up", "Lead FX", "Bass", "Pads", "Glitch" };
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
    p.setRatchetPattern (0xEEEEu);
    p.resetFrazaCustom();
    p.resetTapeDrawn();
    p.setGlidePattern (0x8888u);
    for (int i = 0; i < Glide::maxSteps; ++i) p.setGlideNote (i, 0);
    p.resetGlideDrawn();
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
