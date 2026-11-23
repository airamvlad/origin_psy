#include "PluginEditor.h"

using namespace originpsy;
using namespace originpsy::ui;

namespace
{
juce::Colour moduleColour (int type)
{
    switch ((ModuleType) type)
    {
        case ModuleType::fraktalEcho:   return colours::cyan;
        case ModuleType::vokalAcid:     return colours::magenta;
        case ModuleType::polimetarGate: return colours::amber;
        case ModuleType::zapResonator:  return colours::lime;
        case ModuleType::orbitDoppler:  return colours::blue;
        case ModuleType::tapeStop:      return colours::orange;
        case ModuleType::empty:         break;
    }
    return colours::dim;
}

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour accent, const juce::String& title)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (accent);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (title, r.reduced (12.0f, 6.0f).removeFromTop (18.0f), juce::Justification::topLeft);
    }
}

const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
} // namespace

//==============================================================================
PsyLookAndFeel::PsyLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, colours::panelLight);
    setColour (juce::ComboBox::outlineColourId, colours::outline);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::dim);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::panelLight);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::cyan);
    setColour (juce::Label::textColourId, colours::dim);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, colours::panelLight);
    setColour (juce::TextButton::textColourOffId, colours::text);
    setColour (juce::ToggleButton::textColourId, colours::text);
}

void PsyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start,
                                       float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f, start, end, true);
    g.setColour (colours::outline);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Bipolar ranges draw from the centre.
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? start + (float) s.valueToProportionOfLength (0.0) * (end - start) : start;
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 3.0f, radius - 3.0f, 0.0f, std::min (from, angle),
                         std::max (from, angle), true);
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (colours::panelLight);
    g.fillEllipse (juce::Rectangle<float> (radius * 1.3f, radius * 1.3f).withCentre (centre));
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -radius * 0.62f, 3.0f, radius * 0.32f, 1.5f);
    g.setColour (colours::text);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void PsyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (on ? accent.withAlpha (0.2f) : colours::panelLight);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (on ? accent : (highlighted ? colours::dim : colours::outline));
    g.drawRoundedRectangle (r, 5.0f, 1.2f);
    g.setColour (on ? colours::text : colours::dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

//==============================================================================
juce::Colour originpsy::ui::sourceColour (ModSource s)
{
    switch (s)
    {
        case ModSource::lfo1:     return juce::Colour (0xff6ea8ff);
        case ModSource::lfo2:     return juce::Colour (0xffa98bff);
        case ModSource::envelope: return juce::Colour (0xff7dff9b);
        case ModSource::macro1:
        case ModSource::macro2:
        case ModSource::macro3:
        case ModSource::macro4:   return juce::Colour (0xffffe066);
        case ModSource::fraza:    return colours::cyan;
        case ModSource::none:     break;
    }
    return colours::dim;
}

//==============================================================================
void ModSlider::mouseDown (const juce::MouseEvent& e)
{
    auto* p = processor ? processor() : nullptr;
    const int idx = modIndex ? modIndex() : -1;
    if (p == nullptr || idx < 0) { Slider::mouseDown (e); return; }

    std::vector<int> mine;
    for (int i = 0; i < ModMatrix::numSlots; ++i)
    {
        const auto slot = p->matrix.get (i);
        if (slot.used() && slot.destination == idx) mine.push_back (i);
    }

    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        if (mine.empty())
            menu.addItem (-1, "Drag a source from the MOD row onto this knob", false);
        for (int i : mine)
        {
            const auto slot = p->matrix.get (i);
            const juce::String name = modSourceName (slot.source);
            const juce::String depth = juce::String (juce::roundToInt (std::abs (slot.depth) * 100.0f)) + " %";
            menu.addSectionHeader (name + "  " + depth);
            const bool down = slot.depth < 0.0f;
            menu.addItem (1000 + i * 10, "Up", true, ! slot.bipolar && ! down);
            menu.addItem (1001 + i * 10, "Down", true, ! slot.bipolar && down);
            menu.addItem (1002 + i * 10, "Both ways", true, slot.bipolar && ! down);
            menu.addItem (1003 + i * 10, "Both ways, inverted", true, slot.bipolar && down);
            menu.addItem (1004 + i * 10, "Remove " + name);
        }
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [p, safe = juce::Component::SafePointer<ModSlider> (this)] (int result) {
                                if (result < 1000) return;
                                const int i = (result - 1000) / 10, action = (result - 1000) % 10;
                                const float amount = std::max (0.01f, std::abs (p->matrix.get (i).depth));
                                switch (action)
                                {
                                    case 0: p->matrix.setBipolar (i, false); p->matrix.setDepth (i, amount); break;
                                    case 1: p->matrix.setBipolar (i, false); p->matrix.setDepth (i, -amount); break;
                                    case 2: p->matrix.setBipolar (i, true); p->matrix.setDepth (i, amount); break;
                                    case 3: p->matrix.setBipolar (i, true); p->matrix.setDepth (i, -amount); break;
                                    default: p->matrix.clear (i); break;
                                }
                                if (safe != nullptr && safe->getParentComponent() != nullptr)
                                    safe->getParentComponent()->repaint();
                            });
        return;
    }

    if (e.mods.isShiftDown() && ! mine.empty())
    {
        depthSlot = mine.back();
        depthStart = p->matrix.get (depthSlot).depth;
        return;
    }
    Slider::mouseDown (e);
}

void ModSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (depthSlot >= 0)
    {
        if (auto* p = processor ? processor() : nullptr)
            p->matrix.setDepth (depthSlot, depthStart - (float) e.getDistanceFromDragStartY() / 150.0f);
        if (auto* parent = getParentComponent()) parent->repaint();
        return;
    }
    Slider::mouseDrag (e);
}

void ModSlider::mouseUp (const juce::MouseEvent& e)
{
    if (depthSlot >= 0) { depthSlot = -1; return; }
    Slider::mouseUp (e);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& name,
            juce::Colour accent, bool horizontal)
    : paramId (id), compact (horizontal)
{
    slider.setSliderStyle (compact ? juce::Slider::LinearHorizontal : juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 14);
    slider.setColour (juce::Slider::trackColourId, accent);
    slider.setColour (juce::Slider::thumbColourId, colours::text);
    slider.setColour (juce::Slider::backgroundColourId, colours::outline);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, colours::text);
    slider.modIndex = [this] { return modIndex(); };
    slider.processor = [this] { return findProcessor(); };
    addAndMakeVisible (slider);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f));
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    if (auto* p = state.getParameter (paramId))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

OriginPsyProcessor* Knob::findProcessor() const
{
    if (auto* editor = findParentComponentOfClass<OriginPsyEditor>()) return &editor->getProcessor();
    return nullptr;
}

int Knob::modIndex() const
{
    if (cachedIndex == -2)
        if (auto* p = findProcessor()) cachedIndex = p->modulatableIndex (paramId);
    return cachedIndex < 0 ? -1 : cachedIndex;
}

void Knob::resized()
{
    auto r = getLocalBounds();
    if (compact)
    {
        if (label.getText().isNotEmpty()) label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
        return;
    }
    label.setBounds (r.removeFromBottom (16));
    slider.setBounds (r);
}

bool Knob::isInterestedInDragSource (const SourceDetails& d)
{
    return d.description.toString().startsWith ("mod:") && modIndex() >= 0;
}

void Knob::itemDropped (const SourceDetails& d)
{
    dragOver = false;
    const int src = d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
    if (auto* p = findProcessor(); p != nullptr && src > 0 && src < numModSources && modIndex() >= 0)
        p->matrix.connect ((ModSource) src, modIndex(), 0.25f);
    repaint();
}

void Knob::paintOverChildren (juce::Graphics& g)
{
    if (compact) return;
    auto* p = findProcessor();
    const int idx = modIndex();
    if (p == nullptr || idx < 0) return;

    const auto area = slider.getBounds().withTrimmedBottom (14).toFloat().reduced (4.0f);
    const float radius = std::min (area.getWidth(), area.getHeight()) * 0.5f;
    const auto centre = area.getCentre();
    const auto rotary = slider.getRotaryParameters();
    auto angleOf = [&] (float norm) {
        return rotary.startAngleRadians + juce::jlimit (0.0f, 1.0f, norm) * (rotary.endAngleRadians - rotary.startAngleRadians);
    };

    if (dragOver)
    {
        g.setColour (colours::text.withAlpha (0.5f));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f + 6.0f, radius * 2.0f + 6.0f).withCentre (centre), 1.5f);
    }

    const float base = p->getModulatableParameters()[(size_t) idx]->getValue();
    int ring = 0;
    bool any = false;
    for (int i = 0; i < ModMatrix::numSlots; ++i)
    {
        const auto slot = p->matrix.get (i);
        if (! slot.used() || slot.destination != idx) continue;
        any = true;
        const float lo = slot.bipolar ? base - std::abs (slot.depth) : std::min (base, base + slot.depth);
        const float hi = slot.bipolar ? base + std::abs (slot.depth) : std::max (base, base + slot.depth);
        const float r = radius + 1.5f + 3.0f * (float) ring++;
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, r, r, 0.0f, angleOf (lo), angleOf (hi), true);
        g.setColour (sourceColour (slot.source).withAlpha (slot.depth < 0 ? 0.55f : 0.9f));
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (any)
    {
        // Where the parameter really is right now.
        const float a = angleOf (p->modulatedNormalised (idx));
        const float r = radius - 3.0f;
        const juce::Point<float> dot (centre.x + r * std::sin (a), centre.y - r * std::cos (a));
        g.setColour (colours::text);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (dot));
    }
}

//==============================================================================
void SourceChip::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto c = sourceColour (source);
    g.setColour (c.withAlpha (0.15f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (c);
    g.drawRoundedRectangle (r, 5.0f, 1.2f);

    // Live value as a thin bar along the bottom.
    const float v = proc.meters.source[(size_t) source].load();
    auto bar = r.reduced (5.0f, 0.0f).removeFromBottom (4.0f).translated (0.0f, -3.0f);
    if (isBipolar (source))
    {
        const float mid = bar.getCentreX(), x = mid + v * bar.getWidth() * 0.5f;
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (std::min (mid, x), bar.getY(), std::max (mid, x), bar.getBottom()));
    }
    else
    {
        g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, v)));
    }

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (modSourceName (source), r.withTrimmedBottom (6.0f), juce::Justification::centred);
}

void SourceChip::mouseDrag (const juce::MouseEvent&)
{
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! container->isDragAndDropActive())
            container->startDragging ("mod:" + juce::String ((int) source), this);
}

//==============================================================================
Choice::Choice (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& name)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    label.setText (name, juce::dontSendNotification);
    label.setFont (juce::FontOptions (11.5f));
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramId, box);
}

void Choice::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    box.setBounds (r.removeFromTop (26));
}

//==============================================================================
EchoPage::EchoPage (OriginPsyProcessor& p)
    : proc (p),
      time (p.apvts, params::echoTime, "Time"),
      scale (p.apvts, params::echoScale, "Scale"),
      ratio (p.apvts, params::echoRatio, "Branch"),
      generations (p.apvts, params::echoGenerations, "Generations"),
      spread (p.apvts, params::echoSpread, "Pitch Spread"),
      feedback (p.apvts, params::echoFeedback, "Feedback"),
      damp (p.apvts, params::echoDamp, "Damp"),
      width (p.apvts, params::echoWidth, "Width"),
      mix (p.apvts, params::echoMix, "Mix")
{
    for (auto* c : std::initializer_list<juce::Component*> { &time, &scale, &ratio, &generations, &spread,
                                                             &feedback, &damp, &width, &mix })
        addAndMakeVisible (c);
}

void EchoPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPanel (g, r, colours::cyan, "FRACTAL ECHO");

    // Tree of echoes: time to the right, pitch up and down.
    auto area = r.withWidth (560.0f).reduced (24.0f, 34.0f).withTrimmedTop (4.0f);
    std::array<FraktalEcho::Tap, FraktalEcho::maxTaps> taps {};
    const int count = proc.getEchoTaps (taps);
    double maxDelay = 1.0;
    int maxDeg = 1;
    for (int i = 0; i < count; ++i)
    {
        maxDelay = std::max (maxDelay, taps[(size_t) i].delayBeats);
        maxDeg = std::max (maxDeg, std::abs (taps[(size_t) i].degrees));
    }

    auto pointOf = [&] (const FraktalEcho::Tap& t) {
        const float x = area.getX() + (float) (t.delayBeats / maxDelay) * area.getWidth();
        const float y = area.getCentreY() - (float) t.degrees / (float) maxDeg * area.getHeight() * 0.45f;
        return juce::Point<float> (x, y);
    };

    g.setColour (colours::outline);
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
    const juce::Point<float> origin (area.getX(), area.getCentreY());
    g.setColour (colours::dim);
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (origin));

    for (int i = 0; i < count; ++i)
    {
        const auto pt = pointOf (taps[(size_t) i]);
        const auto parent = i == 0 ? origin : pointOf (taps[(size_t) ((i - 1) / 2)]);
        const float alpha = 1.0f - 0.18f * (float) (taps[(size_t) i].generation - 1);
        g.setColour (colours::cyan.withAlpha (alpha * 0.6f));
        g.drawLine (juce::Line<float> (parent, pt), 1.5f);
        g.setColour (colours::cyan.withAlpha (alpha));
        const float size = 14.0f - 2.5f * (float) taps[(size_t) i].generation;
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (pt));
    }

    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String::fromUTF8 ("time →"), area.withTop (area.getBottom() + 4.0f).withHeight (14.0f), juce::Justification::centredRight);
    g.drawText ("pitch", area.withHeight (14.0f).translated (0.0f, -18.0f), juce::Justification::centredRight);
}

void EchoPage::resized()
{
    auto r = getLocalBounds().withTrimmedLeft (576).reduced (12, 30);
    auto top = r.removeFromTop (46);
    time.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (4, 0));
    scale.setBounds (top.reduced (4, 0));
    r.removeFromTop (8);
    const int kw = r.getWidth() / 4, kh = (r.getHeight()) / 2;
    juce::Component* knobs[] = { &ratio, &generations, &spread, &feedback, &damp, &width, &mix };
    for (int i = 0; i < 7; ++i)
        knobs[i]->setBounds (r.getX() + (i % 4) * kw, r.getY() + (i / 4) * kh, kw, kh);
}

//==============================================================================
VokalPage::VokalPage (OriginPsyProcessor& p)
    : proc (p),
      character (p.apvts, params::vokalCharacter, "Character"),
      glide (p.apvts, params::vokalGlide, "Glide", colours::magenta),
      shift (p.apvts, params::vokalShift, "Formant", colours::magenta),
      resonance (p.apvts, params::vokalResonance, "Resonance", colours::magenta),
      accent (p.apvts, params::vokalAccent, "Accent", colours::magenta),
      drive (p.apvts, params::vokalDrive, "Drive", colours::magenta),
      mix (p.apvts, params::vokalMix, "Mix", colours::magenta)
{
    for (int pc = 0; pc < 12; ++pc)
    {
        vowels.push_back (std::make_unique<Choice> (p.apvts, params::vokalVowel (pc), noteNames[pc]));
        addAndMakeVisible (*vowels.back());
    }
    for (auto* c : std::initializer_list<juce::Component*> { &character, &glide, &shift, &resonance, &accent, &drive, &mix })
        addAndMakeVisible (c);
}

void VokalPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPanel (g, r, colours::magenta, "VOCAL ACID");

    // Big vowel and the note it comes from.
    auto display = r.withWidth (250.0f).reduced (16.0f, 34.0f);
    g.setColour (colours::panelLight);
    g.fillRoundedRectangle (display, 8.0f);

    const float vowel = proc.meters.vokalVowel.load();
    const int lo = juce::jlimit (0, 4, (int) std::floor (vowel)), hi = std::min (4, lo + 1);
    const float frac = juce::jlimit (0.0f, 1.0f, vowel - (float) lo);
    const char* letters[] = { "A", "E", "I", "O", "U" };
    const float accentLevel = proc.meters.vokalAccent.load();
    g.setFont (juce::FontOptions (96.0f, juce::Font::bold));
    g.setColour (colours::magenta.withAlpha (1.0f - frac).interpolatedWith (colours::text, accentLevel * 0.6f));
    g.drawText (letters[lo], display.withTrimmedBottom (40.0f), juce::Justification::centred);
    if (frac > 0.01f)
    {
        g.setColour (colours::magenta.withAlpha (frac));
        g.drawText (letters[hi], display.withTrimmedBottom (40.0f), juce::Justification::centred);
    }

    const int note = proc.meters.vokalNote.load();
    g.setFont (juce::FontOptions (15.0f));
    g.setColour (colours::text);
    const juce::String noteText = note >= 0 ? juce::String (noteNames[note % 12]) + juce::String (note / 12 - 1)
                                            : juce::String ("no pitch");
    g.drawText (noteText, display.removeFromBottom (40.0f), juce::Justification::centred);

    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText ("Note > vowel", juce::Rectangle<float> (266.0f, 30.0f, 400.0f, 16.0f), juce::Justification::left);
    g.drawFittedText (juce::String (
                          "Every note the line plays gets its own vowel, and Glide sets how fast the sound slides "
                          "from one to the next. An accent (a sudden jump in level) briefly pushes the resonance up. "
                          "Robot locks the formants onto the harmonics of the note for a metallic sound."),
                      juce::Rectangle<int> (266, 172, 400, 80), juce::Justification::topLeft, 5);
}

void VokalPage::resized()
{
    for (int pc = 0; pc < 12; ++pc)
        vowels[(size_t) pc]->setBounds (266 + (pc % 6) * 66, 52 + (pc / 6) * 50, 62, 44);

    auto right = getLocalBounds().withTrimmedLeft (680).reduced (12, 30);
    character.setBounds (right.removeFromTop (46).reduced (4, 0));
    right.removeFromTop (6);
    const int kw = right.getWidth() / 3, kh = right.getHeight() / 2;
    juce::Component* knobs[] = { &glide, &shift, &resonance, &accent, &drive, &mix };
    for (int i = 0; i < 6; ++i)
        knobs[i]->setBounds (right.getX() + (i % 3) * kw, right.getY() + (i / 3) * kh, kw, kh);
}

//==============================================================================
ZapPage::ZapPage (OriginPsyProcessor& p)
    : proc (p),
      scale (p.apvts, params::zapScale, "Scale"),
      root (p.apvts, params::zapRoot, "Root"),
      order (p.apvts, params::zapOrder, "Note order"),
      threshold (p.apvts, params::zapThreshold, "Threshold", colours::lime),
      octave (p.apvts, params::zapOctave, "Octave", colours::lime),
      sweep (p.apvts, params::zapSweep, "Sweep", colours::lime),
      sweepTime (p.apvts, params::zapSweepTime, "Sweep Time", colours::lime),
      decay (p.apvts, params::zapDecay, "Decay", colours::lime),
      resonance (p.apvts, params::zapResonance, "Resonance", colours::lime),
      width (p.apvts, params::zapWidth, "Width", colours::lime),
      mix (p.apvts, params::zapMix, "Mix", colours::lime)
{
    for (auto* c : std::initializer_list<juce::Component*> { &scale, &root, &order, &threshold, &octave, &sweep,
                                                             &sweepTime, &decay, &resonance, &width, &mix })
        addAndMakeVisible (c);
}

void ZapPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPanel (g, r, colours::lime, "ZAP RESONATOR");

    auto value = [this] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
    ZapResonator::Params zp;
    zp.scale = (int) value (params::zapScale);
    zp.root = (int) value (params::zapRoot);
    zp.octave = (int) value (params::zapOctave);
    const float sweepSt = value (params::zapSweep);
    const float sweepMs = value (params::zapSweepTime), decayMs = value (params::zapDecay);

    const auto hits = proc.meters.zapHits.load();
    if (hits != lastHits) { lastHits = hits; flash = 1.0f; }
    else flash *= 0.85f;

    // One zap: pitch glides from the sweep onto the note while it fades.
    auto area = r.withWidth (560.0f).reduced (24.0f, 34.0f).withTrimmedBottom (54.0f);
    g.setColour (colours::outline);
    const float noteY = area.getY() + area.getHeight() * (sweepSt >= 0.0f ? 0.8f : 0.2f);
    g.drawHorizontalLine ((int) noteY, area.getX(), area.getRight());
    const float span = std::max (decayMs * 1.2f, 50.0f);
    juce::Path curve;
    for (int i = 0; i <= 120; ++i)
    {
        const float tMs = span * (float) i / 120.0f;
        const float pitch = sweepSt * std::exp (-tMs / std::max (1.0f, sweepMs));
        const float x = area.getX() + area.getWidth() * (float) i / 120.0f;
        const float y = noteY - pitch / 48.0f * area.getHeight() * 0.75f;
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    g.setColour (colours::lime.withAlpha (0.35f + 0.65f * flash));
    g.strokePath (curve, juce::PathStrokeType (2.0f + 2.0f * flash));
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String::fromUTF8 ("time →"), area.withTop (area.getBottom() + 2.0f).withHeight (14.0f), juce::Justification::centredRight);
    g.drawText ("pitch of one zap", area.withHeight (14.0f).translated (0.0f, -18.0f), juce::Justification::centredRight);

    // The notes zaps land on; the last one lights up.
    const int count = ZapResonator::notesInRange (zp);
    const int last = proc.meters.zapNote.load();
    auto row = r.withWidth (560.0f).reduced (24.0f, 0.0f).withTop (r.getBottom() - 70.0f).withHeight (34.0f);
    const float w = row.getWidth() / (float) count;
    for (int i = 0; i < count; ++i)
    {
        const int note = ZapResonator::noteForStep (zp, i);
        auto cell = juce::Rectangle<float> (row.getX() + w * (float) i, row.getY(), w, row.getHeight()).reduced (1.5f, 0.0f);
        const bool lit = note == last;
        g.setColour (lit ? colours::lime.withAlpha (0.3f + 0.7f * flash) : colours::panelLight);
        g.fillRoundedRectangle (cell, 3.0f);
        g.setColour (lit ? colours::text : colours::dim);
        g.setFont (juce::FontOptions (w > 22.0f ? 11.0f : 9.0f));
        g.drawText (noteNames[note % 12] + juce::String (note / 12 - 1), cell, juce::Justification::centred);
    }
}

void ZapPage::resized()
{
    auto r = getLocalBounds().withTrimmedLeft (576).reduced (12, 30);
    auto top = r.removeFromTop (46);
    const int cw = top.getWidth() / 3;
    scale.setBounds (top.removeFromLeft (cw).reduced (4, 0));
    root.setBounds (top.removeFromLeft (cw).reduced (4, 0));
    order.setBounds (top.reduced (4, 0));
    r.removeFromTop (8);
    const int kw = r.getWidth() / 4, kh = r.getHeight() / 2;
    juce::Component* knobs[] = { &threshold, &octave, &sweep, &sweepTime, &decay, &resonance, &width, &mix };
    for (int i = 0; i < 8; ++i)
        knobs[i]->setBounds (r.getX() + (i % 4) * kw, r.getY() + (i / 4) * kh, kw, kh);
}

//==============================================================================
OrbitPage::OrbitPage (OriginPsyProcessor& p)
    : proc (p),
      path (p.apvts, params::orbitPath, "Path"),
      rate (p.apvts, params::orbitRate, "One lap"),
      radius (p.apvts, params::orbitRadius, "Radius", colours::blue),
      doppler (p.apvts, params::orbitDoppler, "Doppler", colours::blue),
      tone (p.apvts, params::orbitTone, "Distance Tone", colours::blue),
      width (p.apvts, params::orbitWidth, "Width", colours::blue),
      mix (p.apvts, params::orbitMix, "Mix", colours::blue)
{
    for (auto* c : std::initializer_list<juce::Component*> { &path, &rate, &radius, &doppler, &tone, &width, &mix })
        addAndMakeVisible (c);
}

void OrbitPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPanel (g, r, colours::blue, "ORBIT DOPPLER");

    // Seen from above: the listener in the middle, facing up.
    auto area = r.withWidth (560.0f).reduced (24.0f, 30.0f).withTrimmedTop (6.0f);
    const int pathIndex = (int) proc.apvts.getRawParameterValue (params::orbitPath)->load();
    const float w = proc.apvts.getRawParameterValue (params::orbitWidth)->load();
    const float scale = std::min (area.getWidth() / 5.0f, area.getHeight() / 2.8f);
    const juce::Point<float> head (area.getCentreX(), area.getY() + 1.7f * scale);
    auto toScreen = [&] (float x, float y) { return juce::Point<float> (head.x + x * w * scale, head.y - y * scale); };

    juce::Path orbit;
    for (int i = 0; i <= 240; ++i)
    {
        const auto pt = OrbitDoppler::pathPoint (pathIndex, i / 240.0);
        const auto s = toScreen (pt.x, pt.y);
        if (i == 0) orbit.startNewSubPath (s); else orbit.lineTo (s);
    }
    g.setColour (colours::blue.withAlpha (0.35f));
    g.strokePath (orbit, juce::PathStrokeType (1.5f));

    // Head with ears and nose.
    g.setColour (colours::panelLight);
    g.fillEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (head));
    g.setColour (colours::dim);
    g.drawEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (head), 1.2f);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 10.0f).withCentre (head.translated (-14.0f, 0.0f)));
    g.fillEllipse (juce::Rectangle<float> (6.0f, 10.0f).withCentre (head.translated (14.0f, 0.0f)));
    juce::Path nose;
    nose.addTriangle (head.x - 4.0f, head.y - 12.0f, head.x + 4.0f, head.y - 12.0f, head.x, head.y - 19.0f);
    g.fillPath (nose);

    // The sound, with a short trail.
    const auto now = toScreen (proc.meters.orbitX.load(), proc.meters.orbitY.load());
    trail[(size_t) trailPos] = now;
    trailPos = (trailPos + 1) % (int) trail.size();
    for (size_t i = 0; i < trail.size(); ++i)
    {
        const auto age = (float) (((size_t) trailPos + trail.size() - 1 - i) % trail.size()) / (float) trail.size();
        if (trail[i] == juce::Point<float>()) continue;
        g.setColour (colours::blue.withAlpha (0.5f * (1.0f - age)));
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (trail[i]));
    }
    g.setColour (colours::text);
    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (now));
    g.setColour (colours::blue);
    g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (now), 2.0f);

    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("front", area.withHeight (14.0f), juce::Justification::centredTop);
}

void OrbitPage::resized()
{
    auto r = getLocalBounds().withTrimmedLeft (576).reduced (12, 30);
    auto top = r.removeFromTop (46);
    path.setBounds (top.removeFromLeft (top.getWidth() / 2).reduced (4, 0));
    rate.setBounds (top.reduced (4, 0));
    r.removeFromTop (8);
    const int kw = r.getWidth() / 4, kh = r.getHeight() / 2;
    juce::Component* knobs[] = { &radius, &doppler, &tone, &width, &mix };
    for (int i = 0; i < 5; ++i)
        knobs[i]->setBounds (r.getX() + (i % 4) * kw, r.getY() + (i / 4) * kh, kw, kh);
}

//==============================================================================
TapePage::TapePage (OriginPsyProcessor& p)
    : proc (p),
      mode (p.apvts, params::tapeMode, "Mode"),
      trigger (p.apvts, params::tapeTrigger, "Trigger"),
      length (p.apvts, params::tapeLength, "Length"),
      curve (p.apvts, params::tapeCurve, "Curve", colours::orange),
      mix (p.apvts, params::tapeMix, "Mix", colours::orange)
{
    for (auto* c : std::initializer_list<juce::Component*> { &mode, &trigger, &length, &curve, &mix, &brake })
        addAndMakeVisible (c);
    // Hold to brake; let go to come back.
    brake.setColour (juce::TextButton::buttonColourId, colours::orange.withAlpha (0.25f));
    brake.setColour (juce::TextButton::buttonOnColourId, colours::orange.withAlpha (0.8f));
    brake.setTooltip ("Hold to slow the tape down, let go to bring it back. Automate Tape Brake to play it from the song.");
    brake.onStateChange = [this] {
        const bool down = brake.isDown();
        const bool on = proc.apvts.getRawParameterValue (params::tapeBrake)->load() > 0.5f;
        if (down != on) setBrake (down);
    };
}

void TapePage::setBrake (bool on)
{
    if (auto* prm = proc.apvts.getParameter (params::tapeBrake))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (on ? 1.0f : 0.0f);
        prm->endChangeGesture();
    }
}

void TapePage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPanel (g, r, colours::orange, "TAPE STOP");
    brake.setToggleState (proc.apvts.getRawParameterValue (params::tapeBrake)->load() > 0.5f, juce::dontSendNotification);

    // Two reels that turn at the tape speed.
    const float speed = proc.meters.tapeSpeed.load();
    reelAngle += speed * 0.35f;
    if (reelAngle > juce::MathConstants<float>::twoPi) reelAngle -= juce::MathConstants<float>::twoPi;
    auto area = r.withWidth (560.0f).reduced (24.0f, 34.0f);
    const float rad = std::min (area.getHeight() * 0.36f, area.getWidth() * 0.18f);
    const juce::Point<float> c1 (area.getCentreX() - rad * 1.45f, area.getY() + rad + 10.0f);
    const juce::Point<float> c2 (area.getCentreX() + rad * 1.45f, c1.y);
    g.setColour (colours::outline);
    g.drawLine (c1.x, c1.y + rad, c2.x, c2.y + rad, 2.0f);
    for (auto c : { c1, c2 })
    {
        g.setColour (colours::panelLight);
        g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c));
        g.setColour (colours::orange.withAlpha (0.4f + 0.6f * speed));
        g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c), 2.0f);
        for (int k = 0; k < 3; ++k)
        {
            const float a = reelAngle + (float) k * juce::MathConstants<float>::twoPi / 3.0f;
            g.drawLine (c.x + std::sin (a) * rad * 0.2f, c.y - std::cos (a) * rad * 0.2f,
                        c.x + std::sin (a) * rad * 0.85f, c.y - std::cos (a) * rad * 0.85f, 3.0f);
        }
        g.fillEllipse (juce::Rectangle<float> (rad * 0.3f, rad * 0.3f).withCentre (c));
    }

    // Speed bar.
    auto bar = area.withTop (c1.y + rad + 26.0f).withHeight (14.0f).reduced (40.0f, 0.0f);
    g.setColour (colours::panelLight);
    g.fillRoundedRectangle (bar, 4.0f);
    g.setColour (colours::orange);
    g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, speed)), 4.0f);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("speed " + juce::String (juce::roundToInt (speed * 100.0f)) + " %", bar.translated (0.0f, 18.0f),
                juce::Justification::centred);
}

void TapePage::resized()
{
    auto r = getLocalBounds().withTrimmedLeft (576).reduced (12, 30);
    auto top = r.removeFromTop (46);
    const int cw = top.getWidth() / 3;
    mode.setBounds (top.removeFromLeft (cw).reduced (4, 0));
    trigger.setBounds (top.removeFromLeft (cw).reduced (4, 0));
    length.setBounds (top.reduced (4, 0));
    r.removeFromTop (8);
    const int kh = r.getHeight() / 2, kw = r.getWidth() / 4;
    curve.setBounds (r.getX(), r.getY(), kw, kh);
    mix.setBounds (r.getX() + kw, r.getY(), kw, kh);
    brake.setBounds (juce::Rectangle<int> (r.getX() + 2 * kw, r.getY(), 2 * kw, kh).reduced (14, 22));
}

//==============================================================================
GateRing::GateRing (OriginPsyProcessor& p, int b) : proc (p), band (b) {}

void GateRing::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (6.0f);
    const auto centre = r.getCentre();
    const float radius = std::min (r.getWidth(), r.getHeight()) * 0.5f;

    PolimetarGate::Band b;
    auto& s = proc.apvts;
    b.euclidMode = s.getRawParameterValue (params::gateBand (band, "mode"))->load() > 0.5f;
    b.steps = (int) s.getRawParameterValue (params::gateBand (band, "steps"))->load();
    b.hits = (int) s.getRawParameterValue (params::gateBand (band, "hits"))->load();
    b.rotate = (int) s.getRawParameterValue (params::gateBand (band, "rotate"))->load();
    b.pattern = proc.getGatePattern (band);
    const uint32_t mask = PolimetarGate::activeMask (b);
    const int current = proc.meters.gateStep[(size_t) band].load();

    for (int i = 0; i < b.steps; ++i)
    {
        const float a0 = juce::MathConstants<float>::twoPi * (float) i / (float) b.steps;
        const float a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / (float) b.steps;
        juce::Path seg;
        seg.addPieSegment (r, a0 + 0.03f, a1 - 0.03f, 0.62f);
        const bool on = (mask >> i) & 1u;
        auto c = on ? colours::amber : colours::panelLight;
        if (i == current) c = on ? colours::text : colours::outline;
        g.setColour (c);
        g.fillPath (seg);
    }

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (juce::String (b.steps), juce::Rectangle<float> (radius, radius).withCentre (centre),
                juce::Justification::centred);
}

void GateRing::mouseDown (const juce::MouseEvent& e)
{
    // Drawing steps works in Steps mode; in Euclid mode the ring only shows.
    if (proc.apvts.getRawParameterValue (params::gateBand (band, "mode"))->load() > 0.5f) return;
    const int steps = (int) proc.apvts.getRawParameterValue (params::gateBand (band, "steps"))->load();
    const auto d = e.position - getLocalBounds().toFloat().getCentre();
    float angle = std::atan2 (d.x, -d.y);
    if (angle < 0.0f) angle += juce::MathConstants<float>::twoPi;
    const int step = juce::jlimit (0, steps - 1, (int) (angle / juce::MathConstants<float>::twoPi * (float) steps));
    proc.toggleGateStep (band, step);
    repaint();
}

GatePage::Column::Column (OriginPsyProcessor& p, int b)
{
    auto& s = p.apvts;
    ring = std::make_unique<GateRing> (p, b);
    mode = std::make_unique<Choice> (s, params::gateBand (b, "mode"), "Mode");
    rate = std::make_unique<Choice> (s, params::gateBand (b, "rate"), "Rate");
    steps = std::make_unique<Knob> (s, params::gateBand (b, "steps"), "Steps", colours::amber);
    hits = std::make_unique<Knob> (s, params::gateBand (b, "hits"), "Hits", colours::amber);
    rotate = std::make_unique<Knob> (s, params::gateBand (b, "rotate"), "Rotate", colours::amber);
    depth = std::make_unique<Knob> (s, params::gateBand (b, "depth"), "Depth", colours::amber);
    length = std::make_unique<Knob> (s, params::gateBand (b, "length"), "Length", colours::amber);
    attack = std::make_unique<Knob> (s, params::gateBand (b, "attack"), "Attack", colours::amber);
    release = std::make_unique<Knob> (s, params::gateBand (b, "release"), "Release", colours::amber);
}

GatePage::GatePage (OriginPsyProcessor& p)
    : proc (p),
      low (p.apvts, params::gateLow, "Low | Mid", colours::amber),
      high (p.apvts, params::gateHigh, "Mid | High", colours::amber),
      mix (p.apvts, params::gateMix, "Mix", colours::amber)
{
    for (int b = 0; b < 3; ++b)
    {
        columns.push_back (std::make_unique<Column> (p, b));
        auto& c = *columns.back();
        for (auto* comp : std::initializer_list<juce::Component*> { c.ring.get(), c.mode.get(), c.rate.get(), c.steps.get(),
                                                                    c.hits.get(), c.rotate.get(), c.depth.get(),
                                                                    c.length.get(), c.attack.get(), c.release.get() })
            addAndMakeVisible (comp);
    }
    addAndMakeVisible (low);
    addAndMakeVisible (high);
    addAndMakeVisible (mix);
}

void GatePage::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), colours::amber, "POLYMETER GATE");
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    const char* names[] = { "LOW", "MID", "HIGH" };
    for (int b = 0; b < 3; ++b)
        g.drawText (names[b], 16 + b * 300, 28, 120, 16, juce::Justification::left);
    g.setColour (colours::outline);
    for (int b = 1; b < 3; ++b)
        g.drawVerticalLine (12 + b * 300, 34.0f, (float) getHeight() - 12.0f);
}

void GatePage::resized()
{
    for (int b = 0; b < 3; ++b)
    {
        auto& c = *columns[(size_t) b];
        auto r = juce::Rectangle<int> (16 + b * 300, 46, 290, getHeight() - 54);
        auto top = r.removeFromTop (138);
        c.ring->setBounds (top.removeFromLeft (124));
        top.removeFromLeft (6);
        auto choices = top.removeFromTop (46);
        c.mode->setBounds (choices.removeFromLeft (choices.getWidth() / 2).reduced (2, 0));
        c.rate->setBounds (choices.reduced (2, 0));
        const int sw = top.getWidth() / 3;
        c.steps->setBounds (top.removeFromLeft (sw));
        c.hits->setBounds (top.removeFromLeft (sw));
        c.rotate->setBounds (top);
        const int kw = r.getWidth() / 4;
        c.depth->setBounds (r.removeFromLeft (kw));
        c.length->setBounds (r.removeFromLeft (kw));
        c.attack->setBounds (r.removeFromLeft (kw));
        c.release->setBounds (r);
    }
    auto right = getLocalBounds().withTrimmedLeft (912).reduced (4, 34);
    const int kh = right.getHeight() / 3;
    low.setBounds (right.removeFromTop (kh));
    high.setBounds (right.removeFromTop (kh));
    mix.setBounds (right);
}

//==============================================================================
void PhraseView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::panelLight);
    g.fillRoundedRectangle (r, 6.0f);
    r = r.reduced (8.0f, 10.0f);

    Fraza::Params fp;
    const int barsChoice = (int) proc.apvts.getRawParameterValue (params::frazaBars)->load();
    fp.bars = barsChoice == 0 ? 8 : (barsChoice == 1 ? 16 : 32);
    fp.curve = (int) proc.apvts.getRawParameterValue (params::frazaCurve)->load();

    g.setColour (colours::outline);
    for (int bar = 0; bar <= fp.bars; ++bar)
    {
        const float x = r.getX() + r.getWidth() * (float) bar / (float) fp.bars;
        g.drawVerticalLine ((int) x, r.getBottom() - (bar % 4 == 0 ? 10.0f : 5.0f), r.getBottom());
    }

    juce::Path curve;
    for (int i = 0; i <= 200; ++i)
    {
        const double pos = i / 200.0;
        const float v = Fraza::evaluate (fp, std::min (pos, 0.99999));
        const float x = r.getX() + r.getWidth() * (float) pos;
        const float y = r.getBottom() - 12.0f - v * (r.getHeight() - 14.0f);
        if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    g.setColour (colours::cyan);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    const float x = r.getX() + r.getWidth() * (float) proc.meters.frazaPosition.load();
    g.setColour (colours::text.withAlpha (0.8f));
    g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
    // Where the curve is now: this is the value the PHRASE source sends.
    const float now = proc.meters.frazaValue.load();
    const float y = r.getBottom() - 12.0f - now * (r.getHeight() - 14.0f);
    g.setColour (colours::cyan);
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, y }));
    const float bar = (float) proc.meters.frazaPosition.load() * (float) fp.bars + 1.0f;
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("bar " + juce::String ((int) bar) + " / " + juce::String (fp.bars), r.withHeight (14.0f),
                juce::Justification::topRight);
}

FrazaStrip::FrazaStrip (OriginPsyProcessor& p)
    : view (p), proc (p),
      bars (p.apvts, params::frazaBars, "Length"),
      curve (p.apvts, params::frazaCurve, "Curve"),
      offset (p.apvts, params::frazaOffset, "Offset")
{
    on.setColour (juce::ToggleButton::tickColourId, colours::cyan);
    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, params::frazaOn, on);
    for (auto* c : std::initializer_list<juce::Component*> { &on, &bars, &curve, &offset, &view, &drop })
        addAndMakeVisible (c);
    for (const auto& t : params::frazaTargets)
    {
        amounts.push_back (std::make_unique<Knob> (p.apvts, t.id, t.label));
        addAndMakeVisible (*amounts.back());
    }
    drop.setColour (juce::TextButton::buttonColourId, colours::magenta.withAlpha (0.35f));
    drop.setTooltip ("Ready-made build-up over the last 4 bars of the phrase");
    drop.onClick = [this] { proc.applyDropPreset(); };
}

void FrazaStrip::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), colours::cyan, "PHRASE");
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText ("How far the curve moves each parameter", 572, 8, 420, 16, juce::Justification::left);
}

void FrazaStrip::resized()
{
    auto r = getLocalBounds().reduced (12, 30);
    auto left = r.removeFromLeft (150);
    on.setBounds (left.removeFromTop (28).withWidth (90));
    left.removeFromTop (6);
    bars.setBounds (left.removeFromTop (46));
    curve.setBounds (left.removeFromTop (46));

    r.removeFromLeft (10);
    auto mid = r.removeFromLeft (84);
    offset.setBounds (mid.removeFromTop (96));
    mid.removeFromTop (8);
    drop.setBounds (mid.removeFromTop (36));

    r.removeFromLeft (10);
    view.setBounds (r.removeFromLeft (300));
    r.removeFromLeft (12);
    const int kw = r.getWidth() / params::numFrazaTargets;
    for (auto& k : amounts) k->setBounds (r.removeFromLeft (kw).withSizeKeepingCentre (kw, 100));
}

//==============================================================================
ModStrip::ModStrip (OriginPsyProcessor& p) : proc (p)
{
    for (int s = 1; s < numModSources; ++s)
    {
        chips.push_back (std::make_unique<SourceChip> (p, (ModSource) s));
        chips.back()->setTooltip (juce::String ("Drag ") + modSourceName ((ModSource) s) + " onto a knob");
        addAndMakeVisible (*chips.back());
    }
    for (int i = 0; i < 2; ++i)
    {
        lfoControls.push_back (std::make_unique<Choice> (p.apvts, params::lfoRate (i), "Rate"));
        lfoControls.push_back (std::make_unique<Choice> (p.apvts, params::lfoShape (i), "Shape"));
    }
    for (int i = 0; i < 4; ++i)
        macros.push_back (std::make_unique<Knob> (p.apvts, params::macro (i), "", sourceColour (ModSource::macro1), true));
    for (auto& c : lfoControls) addAndMakeVisible (*c);
    for (auto& k : macros) addAndMakeVisible (*k);
}

void ModStrip::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), colours::text, "MOD");
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText ("Drag a source onto a knob. Shift-drag the knob to set depth, right-click it for direction or to remove.",
                getLocalBounds().reduced (12, 6).removeFromTop (18), juce::Justification::topRight);
}

void ModStrip::resized()
{
    auto r = getLocalBounds().reduced (12, 0).withTrimmedTop (30).withTrimmedBottom (8);
    auto chip = [this] (ModSource s) -> SourceChip& { return *chips[(size_t) s - 1]; };

    for (int i = 0; i < 2; ++i)
    {
        chip (i == 0 ? ModSource::lfo1 : ModSource::lfo2).setBounds (r.removeFromLeft (64).withTrimmedTop (8));
        r.removeFromLeft (6);
        lfoControls[(size_t) i * 2]->setBounds (r.removeFromLeft (78));
        r.removeFromLeft (4);
        lfoControls[(size_t) i * 2 + 1]->setBounds (r.removeFromLeft (88));
        r.removeFromLeft (16);
    }
    chip (ModSource::envelope).setBounds (r.removeFromLeft (64).withTrimmedTop (8));
    r.removeFromLeft (16);

    chip (ModSource::fraza).setBounds (r.removeFromRight (76).withTrimmedTop (8));
    r.removeFromRight (16);
    const int w = r.getWidth() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto cell = r.removeFromLeft (w).reduced (3, 0);
        chip ((ModSource) ((int) ModSource::macro1 + i)).setBounds (cell.removeFromTop (22));
        cell.removeFromTop (2);
        macros[(size_t) i]->setBounds (cell);
    }
}

//==============================================================================
PresetBar::PresetBar (OriginPsyProcessor& p) : proc (p), presets (p)
{
    for (auto* c : std::initializer_list<juce::Component*> { &box, &prev, &next, &save })
        addAndMakeVisible (c);
    box.setTextWhenNothingSelected ("Init");
    box.onChange = [this] {
        const int id = box.getSelectedId();
        if (id > 0) loadIndex (id - 1);
    };
    prev.onClick = [this] { loadIndex (juce::jmax (0, box.getSelectedId() - 2)); };
    next.onClick = [this] { loadIndex (juce::jmin ((int) entries.size() - 1, box.getSelectedId())); };
    save.onClick = [this] { askToSave(); };
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    save.setTooltip ("Save as a user preset in Documents/AMV/OriginPsy Presets");
    refresh();
}

void PresetBar::refresh()
{
    entries = presets.list();
    box.clear (juce::dontSendNotification);
    juce::String category;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].category != category)
        {
            category = entries[i].category;
            box.addSectionHeading (category);
        }
        box.addItem (entries[i].name, (int) i + 1);
    }
    syncName();
}

void PresetBar::syncName()
{
    shownName = proc.currentPresetName;
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].name == shownName)
        {
            box.setSelectedId ((int) i + 1, juce::dontSendNotification);
            return;
        }
    box.setText (shownName, juce::dontSendNotification);
}

void PresetBar::loadIndex (int index)
{
    if (index < 0 || index >= (int) entries.size()) return;
    if (presets.load (entries[(size_t) index]))
        box.setSelectedId (index + 1, juce::dontSendNotification);
    shownName = proc.currentPresetName;
    if (auto* top = getTopLevelComponent()) top->repaint();
}

void PresetBar::askToSave()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name of the preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.currentPresetName);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetBar> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result) {
                            if (result != 1 || safe == nullptr) return;
                            safe->presets.save (w->getTextEditorContents ("name"));
                            safe->refresh();
                        }),
                        true);
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    save.setBounds (r.removeFromRight (60));
    r.removeFromRight (6);
    next.setBounds (r.removeFromRight (28));
    r.removeFromRight (4);
    prev.setBounds (r.removeFromLeft (28));
    r.removeFromLeft (4);
    box.setBounds (r);
}

//==============================================================================
SlotCard::SlotCard (OriginPsyProcessor& p, int s, std::function<void (int)> select)
    : proc (p), slot (s), onSelect (std::move (select))
{
    module.addItemList (params::moduleNames(), 1);
    addAndMakeVisible (module);
    moduleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.apvts, params::slotModule (s), module);
    module.onChange = [this] { onSelect (moduleType()); repaint(); };

    // The toggle shows "ON" when the slot plays; the parameter stores bypass.
    bypass.setClickingTogglesState (true);
    bypass.setToggleState (p.apvts.getRawParameterValue (params::slotBypass (s))->load() < 0.5f, juce::dontSendNotification);
    bypass.onClick = [this] {
        if (auto* prm = proc.apvts.getParameter (params::slotBypass (slot)))
            prm->setValueNotifyingHost (bypass.getToggleState() ? 0.0f : 1.0f);
        repaint();
    };
    addAndMakeVisible (bypass);
}

int SlotCard::moduleType() const { return module.getSelectedItemIndex(); }

void SlotCard::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const auto accent = moduleColour (moduleType());
    const bool bypassed = proc.apvts.getRawParameterValue (params::slotBypass (slot))->load() > 0.5f;
    bypass.setToggleState (! bypassed, juce::dontSendNotification);
    bypass.setColour (juce::ToggleButton::tickColourId, accent);

    g.setColour (selected ? colours::panelLight : colours::panel);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (selected ? accent : colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, selected ? 2.0f : 1.0f);
    g.setColour (accent.withAlpha (bypassed ? 0.3f : 1.0f));
    g.fillRoundedRectangle (r.withWidth (5.0f).reduced (0.0f, 10.0f).translated (6.0f, 0.0f), 2.0f);

    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    g.drawText ("SLOT " + juce::String (slot + 1), r.reduced (18.0f, 8.0f), juce::Justification::topLeft);

    // A module plays in one slot only: say where it already is.
    for (int other = 0; other < slot; ++other)
        if (moduleType() > 0
            && (int) proc.apvts.getRawParameterValue (params::slotModule (other))->load() == moduleType())
        {
            g.setColour (colours::amber);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("already in slot " + juce::String (other + 1) + ", this one is skipped",
                        r.reduced (18.0f, 8.0f), juce::Justification::topRight);
            break;
        }
}

void SlotCard::resized()
{
    auto r = getLocalBounds().reduced (18, 10);
    r.removeFromTop (18);
    bypass.setBounds (r.removeFromRight (44).withSizeKeepingCentre (44, 26));
    r.removeFromRight (8);
    module.setBounds (r.withSizeKeepingCentre (r.getWidth(), 28));
}

void SlotCard::mouseUp (const juce::MouseEvent&) { onSelect (moduleType()); }

//==============================================================================
OriginPsyEditor::OriginPsyEditor (OriginPsyProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      echoPage (p), vokalPage (p), gatePage (p), zapPage (p), orbitPage (p), tapePage (p), fraza (p), modStrip (p), presetBar (p),
      output (p.apvts, params::outGain, "Output", colours::cyan, true)
{
    setLookAndFeel (&lnf);
    for (int i = 0; i < Chain::numSlots; ++i)
    {
        cards.push_back (std::make_unique<SlotCard> (p, i, [this] (int type) { showModule (type); }));
        addAndMakeVisible (*cards.back());
    }
    addChildComponent (echoPage);
    addChildComponent (vokalPage);
    addChildComponent (gatePage);
    addChildComponent (zapPage);
    addChildComponent (orbitPage);
    addChildComponent (tapePage);
    addAndMakeVisible (fraza);
    addAndMakeVisible (modStrip);
    addAndMakeVisible (presetBar);
    addAndMakeVisible (output);

    showModule (cards.front()->moduleType() > 0 ? cards.front()->moduleType() : (int) ModuleType::fraktalEcho);
    setResizable (true, true);
    setResizeLimits (1040, 790, 2080, 1580);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1040.0 / 790.0);
    setSize (1040, 790);
    startTimerHz (30);
}

OriginPsyEditor::~OriginPsyEditor() { setLookAndFeel (nullptr); }

void OriginPsyEditor::showModule (int type)
{
    if (type <= 0) return;
    shownModule = type;
    echoPage.setVisible (type == (int) ModuleType::fraktalEcho);
    vokalPage.setVisible (type == (int) ModuleType::vokalAcid);
    gatePage.setVisible (type == (int) ModuleType::polimetarGate);
    zapPage.setVisible (type == (int) ModuleType::zapResonator);
    orbitPage.setVisible (type == (int) ModuleType::orbitDoppler);
    tapePage.setVisible (type == (int) ModuleType::tapeStop);
    for (auto& c : cards)
    {
        c->selected = c->moduleType() == type;
        c->repaint();
    }
}

void OriginPsyEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 1040.0f));
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("Origin", 16, 12, 90, 36, juce::Justification::left);
    g.setColour (colours::cyan);
    g.drawText ("Psy", 94, 12, 60, 36, juce::Justification::left);

    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (12.0f));
    const juce::String tempo = juce::String (proc.meters.bpm.load(), 1) + " BPM"
                               + (proc.meters.hostPlaying.load() ? "" : "  (internal clock)");
    g.drawText (tempo, 170, 20, 150, 20, juce::Justification::left);
}

void OriginPsyEditor::resized()
{
    const float scale = (float) getWidth() / 1040.0f;
    const auto t = juce::AffineTransform::scale (scale);

    auto place = [&] (juce::Component& c, juce::Rectangle<int> r) {
        c.setTransform (t);
        c.setBounds (r);
    };

    for (int i = 0; i < Chain::numSlots; ++i)
        place (*cards[(size_t) i], { 16 + i * 255, 60, 243, 76 });
    for (juce::Component* page : { (juce::Component*) &echoPage, (juce::Component*) &vokalPage, (juce::Component*) &gatePage,
                                (juce::Component*) &zapPage, (juce::Component*) &orbitPage,
                                (juce::Component*) &tapePage })
        place (*page, { 16, 248, 1008, 316 });
    place (modStrip, { 16, 148, 1008, 88 });
    place (fraza, { 16, 576, 1008, 208 });
    place (presetBar, { 330, 14, 420, 30 });
    place (output, { 880, 4, 144, 48 });
}

void OriginPsyEditor::timerCallback()
{
    if (vokalPage.isVisible()) vokalPage.repaint();
    if (gatePage.isVisible()) gatePage.repaint();
    if (echoPage.isVisible()) echoPage.repaint();
    fraza.view.repaint();
    for (auto& c : cards) c->repaint();
    modStrip.repaint();
    // Knobs show their modulation live.
    for (juce::Component* page : { (juce::Component*) &echoPage, (juce::Component*) &vokalPage, (juce::Component*) &gatePage,
                                (juce::Component*) &zapPage, (juce::Component*) &orbitPage,
                                (juce::Component*) &tapePage })
        if (page->isVisible()) page->repaint();
    if (proc.currentPresetName != presetBar.shownName) presetBar.syncName();
    repaint (getLocalBounds().withHeight (getHeight() * 56 / 790));
}
