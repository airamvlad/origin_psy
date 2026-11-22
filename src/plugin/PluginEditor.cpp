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
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& name,
            juce::Colour accent, bool horizontal)
    : compact (horizontal)
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
    addAndMakeVisible (slider);
    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (11.5f));
    addAndMakeVisible (label);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    if (compact)
    {
        label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
        return;
    }
    label.setBounds (r.removeFromBottom (16));
    slider.setBounds (r);
}

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
    drawPanel (g, r, colours::cyan, "FRAKTAL ECHO");

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
    g.drawText (juce::String::fromUTF8 ("vrijeme →"), area.withTop (area.getBottom() + 4.0f).withHeight (14.0f), juce::Justification::centredRight);
    g.drawText ("visina tona", area.withHeight (14.0f).translated (0.0f, -18.0f), juce::Justification::centredRight);
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
    drawPanel (g, r, colours::magenta, "VOKAL ACID");

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
                                            : juce::String ("nema tona");
    g.drawText (noteText, display.removeFromBottom (40.0f), juce::Justification::centred);

    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText ("Nota > samoglasnik", juce::Rectangle<float> (266.0f, 30.0f, 400.0f, 16.0f), juce::Justification::left);
    g.drawFittedText (juce::String::fromUTF8 (
                          "Svaka nota koju linija svira dobija svoj samoglasnik, a Glide određuje koliko brzo "
                          "zvuk klizi s jednog na drugi. Akcent (nagli skok jačine) na tren pojača rezonancu. "
                          "Robot zaključava formante na harmonike tona, pa zvuk postaje metalan."),
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
    drawPanel (g, getLocalBounds().toFloat(), colours::amber, "POLIMETAR GATE");
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
    const float bar = (float) proc.meters.frazaPosition.load() * (float) fp.bars + 1.0f;
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("takt " + juce::String ((int) bar) + " / " + juce::String (fp.bars), r.withHeight (14.0f),
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
    drop.setTooltip ("Gotov build-up kroz zadnja 4 takta fraze");
    drop.onClick = [this] { proc.applyDropPreset(); };
}

void FrazaStrip::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat(), colours::cyan, "FRAZA");
    g.setColour (colours::dim);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText ("Koliko kriva pomjera parametar", 572, 8, 420, 16, juce::Justification::left);
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
            g.drawText (juce::String::fromUTF8 ("već u slotu ") + juce::String (other + 1) + juce::String::fromUTF8 (", ovaj se preskače"),
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
      echoPage (p), vokalPage (p), gatePage (p), fraza (p),
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
    addAndMakeVisible (fraza);
    addAndMakeVisible (output);

    showModule (cards.front()->moduleType() > 0 ? cards.front()->moduleType() : (int) ModuleType::fraktalEcho);
    setResizable (true, true);
    setResizeLimits (1040, 700, 2080, 1400);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1040.0 / 700.0);
    setSize (1040, 700);
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
                               + (proc.meters.hostPlaying.load() ? "" : "  (interni sat)");
    g.drawText (tempo, 170, 20, 240, 20, juce::Justification::left);
    g.drawText (juce::String::fromUTF8 ("Klik na slot otvara modul. Isti modul radi samo u jednom slotu."),
                420, 20, 420, 20, juce::Justification::left);
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
    for (juce::Component* page : { (juce::Component*) &echoPage, (juce::Component*) &vokalPage, (juce::Component*) &gatePage })
        place (*page, { 16, 148, 1008, 316 });
    place (fraza, { 16, 476, 1008, 208 });
    place (output, { 880, 4, 144, 48 });
}

void OriginPsyEditor::timerCallback()
{
    if (vokalPage.isVisible()) vokalPage.repaint();
    if (gatePage.isVisible()) gatePage.repaint();
    if (echoPage.isVisible()) echoPage.repaint();
    fraza.view.repaint();
    for (auto& c : cards) c->repaint();
    repaint (getLocalBounds().withHeight (getHeight() * 56 / 700));
}
