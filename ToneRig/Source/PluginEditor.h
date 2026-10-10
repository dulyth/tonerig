#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace tonerig
{

//==============================================================================
class RigLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RigLookAndFeel();

    juce::Colour accent      { 0xffe2a33c };
    juce::Colour accentAlt   { 0xff47c2d6 };
    juce::Colour panelColour { 0xff23262c };
    juce::Colour panelEdge   { 0xff3a3f47 };
    juce::Colour textColour  { 0xffd7dbe0 };

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
};

//==============================================================================
/** A rack module: a titled panel with an optional on/off switch, combo boxes,
    rotary knobs and toggles. */
class RigModule : public juce::Component
{
public:
    RigModule (juce::AudioProcessorValueTreeState& stateToUse,
               const juce::String& moduleTitle,
               juce::Colour moduleAccent,
               const char* onParamId = nullptr);

    void addKnob   (const char* paramId, const juce::String& shortLabel);
    void addChoice (const char* paramId, const juce::String& shortLabel);
    void addToggle (const char* paramId, const juce::String& label);

    void setKnobColumns (int columns) { knobColumns = juce::jmax (1, columns); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label>  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    struct Choice
    {
        std::unique_ptr<juce::ComboBox> box;
        std::unique_ptr<juce::Label>    label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;
    };

    struct Toggle
    {
        std::unique_ptr<juce::ToggleButton> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attach;
    };

    juce::AudioProcessorValueTreeState& state;
    juce::String title;
    juce::Colour accent;

    std::vector<Knob>   knobs;
    std::vector<Choice> choices;
    std::vector<Toggle> toggles;

    std::unique_ptr<juce::ToggleButton> onButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttach;

    int knobColumns = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RigModule)
};

//==============================================================================
class ToneRigAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit ToneRigAudioProcessorEditor (ToneRigAudioProcessor&);
    ~ToneRigAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void applyPresetFromBox();
    void stepPreset (int delta);

    ToneRigAudioProcessor& processor;
    RigLookAndFeel rigLnf;

    std::unique_ptr<RigModule> gateModule, driveModule, ampModule, cabModule,
                               modModule, delayModule, reverbModule, eqModule,
                               outputModule;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };

    juce::Rectangle<int> logoArea, tunerArea, presetArea, meterArea;

    float inputLevel = 0.0f, outputLevel = 0.0f;
    float inputPeakHold = 0.0f, outputPeakHold = 0.0f;
    TunerReading tunerReading {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneRigAudioProcessorEditor)
};

} // namespace tonerig
