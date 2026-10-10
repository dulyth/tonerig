#include "PluginEditor.h"

#include <cmath>

namespace tonerig
{

namespace
{
    const juce::Colour kBackgroundTop    { 0xff14161a };
    const juce::Colour kBackgroundBottom { 0xff1c1f25 };
    const juce::Colour kHeaderColour     { 0xff101216 };
    const juce::Colour kTextColour       { 0xffd7dbe0 };
    const juce::Colour kDimTextColour    { 0xff7f868f };
    const juce::Colour kKnobBody         { 0xff2b2f36 };
    const juce::Colour kKnobEdge         { 0xff767d87 };

    juce::Font boldFont (float height)
    {
        return juce::Font (juce::FontOptions (height)).boldened();
    }
}

//==============================================================================
RigLookAndFeel::RigLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, kBackgroundBottom);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1a1d22));
    setColour (juce::ComboBox::textColourId, kTextColour);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::arrowColourId, accent);

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1f25));
    setColour (juce::PopupMenu::textColourId, kTextColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setColour (juce::Slider::textBoxTextColourId, textColour);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff191c21));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff2f343b));
    setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));

    setColour (juce::Label::textColourId, textColour);

    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff262a30));
    setColour (juce::TextButton::textColourOffId, textColour);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

void RigLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float rotaryStartAngle,
                                       float rotaryEndAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto arcRadius = radius - 3.0f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff14161a));
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (std::abs (angle - rotaryStartAngle) > 1.0e-4f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, angle, true);
        g.setColour (accent);
        g.strokePath (value, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    const auto bodyRadius = arcRadius - 4.0f;
    g.setColour (kKnobBody);
    g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour (kKnobEdge.withAlpha (0.7f));
    g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.2f);

    const juce::Point<float> tip (centre.x + std::sin (angle) * (bodyRadius - 2.0f),
                                  centre.y - std::cos (angle) * (bodyRadius - 2.0f));
    g.setColour (accent.brighter (0.35f));
    g.drawLine (centre.x, centre.y, tip.x, tip.y, 2.2f);
}

void RigLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                       bool shouldDrawButtonAsHighlighted, bool)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = button.getToggleState();

    const auto ledSize = juce::jmin (bounds.getHeight() - 4.0f, 10.0f);
    juce::Rectangle<float> led (bounds.getX() + 2.0f,
                               bounds.getCentreY() - ledSize * 0.5f,
                               ledSize, ledSize);

    g.setColour (juce::Colour (0xff101216));
    g.fillEllipse (led);

    juce::Colour ledColour = on ? accent : juce::Colour (0xff3a3f47);
    if (on)
    {
        g.setColour (ledColour.withAlpha (0.28f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (ledColour);
    g.fillEllipse (led.reduced (1.5f));

    if (button.getButtonText().isNotEmpty())
    {
        g.setColour (on ? textColour : kDimTextColour.brighter (shouldDrawButtonAsHighlighted ? 0.3f : 0.0f));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (button.getButtonText(),
                    bounds.withTrimmedLeft (ledSize + 8.0f),
                    juce::Justification::centredLeft, true);
    }
}

void RigLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                   int, int, int, int, juce::ComboBox&)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);
    g.setColour (juce::Colour (0xff1a1d22));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (panelEdge);
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    juce::Path arrow;
    const auto cx = (float) width - 12.0f;
    const auto cy = (float) height * 0.5f;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 3.0f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (accent);
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

juce::Font RigLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (12.0f));
}

juce::Font RigLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::Font (juce::FontOptions (11.0f));
}

//==============================================================================
RigModule::RigModule (juce::AudioProcessorValueTreeState& stateToUse,
                      const juce::String& moduleTitle,
                      juce::Colour moduleAccent,
                      const char* onParamId)
    : state (stateToUse), title (moduleTitle), accent (moduleAccent)
{
    if (onParamId != nullptr)
    {
        onButton = std::make_unique<juce::ToggleButton>();
        addAndMakeVisible (*onButton);
        onAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, onParamId, *onButton);
    }
}

void RigModule::addKnob (const char* paramId, const juce::String& shortLabel)
{
    knobs.emplace_back();
    auto& k = knobs.back();

    k.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                               juce::Slider::TextBoxBelow);
    k.slider->setTextBoxIsEditable (false);
    k.slider->setDoubleClickReturnValue (true, 0.0);
    k.slider->setColour (juce::Slider::textBoxTextColourId, kTextColour);
    k.slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff191c21));
    k.slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff2f343b));
    addAndMakeVisible (*k.slider);

    k.label = std::make_unique<juce::Label> (juce::String(), shortLabel);
    k.label->setJustificationType (juce::Justification::centred);
    k.label->setColour (juce::Label::textColourId, kDimTextColour);
    k.label->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (*k.label);

    k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, paramId, *k.slider);
}

void RigModule::addChoice (const char* paramId, const juce::String& shortLabel)
{
    choices.emplace_back();
    auto& c = choices.back();

    c.label = std::make_unique<juce::Label> (juce::String(), shortLabel);
    c.label->setJustificationType (juce::Justification::centredLeft);
    c.label->setColour (juce::Label::textColourId, kDimTextColour);
    c.label->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (*c.label);

    c.box = std::make_unique<juce::ComboBox>();
    addAndMakeVisible (*c.box);

    c.attach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, paramId, *c.box);
}

void RigModule::addToggle (const char* paramId, const juce::String& label)
{
    toggles.emplace_back();
    auto& t = toggles.back();

    t.button = std::make_unique<juce::ToggleButton> (label);
    addAndMakeVisible (*t.button);

    t.attach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, paramId, *t.button);
}

void RigModule::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff23262c));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (juce::Colour (0xff3a3f47));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    g.setColour (accent.withAlpha (0.9f));
    g.fillRect (bounds.getX() + 8.0f, bounds.getY() + 4.0f, juce::jmax (18.0f, bounds.getWidth() - 16.0f), 2.0f);

    auto titleArea = getLocalBounds().reduced (8, 6).removeFromTop (18);
    if (onButton != nullptr)
        titleArea.removeFromRight (46);

    g.setColour (kTextColour);
    g.setFont (boldFont (12.0f));
    g.drawText (title.toUpperCase(), titleArea, juce::Justification::centredLeft, true);
}

void RigModule::resized()
{
    auto r = getLocalBounds().reduced (8, 6);
    auto header = r.removeFromTop (18);

    if (onButton != nullptr)
        onButton->setBounds (header.removeFromRight (44).reduced (0, 1));

    r.removeFromTop (4);

    for (auto& c : choices)
    {
        auto row = r.removeFromTop (20);
        if (c.label != nullptr)
            c.label->setBounds (row.removeFromLeft (40));
        if (c.box != nullptr)
            c.box->setBounds (row.reduced (0, 1));
        r.removeFromTop (4);
    }

    for (auto& t : toggles)
    {
        auto row = r.removeFromBottom (22);
        if (t.button != nullptr)
            t.button->setBounds (row.reduced (1, 1));
        r.removeFromBottom (2);
    }

    if (knobs.empty() || r.getHeight() <= 0 || r.getWidth() <= 0)
        return;

    const int n = (int) knobs.size();
    const int cols = juce::jmin (knobColumns, n);
    const int rows = (n + cols - 1) / cols;
    const int cellW = r.getWidth() / cols;
    const int cellH = r.getHeight() / rows;

    for (int i = 0; i < n; ++i)
    {
        const int cx = i % cols;
        const int cy = i / cols;

        juce::Rectangle<int> cell (r.getX() + cx * cellW, r.getY() + cy * cellH, cellW, cellH);
        cell = cell.reduced (2, 1);

        auto labelRow = cell.removeFromBottom (juce::jmin (13, cell.getHeight() / 3));
        if (knobs[(size_t) i].label != nullptr)
            knobs[(size_t) i].label->setBounds (labelRow);
        if (knobs[(size_t) i].slider != nullptr)
            knobs[(size_t) i].slider->setBounds (cell);
    }
}

//==============================================================================
ToneRigAudioProcessorEditor::ToneRigAudioProcessorEditor (ToneRigAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&rigLnf);
    setOpaque (true);

    auto makeModule = [this] (const juce::String& title, juce::Colour accent, const char* onId)
    {
        return std::make_unique<RigModule> (processor.apvts, title, accent, onId);
    };

    gateModule = makeModule ("Input / Gate", rigLnf.accentAlt, pid::gateOn);
    gateModule->addKnob (pid::inputGain, "In");
    gateModule->addKnob (pid::gateThresh, "Thresh");
    gateModule->addKnob (pid::gateRelease, "Release");
    gateModule->setKnobColumns (2);

    driveModule = makeModule ("Drive", rigLnf.accent, pid::odOn);
    driveModule->addKnob (pid::odDrive, "Drive");
    driveModule->addKnob (pid::odTone, "Tone");
    driveModule->addKnob (pid::odLevel, "Level");
    driveModule->setKnobColumns (3);

    ampModule = makeModule ("Amp Head", rigLnf.accent, pid::ampOn);
    ampModule->addChoice (pid::ampModel, "Model");
    ampModule->addKnob (pid::ampGain, "Gain");
    ampModule->addKnob (pid::ampBass, "Bass");
    ampModule->addKnob (pid::ampMid, "Mid");
    ampModule->addKnob (pid::ampTreble, "Treble");
    ampModule->addKnob (pid::ampPresence, "Presence");
    ampModule->addKnob (pid::ampMaster, "Master");
    ampModule->setKnobColumns (3);

    cabModule = makeModule ("Cabinet", rigLnf.accent, pid::cabOn);
    cabModule->addChoice (pid::cabModel, "Cab");
    cabModule->addKnob (pid::cabLevel, "Level");
    cabModule->setKnobColumns (1);

    modModule = makeModule ("Modulation", rigLnf.accentAlt, pid::modOn);
    modModule->addChoice (pid::modType, "Type");
    modModule->addKnob (pid::modRate, "Rate");
    modModule->addKnob (pid::modDepth, "Depth");
    modModule->addKnob (pid::modMix, "Mix");
    modModule->setKnobColumns (3);

    delayModule = makeModule ("Delay", rigLnf.accent, pid::delayOn);
    delayModule->addKnob (pid::delayTime, "Time");
    delayModule->addKnob (pid::delayFeedback, "Feedback");
    delayModule->addKnob (pid::delayMix, "Mix");
    delayModule->setKnobColumns (2);
    delayModule->addToggle (pid::delayPingPong, "Ping Pong");

    reverbModule = makeModule ("Reverb", rigLnf.accentAlt, pid::revOn);
    reverbModule->addKnob (pid::revSize, "Size");
    reverbModule->addKnob (pid::revDamp, "Damp");
    reverbModule->addKnob (pid::revWidth, "Width");
    reverbModule->addKnob (pid::revMix, "Mix");
    reverbModule->setKnobColumns (2);

    eqModule = makeModule ("EQ", rigLnf.accent, pid::eqOn);
    eqModule->addKnob (pid::eqLow, "Low");
    eqModule->addKnob (pid::eqMid, "Mid");
    eqModule->addKnob (pid::eqHigh, "High");
    eqModule->setKnobColumns (3);

    outputModule = makeModule ("Output", rigLnf.accentAlt, nullptr);
    outputModule->addKnob (pid::outputGain, "Out");
    outputModule->setKnobColumns (1);
    outputModule->addToggle (pid::limiterOn, "Limiter");

    for (auto* m : { gateModule.get(), driveModule.get(), ampModule.get(), cabModule.get(),
                     modModule.get(), delayModule.get(), reverbModule.get(), eqModule.get(),
                     outputModule.get() })
        addAndMakeVisible (*m);

    //== header ================================================================
    const auto& presets = getFactoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (presets[(size_t) i].name, i + 1);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { applyPresetFromBox(); };
    addAndMakeVisible (presetBox);

    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    setResizable (true, true);
    setResizeLimits (860, 520, 2200, 1500);
    setSize (1000, 620);

    startTimerHz (24);
}

ToneRigAudioProcessorEditor::~ToneRigAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ToneRigAudioProcessorEditor::applyPresetFromBox()
{
    const int index = presetBox.getSelectedId() - 1;
    if (index >= 0)
        processor.setCurrentProgram (index);
}

void ToneRigAudioProcessorEditor::stepPreset (int delta)
{
    const int count = juce::jmax (1, processor.getNumFactoryPresets());
    int index = (processor.getCurrentProgram() + delta + count) % count;
    presetBox.setSelectedId (index + 1, juce::dontSendNotification);
    processor.setCurrentProgram (index);
}

void ToneRigAudioProcessorEditor::timerCallback()
{
    inputLevel  = processor.chain.getInputLevel();
    outputLevel = processor.chain.getOutputLevel();

    const float inDb  = juce::Decibels::gainToDecibels (inputLevel, -72.0f);
    const float outDb = juce::Decibels::gainToDecibels (outputLevel, -72.0f);
    inputPeakHold  = juce::jmax (inDb,  inputPeakHold  - 1.2f);
    outputPeakHold = juce::jmax (outDb, outputPeakHold - 1.2f);

    if (processor.chain.tunerWantsAnalysis())
        processor.chain.analyseTuner();
    tunerReading = processor.chain.getTunerReading();

    if (presetBox.getSelectedId() != processor.getCurrentProgram() + 1)
        presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);

    repaint();
}

void ToneRigAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto header = r.removeFromTop (56);

    logoArea = header.removeFromLeft (176);
    tunerArea = header.removeFromLeft (250);
    meterArea = header.removeFromRight (juce::jmax (180, header.getWidth() - 380));
    presetArea = header;

    auto presetRow = presetArea.withSizeKeepingCentre (presetArea.getWidth(), 26);
    prevButton.setBounds (presetRow.removeFromLeft (28));
    presetRow.removeFromLeft (4);
    nextButton.setBounds (presetRow.removeFromRight (28));
    presetRow.removeFromRight (4);
    presetBox.setBounds (presetRow);

    r.removeFromTop (8);

    auto row1 = r.removeFromTop ((int) (r.getHeight() * 0.5f));
    juce::FlexBox fb1;
    fb1.flexDirection = juce::FlexBox::Direction::row;
    fb1.items.add (juce::FlexItem (*gateModule).withFlex (1.05f).withMargin (4.0f));
    fb1.items.add (juce::FlexItem (*driveModule).withFlex (0.95f).withMargin (4.0f));
    fb1.items.add (juce::FlexItem (*ampModule).withFlex (2.20f).withMargin (4.0f));
    fb1.items.add (juce::FlexItem (*cabModule).withFlex (1.05f).withMargin (4.0f));
    fb1.performLayout (row1);

    auto row2 = r;
    juce::FlexBox fb2;
    fb2.flexDirection = juce::FlexBox::Direction::row;
    fb2.items.add (juce::FlexItem (*modModule).withFlex (1.25f).withMargin (4.0f));
    fb2.items.add (juce::FlexItem (*delayModule).withFlex (1.45f).withMargin (4.0f));
    fb2.items.add (juce::FlexItem (*reverbModule).withFlex (1.35f).withMargin (4.0f));
    fb2.items.add (juce::FlexItem (*eqModule).withFlex (1.15f).withMargin (4.0f));
    fb2.items.add (juce::FlexItem (*outputModule).withFlex (0.95f).withMargin (4.0f));
    fb2.performLayout (row2);
}

void ToneRigAudioProcessorEditor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (kBackgroundTop, 0.0f, 0.0f,
                             kBackgroundBottom, 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();

    //== header ===============================================================
    g.setColour (kHeaderColour);
    g.fillRect (getLocalBounds().removeFromTop (76));

    g.setColour (rigLnf.accent);
    g.fillRect (getLocalBounds().removeFromTop (76).removeFromBottom (2));

    auto logo = logoArea;
    g.setColour (kTextColour);
    g.setFont (boldFont (24.0f));
    g.drawText ("ToneRig", logo.removeFromTop (30), juce::Justification::centredLeft, false);

    g.setColour (rigLnf.accent.withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawText ("GUITAR RIG SUITE  -  VST3", logo, juce::Justification::centredLeft, false);

    //== tuner ================================================================
    {
        auto t = tunerArea.reduced (6, 8);
        g.setColour (juce::Colour (0xff191c21));
        g.fillRoundedRectangle (t.toFloat(), 5.0f);
        g.setColour (juce::Colour (0xff3a3f47));
        g.drawRoundedRectangle (t.toFloat().reduced (0.5f), 5.0f, 1.0f);

        auto inner = t.reduced (10, 6);
        auto labelRow = inner.removeFromTop (11);

        g.setColour (juce::Colour (0xff8d949d));
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        g.drawText ("TUNER", labelRow, juce::Justification::centredLeft, false);

        const bool havePitch = tunerReading.midiNote >= 0;
        auto noteArea = inner.removeFromLeft (74);

        g.setColour (havePitch ? rigLnf.accent : juce::Colour (0xff5a6069));
        g.setFont (boldFont (26.0f));
        const auto noteText = havePitch
            ? juce::MidiMessage::getMidiNoteName (tunerReading.midiNote, true, true, 3)
            : juce::String ("--");
        g.drawText (noteText, noteArea, juce::Justification::centredLeft, false);

        auto bar = inner;
        const float cents = havePitch ? juce::jlimit (-50.0f, 50.0f, tunerReading.cents) : 0.0f;
        const float centreX = bar.getCentreX();

        g.setColour (juce::Colour (0xff2b3038));
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) bar.getX(),
                                                       (float) bar.getCentreY() - 1.5f,
                                                       (float) bar.getWidth(), 3.0f), 1.5f);

        const int marks = 10;
        for (int i = 0; i <= marks; ++i)
        {
            const float px = bar.getX() + (float) i / (float) marks * (float) bar.getWidth();
            g.setColour (i == marks / 2 ? rigLnf.accent : juce::Colour (0xff4a505a));
            g.fillRect (px - 0.5f, (float) bar.getCentreY() - 5.0f, 1.0f, 10.0f);
        }

        if (havePitch)
        {
            const float px = centreX + cents / 50.0f * (float) bar.getWidth() * 0.5f;
            const bool inTune = std::abs (cents) < 5.0f;
            g.setColour (inTune ? juce::Colour (0xff62d67a) : rigLnf.accent);
            g.fillRect (px - 1.5f, (float) bar.getCentreY() - 11.0f, 3.0f, 22.0f);
        }
    }

    //== meters ===============================================================
    {
        auto m = meterArea.reduced (6, 10);
        auto drawMeter = [&g] (juce::Rectangle<int> area, const juce::String& label,
                               float db, float peakDb, juce::Colour colour)
        {
            auto row = area;
            auto labelArea = row.removeFromLeft (28);
            g.setColour (juce::Colour (0xff8d949d));
            g.setFont (juce::Font (juce::FontOptions (9.5f)));
            g.drawText (label, labelArea, juce::Justification::centredLeft, false);

            g.setColour (juce::Colour (0xff13161a));
            g.fillRoundedRectangle (row.toFloat(), 2.5f);

            const float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
            auto filled = row.toFloat();
            filled.setWidth (filled.getWidth() * norm);

            juce::ColourGradient grad (juce::Colour (0xff3f7f4c), (float) row.getX(), 0.0f,
                                       juce::Colour (0xffd9b23c), (float) row.getRight(), 0.0f, false);
            grad.addColour (0.82, juce::Colour (0xffd95c3c));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (filled, 2.5f);

            const float peakNorm = juce::jlimit (0.0f, 1.0f, (peakDb + 60.0f) / 60.0f);
            const float px = (float) row.getX() + peakNorm * (float) row.getWidth();
            g.setColour (colour);
            g.fillRect (px - 1.0f, (float) row.getY(), 2.0f, (float) row.getHeight());
        };

        auto top = m.removeFromTop (m.getHeight() / 2 - 2);
        m.removeFromTop (4);
        drawMeter (top, "IN",  juce::Decibels::gainToDecibels (inputLevel, -72.0f),
                   inputPeakHold, juce::Colours::white.withAlpha (0.8f));
        drawMeter (m, "OUT", juce::Decibels::gainToDecibels (outputLevel, -72.0f),
                   outputPeakHold, juce::Colours::white.withAlpha (0.8f));
    }
}

} // namespace tonerig
