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
    Knob curve, mix;
    juce::TextButton brake { "BRAKE" };
    float reelAngle = 0.0f;
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

class PhraseView : public juce::Component
{
public:
    explicit PhraseView (OriginPsyProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;

private:
    OriginPsyProcessor& proc;
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
    originpsy::ui::FrazaStrip fraza;
    originpsy::ui::ModStrip modStrip;
    originpsy::ui::PresetBar presetBar;
    originpsy::ui::Knob output;
    juce::ToggleButton limit { "LIMIT" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limitAttachment;
    juce::TooltipWindow tooltips { this, 700 };
    int shownModule = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OriginPsyEditor)
};
