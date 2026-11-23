#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace originpsy;

OriginPsyProcessor::OriginPsyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OriginPsy", params::createLayout())
{
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (params::isModulatable (*ranged))
            {
                modIndex[ranged->paramID] = (int) modParams.size();
                modParams.push_back (ranged);
            }

    modNorm = std::make_unique<std::atomic<float>[]> (modParams.size());
    frazaTargetOf.assign (modParams.size(), -1);
    const std::pair<juce::String, int> frazaMap[] = {
        { params::echoMix, 0 }, { params::echoFeedback, 1 }, { params::echoDamp, 2 }, { params::vokalShift, 3 },
        { params::vokalResonance, 4 }, { params::gateBand (0, "depth"), 5 }, { params::gateBand (1, "depth"), 5 },
        { params::gateBand (2, "depth"), 5 } };
    for (const auto& [id, target] : frazaMap) frazaTargetOf[(size_t) modIndex.at (id)] = target;
    for (size_t i = 0; i < modParams.size(); ++i) modNorm[i].store (modParams[i]->getValue());
    originpsy::PresetManager::wireMacros (*this);
}

bool OriginPsyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void OriginPsyProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    chain.prepare (sr, samplesPerBlock);
    monoScratch.assign ((size_t) std::max (1, samplesPerBlock), 0.0f);
    envelope.prepare (sr);
    for (size_t i = 0; i < modParams.size(); ++i) modNorm[i].store (modParams[i]->getValue());
    updateModules();
    chain.reset();
    clock.reset();
    outGain.reset (sr, 0.02);
    limiter.prepare (sr);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (value (params::outGain)));
    wantedLatency = Chain::latencyOf (chain.activeLayout());
    setLatencySamples (wantedLatency.load());
}

Transport OriginPsyProcessor::readTransport (int numSamples)
{
    double bpm = 140.0, beatsPerBar = 4.0;
    if (auto* head = getPlayHead())
    {
        if (auto pos = head->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto sig = pos->getTimeSignature())
                beatsPerBar = sig->numerator * 4.0 / std::max (1, sig->denominator);

            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                {
                    Transport t;
                    t.bpm = bpm;
                    t.beatsPerBar = beatsPerBar;
                    t.ppqAtBlockStart = *ppq;
                    t.playing = true;
                    return t;
                }
        }
    }
    return clock.next (bpm, beatsPerBar, numSamples, sampleRate);
}

float OriginPsyProcessor::m (const juce::String& id) const
{
    const auto it = modIndex.find (id);
    if (it == modIndex.end()) return value (id);
    const int i = it->second;
    const auto* p = modParams[(size_t) i];
    return p->getNormalisableRange().snapToLegalValue (p->convertFrom0to1 (modNorm[(size_t) i].load (std::memory_order_relaxed)));
}

void OriginPsyProcessor::computeModulation (const Transport& t, float fraza, const float* left, const float* right,
                                            int numSamples)
{
    envelope.process (left, right, numSamples);

    std::array<float, numModSources> src {};
    for (int i = 0; i < 2; ++i)
        src[(size_t) ModSource::lfo1 + (size_t) i] = Lfo::value ((int) value (params::lfoShape (i)),
                                                                  (int) value (params::lfoRate (i)), t.ppqAtBlockStart);
    src[(size_t) ModSource::envelope] = envelope.value();
    for (int i = 0; i < 4; ++i) src[(size_t) ModSource::macro1 + (size_t) i] = value (params::macro (i));
    src[(size_t) ModSource::fraza] = fraza;
    for (int i = 0; i < numModSources; ++i) meters.source[(size_t) i].store (src[(size_t) i], std::memory_order_relaxed);

    for (size_t i = 0; i < modParams.size(); ++i)
    {
        float norm = modParams[i]->getValue() + matrix.offsetFor ((int) i, src);
        if (frazaTargetOf[i] >= 0)
            norm += value (params::frazaTargets[frazaTargetOf[i]].id) * fraza;
        modNorm[i].store (juce::jlimit (0.0f, 1.0f, norm), std::memory_order_relaxed);
    }
}

void OriginPsyProcessor::updateModules()
{
    // Chain layout.
    Chain::Layout layout;
    for (int i = 0; i < Chain::numSlots; ++i)
    {
        layout.slots[(size_t) i] = (ModuleType) (int) m (params::slotModule (i));
        layout.bypass[(size_t) i] = m (params::slotBypass (i)) > 0.5f;
        layout.solo[(size_t) i] = value (params::slotSolo (i)) > 0.5f;
        chain.setSlotMix (i, m (params::slotMix (i)));
    }
    chain.setLayout (layout);

    // Fraktal Echo.
    FraktalEcho::Params e;
    e.division = (int) m (params::echoTime);
    e.ratio = m (params::echoRatio);
    e.generations = (int) m (params::echoGenerations);
    e.scale = (int) m (params::echoScale);
    e.spread = (int) m (params::echoSpread);
    e.feedback = m (params::echoFeedback) * 0.95f;
    e.damp = m (params::echoDamp);
    e.width = m (params::echoWidth);
    e.mix = m (params::echoMix);
    chain.echo.setParams (e);

    // Vokal Acid.
    VokalAcid::Params v;
    for (int pc = 0; pc < 12; ++pc) v.vowelMap[(size_t) pc] = m (params::vokalVowel (pc));
    v.glideMs = m (params::vokalGlide);
    v.shift = m (params::vokalShift);
    v.resonance = m (params::vokalResonance);
    v.accent = m (params::vokalAccent);
    v.character = (int) m (params::vokalCharacter);
    v.drive = m (params::vokalDrive);
    v.mix = m (params::vokalMix);
    chain.vokal.setParams (v);

    // Polimetar Gate.
    PolimetarGate::Params g;
    g.lowHz = m (params::gateLow);
    g.highHz = m (params::gateHigh);
    g.mix = m (params::gateMix);
    for (int b = 0; b < 3; ++b)
    {
        auto& band = g.bands[(size_t) b];
        band.euclidMode = m (params::gateBand (b, "mode")) > 0.5f;
        band.steps = (int) m (params::gateBand (b, "steps"));
        band.hits = (int) m (params::gateBand (b, "hits"));
        band.rotate = (int) m (params::gateBand (b, "rotate"));
        band.division = (int) m (params::gateBand (b, "rate"));
        band.depth = m (params::gateBand (b, "depth"));
        band.length = m (params::gateBand (b, "length"));
        band.attackMs = m (params::gateBand (b, "attack"));
        band.releaseMs = m (params::gateBand (b, "release"));
        band.pattern = gatePatterns[(size_t) b].load();
    }
    chain.gate.setParams (g);

    // Zap Resonator.
    ZapResonator::Params z;
    z.thresholdDb = m (params::zapThreshold);
    z.scale = (int) m (params::zapScale);
    z.root = (int) m (params::zapRoot);
    z.octave = (int) m (params::zapOctave);
    z.order = (int) m (params::zapOrder);
    z.sweep = m (params::zapSweep);
    z.sweepMs = m (params::zapSweepTime);
    z.decayMs = m (params::zapDecay);
    z.resonance = m (params::zapResonance);
    z.width = m (params::zapWidth);
    z.mix = m (params::zapMix);
    chain.zap.setParams (z);

    // Orbit Doppler.
    OrbitDoppler::Params o;
    o.path = (int) m (params::orbitPath);
    o.rate = (int) m (params::orbitRate);
    o.radius = m (params::orbitRadius);
    o.doppler = m (params::orbitDoppler);
    o.distanceTone = m (params::orbitTone);
    o.width = m (params::orbitWidth);
    o.mix = m (params::orbitMix);
    chain.orbit.setParams (o);

    // Tape Stop. The Brake button is not modulated: it is read as it is.
    TapeStop::Params tp;
    tp.mode = (int) m (params::tapeMode);
    tp.trigger = (int) m (params::tapeTrigger);
    tp.brake = value (params::tapeBrake) > 0.5f || midiBrake;
    tp.length = (int) m (params::tapeLength);
    tp.curve = m (params::tapeCurve);
    tp.mix = m (params::tapeMix);
    chain.tape.setParams (tp);

    // Spectral Ratchet.
    SpectralRatchet::Params rp;
    rp.division = (int) m (params::ratchetRate);
    rp.steps = (int) m (params::ratchetSteps);
    rp.pattern = ratchetPattern.load();
    rp.interval = (int) m (params::ratchetInterval);
    rp.scale = (int) m (params::ratchetScale);
    rp.mask = (int) m (params::ratchetMask);
    rp.smear = m (params::ratchetSmear);
    rp.tilt = m (params::ratchetTilt);
    rp.mix = m (params::ratchetMix);
    for (int i = 0; i < Chain::numSlots; ++i)
        if ((ModuleType) (int) m (params::slotModule (i)) == ModuleType::spectralRatchet)
            rp.mix *= m (params::slotMix (i));
    chain.ratchet.setParams (rp);

    // Latency follows the layout (Spectral Ratchet adds 2048 samples).
    const int latency = Chain::latencyOf (layout);
    if (latency != wantedLatency.load())
    {
        wantedLatency = latency;
        triggerAsyncUpdate();
    }
}

void OriginPsyProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    bool asked = false;
    uint32_t used = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            switch (msg.getNoteNumber())
            {
                case midiNoteBrake: midiBrake = true; break;
                case midiNoteDrop: midiActions.drop = true; asked = true; break;
                case midiNotePrev: --midiActions.step; asked = true; break;
                case midiNoteNext: ++midiActions.step; asked = true; break;
                default: continue;
            }
            ++used;
        }
        else if (msg.isNoteOff() && msg.getNoteNumber() == midiNoteBrake)
        {
            midiBrake = false;
            ++used;
        }
        else if (msg.isProgramChange())
        {
            midiActions.program = msg.getProgramChangeNumber();
            midiActions.step = 0;
            asked = true;
            ++used;
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            midiBrake = false;
        }
    }
    if (used > 0) meters.midiEvents += used;
    meters.midiBrake = midiBrake;
    if (asked) midiActions.triggerAsyncUpdate();
}

void OriginPsyProcessor::MidiActions::handleAsyncUpdate()
{
    if (drop.exchange (false)) proc.applyDropPreset();

    const int prog = program.exchange (-1);
    const int delta = step.exchange (0);
    if (prog < 0 && delta == 0) return;

    PresetManager presets (proc);
    const auto entries = presets.list();
    if (entries.empty()) return;
    const int count = (int) entries.size();
    int index = -1;
    if (prog >= 0)
    {
        if (prog >= count) return;
        index = prog;
    }
    else
    {
        int current = -1;
        for (int i = 0; i < count && current < 0; ++i)
            if (entries[(size_t) i].name == proc.currentPresetName) current = i;
        index = current < 0 ? (delta > 0 ? 0 : count - 1) : current + delta;
        index = ((index % count) + count) % count;
    }
    presets.load (entries[(size_t) index]);
}

void OriginPsyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
    handleMidi (midi);
    midi.clear();
    if (numSamples == 0) return;

    const auto t = readTransport (numSamples);

    Fraza::Params fp;
    const int barsChoice = (int) value (params::frazaBars);
    fp.bars = barsChoice == 0 ? 8 : (barsChoice == 1 ? 16 : 32);
    fp.offset = (int) value (params::frazaOffset);
    fp.curve = (int) value (params::frazaCurve);
    const double phrasePos = Fraza::phrasePosition (fp, t.ppqAtBlockStart, t.beatsPerBar);
    const float fraza = value (params::frazaOn) > 0.5f ? Fraza::evaluate (fp, phrasePos) : 0.0f;

    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    computeModulation (t, fraza, left, right, numSamples);
    updateModules();
    if (right == nullptr)
    {
        // Mono: run the stereo chain on a copy and fold it back.
        if ((int) monoScratch.size() < numSamples) monoScratch.resize ((size_t) numSamples); // only if the host lied about the block size
        float* tmp = monoScratch.data();
        std::copy (left, left + numSamples, tmp);
        chain.process (left, tmp, numSamples, t);
        for (int n = 0; n < numSamples; ++n) left[n] = 0.5f * (left[n] + tmp[n]);
    }
    else
    {
        chain.process (left, right, numSamples, t);
    }

    outGain.setTargetValue (juce::Decibels::decibelsToGain (value (params::outGain)));
    for (int n = 0; n < numSamples; ++n)
    {
        const float g = outGain.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[n] *= g;
    }

    if (value (params::outLimit) > 0.5f)
    {
        limiter.process (buffer.getWritePointer (0), buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr,
                         numSamples, value (params::outCeiling));
        meters.limiterDb = limiter.getReductionDb();
    }
    else
    {
        limiter.reset();
        meters.limiterDb = 0.0f;
    }

    meters.vokalNote = chain.vokal.getDetectedNote();
    meters.vokalVowel = chain.vokal.getVowelPosition();
    meters.vokalAccent = chain.vokal.getAccent();
    for (int b = 0; b < 3; ++b) meters.gateStep[(size_t) b] = chain.gate.getStep (b);
    meters.tapeSpeed = chain.tape.getSpeed();
    meters.ratchetStep = chain.ratchet.getStep();
    meters.ratchetFreezing = chain.ratchet.isFreezing();
    for (size_t b = 0; b < meters.ratchetBands.size(); ++b)
        meters.ratchetBands[b].store (chain.ratchet.getBands()[b], std::memory_order_relaxed);
    meters.zapNote = chain.zap.getLastNote();
    meters.zapHits = chain.zap.getHitCount();
    meters.zapLevel = chain.zap.getLevel();
    const auto orbitPos = chain.orbit.getPosition();
    meters.orbitX = orbitPos.x;
    meters.orbitY = orbitPos.y;
    meters.frazaPosition = phrasePos;
    meters.frazaValue = fraza;
    meters.bpm = t.bpm;
    meters.hostPlaying = t.playing;
}

void OriginPsyProcessor::setGatePattern (int band, uint32_t pattern)
{
    if (band >= 0 && band < 3) gatePatterns[(size_t) band] = pattern;
}

void OriginPsyProcessor::toggleGateStep (int band, int step)
{
    if (band < 0 || band >= 3 || step < 0 || step >= 32) return;
    gatePatterns[(size_t) band] ^= (1u << step);
}

void OriginPsyProcessor::swapSlots (int a, int b)
{
    if (a == b || a < 0 || b < 0 || a >= Chain::numSlots || b >= Chain::numSlots) return;
    // Everything that belongs to a slot moves with its module.
    const std::pair<juce::String, juce::String> pairs[] = {
        { params::slotModule (a), params::slotModule (b) }, { params::slotBypass (a), params::slotBypass (b) },
        { params::slotMix (a), params::slotMix (b) },       { params::slotSolo (a), params::slotSolo (b) },
    };
    for (const auto& [idA, idB] : pairs)
    {
        auto* pa = apvts.getParameter (idA);
        auto* pb = apvts.getParameter (idB);
        const float va = pa->getValue(), vb = pb->getValue();
        pa->beginChangeGesture(); pb->beginChangeGesture();
        pa->setValueNotifyingHost (vb);
        pb->setValueNotifyingHost (va);
        pa->endChangeGesture(); pb->endChangeGesture();
    }
}

void OriginPsyProcessor::applyDropPreset()
{
    for (const auto& target : params::frazaTargets)
        if (auto* p = apvts.getParameter (target.id))
            p->setValueNotifyingHost (p->convertTo0to1 (target.dropAmount));

    if (auto* on = apvts.getParameter (params::frazaOn)) on->setValueNotifyingHost (1.0f);
    if (auto* c = apvts.getParameter (params::frazaCurve))
        c->setValueNotifyingHost (c->convertTo0to1 ((float) Fraza::last4Bars));
}

int OriginPsyProcessor::getEchoTaps (std::array<FraktalEcho::Tap, FraktalEcho::maxTaps>& out) const
{
    FraktalEcho::Params e;
    e.ratio = value (params::echoRatio);
    e.generations = (int) value (params::echoGenerations);
    e.spread = (int) value (params::echoSpread);
    return FraktalEcho::buildTree (e, out);
}

juce::ValueTree OriginPsyProcessor::createStateTree()
{
    auto state = apvts.copyState();
    for (int b = 0; b < 3; ++b)
        state.setProperty ("gate_pattern_" + juce::String (b + 1), (juce::int64) gatePatterns[(size_t) b].load(), nullptr);
    state.setProperty ("ratchet_pattern", (juce::int64) ratchetPattern.load(), nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    state.setProperty ("preset_name", currentPresetName, nullptr);

    // Modulation: destinations are saved by parameter ID, not by index, so a
    // later version that adds parameters still loads old projects.
    juce::ValueTree mods ("MODS");
    for (int i = 0; i < ModMatrix::numSlots; ++i)
    {
        const auto slot = matrix.get (i);
        if (! slot.used() || slot.destination >= (int) modParams.size()) continue;
        juce::ValueTree m ("MOD");
        m.setProperty ("source", (int) slot.source, nullptr);
        m.setProperty ("dest", modParams[(size_t) slot.destination]->paramID, nullptr);
        m.setProperty ("depth", slot.depth, nullptr);
        m.setProperty ("bipolar", slot.bipolar, nullptr);
        mods.appendChild (m, nullptr);
    }
    state.removeChild (state.getChildWithName ("MODS"), nullptr);
    state.appendChild (mods, nullptr);
    return state;
}

void OriginPsyProcessor::loadStateTree (const juce::ValueTree& source)
{
    if (! source.hasType (apvts.state.getType())) return;
    auto state = source.createCopy();

    for (int b = 0; b < 3; ++b)
    {
        const auto key = "gate_pattern_" + juce::String (b + 1);
        if (state.hasProperty (key))
            gatePatterns[(size_t) b] = (uint32_t) (juce::int64) state.getProperty (key);
    }
    if (state.hasProperty ("ratchet_pattern"))
        ratchetPattern = (uint32_t) (juce::int64) state.getProperty ("ratchet_pattern") & 0xffffu;
    currentPresetName = state.getProperty ("preset_name", "Init").toString();

    matrix.clearAll();
    int slot = 0;
    for (const auto& m : state.getChildWithName ("MODS"))
    {
        const int dest = modulatableIndex (m.getProperty ("dest").toString());
        const int src = (int) m.getProperty ("source", 0);
        if (dest < 0 || src <= 0 || src >= numModSources || slot >= ModMatrix::numSlots) continue;
        matrix.set (slot++, { (ModSource) src, dest, (float) m.getProperty ("depth", 0.0f),
                              (bool) m.getProperty ("bipolar", isBipolar ((ModSource) src)) });
    }
    state.removeChild (state.getChildWithName ("MODS"), nullptr);
    apvts.replaceState (state);
}

void OriginPsyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void OriginPsyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        loadStateTree (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* OriginPsyProcessor::createEditor()
{
    return new OriginPsyEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OriginPsyProcessor();
}
