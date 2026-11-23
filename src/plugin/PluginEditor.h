#pragma once

#include "PluginProcessor.h"

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
} // namespace colours

class PsyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PsyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end,
                           juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

/** Rotary knob with its name under it, attached to a parameter. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& name,
          juce::Colour accent = colours::cyan, bool horizontal = false);
    void resized() override;

private:
    bool compact = false;
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
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
    juce::ToggleButton on { "Fraza" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    Choice bars, curve;
    Knob offset;
    std::vector<std::unique_ptr<Knob>> amounts;
    juce::TextButton drop { "DROP" };
};

class SlotCard : public juce::Component
{
public:
    SlotCard (OriginPsyProcessor&, int slot, std::function<void (int)> onSelect);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    int moduleType() const;
    bool selected = false;

private:
    OriginPsyProcessor& proc;
    int slot;
    std::function<void (int)> onSelect;
    juce::ComboBox module;
    juce::ToggleButton bypass { "ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> moduleAttachment;
};

} // namespace originpsy::ui

//==============================================================================
class OriginPsyEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OriginPsyEditor (OriginPsyProcessor&);
    ~OriginPsyEditor() override;

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
    originpsy::ui::FrazaStrip fraza;
    originpsy::ui::Knob output;
    int shownModule = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OriginPsyEditor)
};
