#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace originpsy;

OriginPsyProcessor::OriginPsyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OriginPsy", params::createLayout())
{
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
    updateModules (0.0f);
    chain.reset();
    clock.reset();
    outGain.reset (sr, 0.02);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (value (params::outGain)));
    setLatencySamples (0);
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

float OriginPsyProcessor::modulated (const juce::String& id, int targetIndex, float fraza) const
{
    auto* p = apvts.getParameter (id);
    const float amount = value (params::frazaTargets[targetIndex].id);
    const float norm = juce::jlimit (0.0f, 1.0f, p->getValue() + amount * fraza);
    return p->convertFrom0to1 (norm);
}

void OriginPsyProcessor::updateModules (float fraza)
{
    // Chain layout.
    Chain::Layout layout;
    for (int i = 0; i < Chain::numSlots; ++i)
    {
        layout.slots[(size_t) i] = (ModuleType) (int) value (params::slotModule (i));
        layout.bypass[(size_t) i] = value (params::slotBypass (i)) > 0.5f;
    }
    chain.setLayout (layout);

    // Fraktal Echo.
    FraktalEcho::Params e;
    e.division = (int) value (params::echoTime);
    e.ratio = value (params::echoRatio);
    e.generations = (int) value (params::echoGenerations);
    e.scale = (int) value (params::echoScale);
    e.spread = (int) value (params::echoSpread);
    e.feedback = modulated (params::echoFeedback, 1, fraza) * 0.95f;
    e.damp = modulated (params::echoDamp, 2, fraza);
    e.width = value (params::echoWidth);
    e.mix = modulated (params::echoMix, 0, fraza);
    chain.echo.setParams (e);

    // Vokal Acid.
    VokalAcid::Params v;
    for (int pc = 0; pc < 12; ++pc) v.vowelMap[(size_t) pc] = value (params::vokalVowel (pc));
    v.glideMs = value (params::vokalGlide);
    v.shift = modulated (params::vokalShift, 3, fraza);
    v.resonance = modulated (params::vokalResonance, 4, fraza);
    v.accent = value (params::vokalAccent);
    v.character = (int) value (params::vokalCharacter);
    v.drive = value (params::vokalDrive);
    v.mix = value (params::vokalMix);
    chain.vokal.setParams (v);

    // Polimetar Gate.
    PolimetarGate::Params g;
    g.lowHz = value (params::gateLow);
    g.highHz = value (params::gateHigh);
    g.mix = value (params::gateMix);
    for (int b = 0; b < 3; ++b)
    {
        auto& band = g.bands[(size_t) b];
        band.euclidMode = value (params::gateBand (b, "mode")) > 0.5f;
        band.steps = (int) value (params::gateBand (b, "steps"));
        band.hits = (int) value (params::gateBand (b, "hits"));
        band.rotate = (int) value (params::gateBand (b, "rotate"));
        band.division = (int) value (params::gateBand (b, "rate"));
        band.depth = juce::jlimit (0.0f, 1.0f, value (params::gateBand (b, "depth"))
                                                   + value (params::frazaTargets[5].id) * fraza);
        band.length = value (params::gateBand (b, "length"));
        band.attackMs = value (params::gateBand (b, "attack"));
        band.releaseMs = value (params::gateBand (b, "release"));
        band.pattern = gatePatterns[(size_t) b].load();
    }
    chain.gate.setParams (g);
}

void OriginPsyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    for (int ch = numIn; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
    if (numSamples == 0) return;

    const auto t = readTransport (numSamples);

    Fraza::Params fp;
    const int barsChoice = (int) value (params::frazaBars);
    fp.bars = barsChoice == 0 ? 8 : (barsChoice == 1 ? 16 : 32);
    fp.offset = (int) value (params::frazaOffset);
    fp.curve = (int) value (params::frazaCurve);
    const double phrasePos = Fraza::phrasePosition (fp, t.ppqAtBlockStart, t.beatsPerBar);
    const float fraza = value (params::frazaOn) > 0.5f ? Fraza::evaluate (fp, phrasePos) : 0.0f;

    updateModules (fraza);

    float* left = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
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

    meters.vokalNote = chain.vokal.getDetectedNote();
    meters.vokalVowel = chain.vokal.getVowelPosition();
    meters.vokalAccent = chain.vokal.getAccent();
    for (int b = 0; b < 3; ++b) meters.gateStep[(size_t) b] = chain.gate.getStep (b);
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

void OriginPsyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    for (int b = 0; b < 3; ++b)
        state.setProperty ("gate_pattern_" + juce::String (b + 1), (juce::int64) gatePatterns[(size_t) b].load(), nullptr);
    state.setProperty ("version", JucePlugin_VersionString, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OriginPsyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.hasType (apvts.state.getType())) return;
        for (int b = 0; b < 3; ++b)
        {
            const auto key = "gate_pattern_" + juce::String (b + 1);
            if (state.hasProperty (key))
                gatePatterns[(size_t) b] = (uint32_t) (juce::int64) state.getProperty (key);
        }
        apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* OriginPsyProcessor::createEditor()
{
    return new OriginPsyEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OriginPsyProcessor();
}
