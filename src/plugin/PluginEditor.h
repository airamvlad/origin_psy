#pragma once

#include "PluginProcessor.h"
#include "Presets.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace originpsy::ui
{

namespace colours
{
inline const juce::Colour background { 0xff121019 };
inline const juce::Colour panel { 0xff1c1a27 };
inline const juce::Colour panelLight { 0xff272435 };
inline const juce::Colour outline { 0xff3a3650 };
inline const juce::Colour text { 0xffe8e6f0 };
inline const juce::Colour dim { 0xff8b87a0 };
inline const juce::Colour cyan { 0xff39e1d0 };
inline const juce::Colour magenta { 0xffe24bd8 };
inline const juce::Colour amber { 0xffffb347 };
inline const juce::Colour lime { 0xffa6ff4d };
inline const juce::Colour blue { 0xff5aa9ff };
inline const juce::Colour orange { 0xffff7a45 };
inline const juce::Colour violet { 0xffb57bff };
inline const juce::Colour rose { 0xffff5c93 };
} // namespace colours

class PsyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PsyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

juce::Colour sourceColour (ModSource);

/** Slider that also edits the modulation on its parameter:
    shift-drag sets the depth of the newest connection, right-click lists the
    connections (invert, remove). */
class ModSlider : public juce::Slider
{
public:
    std::function<int()> modIndex;            // modulatable index of the parameter, -1 = none
    std::function<OriginPsyProcessor*()> processor;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int depthSlot = -1;
    float depthStart = 0.0f;
};

/** Rotary knob with its name under it, attached to a parameter. Modulation
    sources can be dropped on it; rings show each connection. */
class Knob : public juce::Component, public juce::DragAndDropTarget
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& name,
          juce::Colour accent = colours::cyan, bool horizontal = false);
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

private:
    OriginPsyProcessor* findProcessor() const;
    int modIndex() const;

    juce::String paramId;
    bool compact = false, dragOver = false;
    mutable int cachedIndex = -2;
    ModSlider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** A modulation source you can drag onto any knob. Shows its live value. */
class SourceChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    SourceChip (OriginPsyProcessor& p, ModSource s) : proc (p), source (s) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

private:
    OriginPsyProcessor& proc;
    ModSource source;
};

/** Combo box with a small caption, attached to a choice parameter. */
class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& name);
    void resized() override;
    juce::ComboBox box;

private:
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

//==============================================================================
class EchoPage : public juce::Component
{
public:
    explicit EchoPage (OriginPsyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    OriginPsyProcessor& proc;
    Choice time, scale;
    Knob ratio, generations, spread, feedback, damp, width, mix;
};

class VokalPage : public juce::Component
{
public:
    explicit VokalPage (OriginPsyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    OriginPsyProcessor& proc;
    std::vector<std::unique_ptr<Choice>> vowels;
    Choice character;
    Knob glide, shift, resonance, accent, drive, mix;
};

class ZapPage : public juce::Component
{
public:
    explicit ZapPage (OriginPsyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    OriginPsyProcessor& proc;
    Choice scale, root, order, key;
    Knob threshold, octave, sweep, sweepTime, decay, resonance, width, mix;
    uint32_t lastHits = 0;
    float flash = 0.0f;
};

class OrbitPage : public juce::Component
{
public:
    explicit OrbitPage (OriginPsyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    OriginPsyProcessor& proc;
    Choice path, rate;
    Knob radius, doppler, tone, width, mix;
    std::array<juce::Point<float>, 24> trail {};
    int trailPos = 0;
};

/** A curve of evenly spaced points (0..1) that is drawn with the mouse.
    Shift-drag draws a straight line, right-click opens reset, smooth, flip,
    reverse and clear. Subclasses say where the points live and paint them. */
class CurveDrawer : public juce::Component, public juce::SettableTooltipClient
{
public:
    CurveDrawer();
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

protected:
    virtual int numPoints() const = 0;
    virtual float getPoint (int i) const = 0;
    virtual void setPoint (int i, float v) = 0;
    /** Called before the first edit: switch to the drawn curve, copying the shape shown. */
    virtual void beginDrawing() = 0;
    /** The rectangle the curve spans: x = 0..1 across, value 1 at the top. */
    virtual juce::Rectangle<float> curveArea() const = 0;
    /** What "Reset" gives. */
    virtual float defaultPoint (int i) const = 0;
    virtual void addMenuItems (juce::PopupMenu&) {}
    virtual void menuItemChosen (int) {}
    /** The host should know the project changed (the points are not parameters). */
    virtual void edited() = 0;

    juce::Path pointsPath (const juce::Rectangle<float>& area) const;

private:
    std::vector<float> dragStart;
    juce::Point<float> downPos, lastPos;
    float toIndex (float x) const;
    float toValue (float y) const;
    void drawSegment (juce::Point<float> from, juce::Point<float> to);
    void showMenu();
};

/** The phrase curve and where the song is in it. Drawing on it makes a
    Custom curve, starting from the shape shown. */
class PhraseView : public CurveDrawer
{
public:
    explicit PhraseView (OriginPsyProcessor& p);
    void paint (juce::Graphics&) override;

private:
    OriginPsyProcessor& proc;
    juce::Rectangle<float> plotArea() const;
    int numPoints() const override;
    float getPoint (int i) const override;
    void setPoint (int i, float v) override;
    void beginDrawing() override;
    juce::Rectangle<float> curveArea() const override;
    float defaultPoint (int i) const override;
    void edited() override;
};

/** Tape speed over the slow down. Drawing on it switches Tape Stop to the
    drawn curve; the spin-up plays it backwards. */
class TapeCurveView : public CurveDrawer
{
public:
    explicit TapeCurveView (OriginPsyProcessor& p);
    void paint (juce::Graphics&) override;

private:
    OriginPsyProcessor& proc;
    bool drawnOn() const;
    int numPoints() const override;
    float getPoint (int i) const override;
    void setPoint (int i, float v) override;
    void beginDrawing() override;
    juce::Rectangle<float> curveArea() const override;
    float defaultPoint (int i) const override;
    void addMenuItems (juce::PopupMenu&) override;
    void menuItemChosen (int) override;
    void edited() override;
};

class TapePage : public juce::Component
{
public:
    explicit TapePage (OriginPsyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setBrake (bool on);

    OriginPsyProcessor& proc;
    Choice mode, trigger, length;
    Knob curve, mix, drive, crush, filter;
    juce::TextButton brake { "BRAKE" };
    TapeCurveView curveView;
    juce::Rectangle<float> fxCaption;
    float reelAngle = 0.0f;
};

/** Glide shape: how the pitch travels from the start of a glide to its
    target. Drawing on it switches Glide to the drawn curve. */
class GlideCurveView : public CurveDrawer
{
public:
    explicit GlideCurveView (OriginPsyProcessor& p);
    void paint (juce::Graphics&) override;

private:
    OriginPsyProcessor& proc;
    bool drawnOn() const;
    int numPoints() const override;
    float getPoint (int i) const override;
    void setPoint (int i, float v) override;
    void beginDrawing() override;
    juce::Rectangle<float> curveArea() const override;
    float defaultPoint (int i) const override;
    void addMenuItems (juce::PopupMenu&) override;
    void menuItemChosen (int) override;
    void edited() override;
};

/** The Glide pattern: a live pitch trace, a row of Glide switches and a lane
    of step notes (drag up and down, double-click for 0). */
class GlideSteps : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit GlideSteps (OriginPsyProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    /** Adds the current pitch to the trace (called by the page's timer). */
    void tick();

private:
    juce::Rectangle<float> traceArea() const;
    juce::Rectangle<float> glideRow() const;
    juce::Rectangle<float> noteLane() const;
    int stepAt (float x) const;
    int noteAt (float y) const;
    void showMenu();
    void edited();

    OriginPsyProcessor& proc;
    std::array<float, 160> trace {};
    int tracePos = 0;
    enum Drag { none, glides, notes } drag = none;
    bool paintGlideOn = true;
    int lastDragStep = -1;
};

class GlidePage : public juce::Component
{
public:
    explicit GlidePage (OriginPsyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setDive (bool on);

    OriginPsyProcessor& proc;
    Choice mode, rate, dive, diveLength;
    Knob steps, interval, time, curve, grain, mix, diveDepth, diveCurve;
    juce::TextButton diveButton { "DIVE" };
    GlideSteps stepsView;
    GlideCurveView curveView;
};

class RatchetPage : public juce::Component
{
public:
    explicit RatchetPage (OriginPsyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> stepArea() const;

    OriginPsyProcessor& proc;
    Choice rate, interval, scale, mask;
    Knob steps, smear, tilt, mix;
};

class GateRing : public juce::Component
{
public:
    GateRing (OriginPsyProcessor&, int band);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    OriginPsyProcessor& proc;
    int band;
};

class GatePage : public juce::Component
{
public:
    explicit GatePage (OriginPsyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Column
    {
        Column (OriginPsyProcessor&, int band);
        std::unique_ptr<GateRing> ring;
        std::unique_ptr<Choice> mode, rate;
        std::unique_ptr<Knob> steps, hits, rotate, depth, length, attack, release;
    };

    OriginPsyProcessor& proc;
    std::vector<std::unique_ptr<Column>> columns;
    Knob low, high, mix;
};

class FrazaStrip : public juce::Component
{
public:
    explicit FrazaStrip (OriginPsyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    PhraseView view;

private:
    OriginPsyProcessor& proc;
    juce::ToggleButton on { "Phrase" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    Choice bars, curve;
    Knob offset;
    std::vector<std::unique_ptr<Knob>> amounts;
    juce::TextButton drop { "DROP" };
};

class ModStrip : public juce::Component
{
public:
    explicit ModStrip (OriginPsyProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    OriginPsyProcessor& proc;
    std::vector<std::unique_ptr<SourceChip>> chips;
    std::vector<std::unique_ptr<Choice>> lfoControls;
    std::vector<std::unique_ptr<Knob>> macros;
};

class PresetBar : public juce::Component
{
public:
    explicit PresetBar (OriginPsyProcessor&);
    void resized() override;
    void refresh();
    /** Shows the processor's current preset name (after a host recall, for example). */
    void syncName();
    juce::String shownName;

private:
    void loadIndex (int index);
    void askToSave();

    OriginPsyProcessor& proc;
    originpsy::PresetManager presets;
    std::vector<originpsy::PresetManager::Entry> entries;
    juce::ComboBox box;
    juce::TextButton prev { "<" }, next { ">" }, save { "Save" }, randomize { "Randomize" }, mutate { "Mutate" };
    juce::Random rng;
};

class SlotCard : public juce::Component, public juce::DragAndDropTarget
{
public:
    SlotCard (OriginPsyProcessor&, int slot, std::function<void (int)> onSelect);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    // Drag a card onto another to swap the two slots.
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    int moduleType() const;
    bool selected = false;

private:
    OriginPsyProcessor& proc;
    int slot;
    std::function<void (int)> onSelect;
    juce::ComboBox module;
    juce::ToggleButton bypass { "ON" }, solo { "S" };
    juce::Slider mix;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> moduleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    bool dropHover = false;
};

} // namespace originpsy::ui

//==============================================================================
class OriginPsyEditor : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer,
                        private juce::Timer
{
public:
    explicit OriginPsyEditor (OriginPsyProcessor&);
    ~OriginPsyEditor() override;
    OriginPsyProcessor& getProcessor() noexcept { return proc; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showModule (int moduleType);

    OriginPsyProcessor& proc;
    originpsy::ui::PsyLookAndFeel lnf;
    std::vector<std::unique_ptr<originpsy::ui::SlotCard>> cards;
    originpsy::ui::EchoPage echoPage;
    originpsy::ui::VokalPage vokalPage;
    originpsy::ui::GatePage gatePage;
    originpsy::ui::ZapPage zapPage;
    originpsy::ui::OrbitPage orbitPage;
    originpsy::ui::TapePage tapePage;
    originpsy::ui::RatchetPage ratchetPage;
    originpsy::ui::GlidePage glidePage;
    originpsy::ui::FrazaStrip fraza;
    originpsy::ui::ModStrip modStrip;
    originpsy::ui::PresetBar presetBar;
    originpsy::ui::Knob output, mix;
    juce::ToggleButton limit { "LIMIT" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limitAttachment;
    juce::TooltipWindow tooltips { this, 700 };
    int shownModule = 1;
    uint32_t lastMidiEvents = 0;
    float midiFlash = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OriginPsyEditor)
};
