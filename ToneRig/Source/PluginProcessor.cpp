#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace tonerig
{

//==============================================================================
namespace
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;

    const juce::StringArray kAmpModels { "Clean", "Crunch", "Lead", "Fuzz", "Acoustic" };
    const juce::StringArray kCabModels { "4x12 V30", "4x12 Green", "2x12 Open", "1x12 Combo", "Direct" };
    const juce::StringArray kModTypes  { "Chorus", "Flanger", "Phaser" };

    juce::NormalisableRange<float> linear (float lo, float hi, float step)
    {
        return { lo, hi, step };
    }

    juce::NormalisableRange<float> skewed (float lo, float hi, float step, float skew)
    {
        return { lo, hi, step, skew };
    }
}

//==============================================================================
const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets
    {
        { "Init (Default)", {} },

        { "Clean Sparkle",
          { { pid::ampOn, 1 }, { pid::ampModel, 0 }, { pid::ampGain, 0.22f },
            { pid::ampBass, 0.45f }, { pid::ampMid, 0.42f }, { pid::ampTreble, 0.62f },
            { pid::ampPresence, 0.62f }, { pid::ampMaster, 0.58f },
            { pid::cabOn, 1 }, { pid::cabModel, 3 }, { pid::cabLevel, 0.55f },
            { pid::revOn, 1 }, { pid::revSize, 0.42f }, { pid::revMix, 0.18f },
            { pid::eqOn, 1 }, { pid::eqLow, 1.5f }, { pid::eqHigh, 2.5f },
            { pid::gateOn, 0 }, { pid::odOn, 0 }, { pid::modOn, 0 }, { pid::delayOn, 0 } } },

        { "British Crunch",
          { { pid::gateOn, 1 }, { pid::gateThresh, -55.0f },
            { pid::odOn, 1 }, { pid::odDrive, 0.12f }, { pid::odTone, 0.55f }, { pid::odLevel, 0.75f },
            { pid::ampOn, 1 }, { pid::ampModel, 1 }, { pid::ampGain, 0.58f },
            { pid::ampBass, 0.42f }, { pid::ampMid, 0.62f }, { pid::ampTreble, 0.56f },
            { pid::ampPresence, 0.55f }, { pid::ampMaster, 0.5f },
            { pid::cabOn, 1 }, { pid::cabModel, 1 }, { pid::cabLevel, 0.52f },
            { pid::revOn, 1 }, { pid::revSize, 0.35f }, { pid::revMix, 0.12f },
            { pid::modOn, 0 }, { pid::delayOn, 0 }, { pid::eqOn, 0 } } },

        { "High Gain Lead",
          { { pid::gateOn, 1 }, { pid::gateThresh, -50.0f }, { pid::gateRelease, 120.0f },
            { pid::odOn, 1 }, { pid::odDrive, 0.35f }, { pid::odTone, 0.6f }, { pid::odLevel, 0.6f },
            { pid::ampOn, 1 }, { pid::ampModel, 2 }, { pid::ampGain, 0.72f },
            { pid::ampBass, 0.5f }, { pid::ampMid, 0.38f }, { pid::ampTreble, 0.6f },
            { pid::ampPresence, 0.66f }, { pid::ampMaster, 0.5f },
            { pid::cabOn, 1 }, { pid::cabModel, 0 }, { pid::cabLevel, 0.55f },
            { pid::delayOn, 1 }, { pid::delayTime, 380.0f }, { pid::delayFeedback, 0.28f },
            { pid::delayMix, 0.16f }, { pid::delayPingPong, 0 },
            { pid::revOn, 1 }, { pid::revSize, 0.4f }, { pid::revMix, 0.1f },
            { pid::modOn, 0 }, { pid::eqOn, 0 } } },

        { "Fuzz Wall",
          { { pid::gateOn, 1 }, { pid::gateThresh, -52.0f },
            { pid::ampOn, 1 }, { pid::ampModel, 3 }, { pid::ampGain, 0.68f },
            { pid::ampBass, 0.55f }, { pid::ampMid, 0.45f }, { pid::ampTreble, 0.5f },
            { pid::ampMaster, 0.45f },
            { pid::cabOn, 1 }, { pid::cabModel, 0 }, { pid::cabLevel, 0.5f },
            { pid::revOn, 1 }, { pid::revSize, 0.3f }, { pid::revMix, 0.1f },
            { pid::odOn, 0 }, { pid::modOn, 0 }, { pid::delayOn, 0 }, { pid::eqOn, 0 } } },

        { "Ambient Clean",
          { { pid::gateOn, 1 }, { pid::gateThresh, -62.0f },
            { pid::ampOn, 1 }, { pid::ampModel, 0 }, { pid::ampGain, 0.3f },
            { pid::ampBass, 0.5f }, { pid::ampMid, 0.45f }, { pid::ampTreble, 0.6f },
            { pid::cabOn, 1 }, { pid::cabModel, 3 }, { pid::cabLevel, 0.5f },
            { pid::modOn, 1 }, { pid::modType, 0 }, { pid::modRate, 0.22f },
            { pid::modDepth, 0.5f }, { pid::modMix, 0.45f },
            { pid::delayOn, 1 }, { pid::delayTime, 470.0f }, { pid::delayFeedback, 0.42f },
            { pid::delayMix, 0.3f }, { pid::delayPingPong, 1 },
            { pid::revOn, 1 }, { pid::revSize, 0.78f }, { pid::revDamp, 0.45f },
            { pid::revWidth, 1.0f }, { pid::revMix, 0.34f },
            { pid::odOn, 0 }, { pid::eqOn, 0 } } },

        { "Acoustic DI",
          { { pid::ampOn, 1 }, { pid::ampModel, 4 }, { pid::ampGain, 0.35f },
            { pid::ampBass, 0.5f }, { pid::ampMid, 0.4f }, { pid::ampTreble, 0.62f },
            { pid::ampMaster, 0.6f },
            { pid::cabOn, 1 }, { pid::cabModel, 4 }, { pid::cabLevel, 0.5f },
            { pid::eqOn, 1 }, { pid::eqLow, -1.5f }, { pid::eqMid, -2.0f }, { pid::eqHigh, 2.0f },
            { pid::revOn, 1 }, { pid::revSize, 0.35f }, { pid::revMix, 0.14f },
            { pid::gateOn, 0 }, { pid::odOn, 0 }, { pid::modOn, 0 }, { pid::delayOn, 0 } } },

        { "Ping Pong Lead",
          { { pid::gateOn, 1 }, { pid::gateThresh, -54.0f },
            { pid::odOn, 1 }, { pid::odDrive, 0.2f }, { pid::odLevel, 0.65f },
            { pid::ampOn, 1 }, { pid::ampModel, 2 }, { pid::ampGain, 0.62f },
            { pid::ampBass, 0.48f }, { pid::ampMid, 0.45f }, { pid::ampTreble, 0.58f },
            { pid::cabOn, 1 }, { pid::cabModel, 0 }, { pid::cabLevel, 0.52f },
            { pid::delayOn, 1 }, { pid::delayTime, 520.0f }, { pid::delayFeedback, 0.45f },
            { pid::delayMix, 0.32f }, { pid::delayPingPong, 1 },
            { pid::revOn, 1 }, { pid::revSize, 0.55f }, { pid::revMix, 0.18f },
            { pid::modOn, 0 }, { pid::eqOn, 0 } } },

        { "Scooped Metal",
          { { pid::gateOn, 1 }, { pid::gateThresh, -48.0f }, { pid::gateRelease, 90.0f },
            { pid::odOn, 1 }, { pid::odDrive, 0.5f }, { pid::odTone, 0.7f }, { pid::odLevel, 0.55f },
            { pid::ampOn, 1 }, { pid::ampModel, 2 }, { pid::ampGain, 0.85f },
            { pid::ampBass, 0.62f }, { pid::ampMid, 0.22f }, { pid::ampTreble, 0.66f },
            { pid::ampPresence, 0.7f }, { pid::ampMaster, 0.45f },
            { pid::cabOn, 1 }, { pid::cabModel, 0 }, { pid::cabLevel, 0.56f },
            { pid::eqOn, 1 }, { pid::eqLow, 2.0f }, { pid::eqMid, -3.0f }, { pid::eqHigh, 1.5f },
            { pid::revOn, 1 }, { pid::revSize, 0.3f }, { pid::revMix, 0.08f },
            { pid::modOn, 0 }, { pid::delayOn, 0 } } },
    };

    return presets;
}

//==============================================================================
ToneRigAudioProcessor::ToneRigAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ToneRigState", createParameterLayout())
{
    currentProgramName = getFactoryPresets().front().name;
}

juce::AudioProcessorValueTreeState::ParameterLayout ToneRigAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const juce::ParameterID ver { "", 1 };   // placeholder, real ids built inline

    auto addFloat = [&layout] (const char* id, const juce::String& name,
                               juce::NormalisableRange<float> range, float def,
                               const juce::String& suffix = {})
    {
        layout.add (std::make_unique<APF> (juce::ParameterID { id, 1 }, name, range, def,
                                           juce::AudioParameterFloatAttributes().withLabel (suffix)));
    };

    auto addBool = [&layout] (const char* id, const juce::String& name, bool def)
    {
        layout.add (std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def,
                                           juce::AudioParameterBoolAttributes()));
    };

    auto addChoice = [&layout] (const char* id, const juce::String& name,
                                const juce::StringArray& choices, int def)
    {
        layout.add (std::make_unique<APC> (juce::ParameterID { id, 1 }, name, choices, def));
    };

    juce::ignoreUnused (ver);

    layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
        "input", "Input", "|",
        std::make_unique<APF> (juce::ParameterID { pid::inputGain, 1 }, "Input Gain",
                               linear (-24.0f, 24.0f, 0.1f), 0.0f,
                               juce::AudioParameterFloatAttributes().withLabel ("dB"))));

    layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
        "gate", "Noise Gate", "|",
        std::make_unique<APB> (juce::ParameterID { pid::gateOn, 1 }, "Gate On", false),
        std::make_unique<APF> (juce::ParameterID { pid::gateThresh, 1 }, "Gate Threshold",
                               linear (-80.0f, 0.0f, 0.1f), -60.0f,
                               juce::AudioParameterFloatAttributes().withLabel ("dB")),
        std::make_unique<APF> (juce::ParameterID { pid::gateRelease, 1 }, "Gate Release",
                               skewed (10.0f, 2000.0f, 1.0f, 0.35f), 150.0f,
                               juce::AudioParameterFloatAttributes().withLabel ("ms"))));

    layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
        "drive", "Drive", "|",
        std::make_unique<APB> (juce::ParameterID { pid::odOn, 1 }, "Drive On", false),
        std::make_unique<APF> (juce::ParameterID { pid::odDrive, 1 }, "Drive", linear (0.0f, 1.0f, 0.001f), 0.3f),
        std::make_unique<APF> (juce::ParameterID { pid::odTone, 1 }, "Tone", linear (0.0f, 1.0f, 0.001f), 0.5f),
        std::make_unique<APF> (juce::ParameterID { pid::odLevel, 1 }, "Level", linear (0.0f, 1.0f, 0.001f), 0.5f)));

    layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
        "amp", "Amp Head", "|",
        std::make_unique<APB> (juce::ParameterID { pid::ampOn, 1 }, "Amp On", true),
        std::make_unique<APC> (juce::ParameterID { pid::ampModel, 1 }, "Amp Model", kAmpModels, 1),
        std::make_unique<APF> (juce::ParameterID { pid::ampGain, 1 }, "Gain", linear (0.0f, 1.0f, 0.001f), 0.5f),
        std::make_unique<APF> (juce::ParameterID { pid::ampBass, 1 }, "Bass", linear (0.0f, 1.0f, 0.001f), 0.5f),
        std::make_unique<APF> (juce::ParameterID { pid::ampMid, 1 }, "Mid", linear (0.0f, 1.0f, 0.001f), 0.5f),
        std::make_unique<APF> (juce::ParameterID { pid::ampTreble, 1 }, "Treble", linear (0.0f, 1.0f, 0.001f), 0.5f),
        std::make_unique<APF> (juce::ParameterID { pid::ampPresence, 1 }, "Presence", linear (0.0f, 1.0f, 0.001f), 0.5f),
        std::make_unique<APF> (juce::ParameterID { pid::ampMaster, 1 }, "Master", linear (0.0f, 1.0f, 0.001f), 0.5f)));

    layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
        "cab", "Cabinet", "|",
        std::make_unique<APB> (juce::ParameterID { pid::cabOn, 1 }, "Cab On", true),
        std::make_unique<APC> (juce::ParameterID { pid::cabModel, 1 }, "Cab Model", kCabModels, 0),
        std::make_unique<APF> (juce::ParameterID { pid::cabLevel, 1 }, "Cab Level", linear (0.0f, 1.0f, 0.001f), 0.5f)));

    layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
        "mod", "Modulation", "|",
        std::make_unique<APB> (juce::ParameterID { pid::modOn, 1 }, "Mod On", false),
        std::make_unique<APC> (juce::ParameterID { pid::modType, 1 }, "Mod Type", kModTypes, 0),
        std::make_unique<APF> (juce::ParameterID { pid::modRate, 1 }, "Rate", linear (0.0f, 1.0f, 0.001f), 0.3f),
        std::make_unique<APF> (juce::ParameterID { pid::modDepth, 1 }, "Depth", linear (0.0f, 1.0f, 0.001f), 0.4f),
        std::make_unique<APF> (juce::ParameterID { pid::modMix, 1 }, "Mix", linear (0.0f, 1.0f, 0.001f), 0.5f)));

    layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
        "delay", "Delay", "|",
        std::make_unique<APB> (juce::ParameterID { pid::delayOn, 1 }, "Delay On", false),
        std::make_unique<APF> (juce::ParameterID { pid::delayTime, 1 }, "Time",
                               skewed (5.0f, 1300.0f, 1.0f, 0.5f), 320.0f,
                               juce::AudioParameterFloatAttributes().withLabel ("ms")),
        std::make_unique<APF> (juce::ParameterID { pid::delayFeedback, 1 }, "Feedback", linear (0.0f, 0.95f, 0.001f), 0.35f),
        std::make_unique<APF> (juce::ParameterID { pid::delayMix, 1 }, "Mix", linear (0.0f, 1.0f, 0.001f), 0.25f),
        std::make_unique<APB> (juce::ParameterID { pid::delayPingPong, 1 }, "Ping Pong", false)));

    layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
        "reverb", "Reverb", "|",
        std::make_unique<APB> (juce::ParameterID { pid::revOn, 1 }, "Reverb On", false),
        std::make_unique<APF> (juce::ParameterID { pid::revSize, 1 }, "Size", linear (0.0f, 1.0f, 0.001f), 0.55f),
        std::make_unique<APF> (juce::ParameterID { pid::revDamp, 1 }, "Damping", linear (0.0f, 1.0f, 0.001f), 0.5f),
        std::make_unique<APF> (juce::ParameterID { pid::revWidth, 1 }, "Width", linear (0.0f, 1.0f, 0.001f), 1.0f),
        std::make_unique<APF> (juce::ParameterID { pid::revMix, 1 }, "Mix", linear (0.0f, 1.0f, 0.001f), 0.2f)));

    layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
        "eq", "EQ", "|",
        std::make_unique<APB> (juce::ParameterID { pid::eqOn, 1 }, "EQ On", false),
        std::make_unique<APF> (juce::ParameterID { pid::eqLow, 1 }, "Low", linear (-12.0f, 12.0f, 0.1f), 0.0f,
                               juce::AudioParameterFloatAttributes().withLabel ("dB")),
        std::make_unique<APF> (juce::ParameterID { pid::eqMid, 1 }, "Mid", linear (-12.0f, 12.0f, 0.1f), 0.0f,
                               juce::AudioParameterFloatAttributes().withLabel ("dB")),
        std::make_unique<APF> (juce::ParameterID { pid::eqHigh, 1 }, "High", linear (-12.0f, 12.0f, 0.1f), 0.0f,
                               juce::AudioParameterFloatAttributes().withLabel ("dB"))));

    layout.add (std::make_unique<juce::AudioProcessorParameterGroup> (
        "output", "Output", "|",
        std::make_unique<APF> (juce::ParameterID { pid::outputGain, 1 }, "Output Gain",
                               linear (-24.0f, 12.0f, 0.1f), 0.0f,
                               juce::AudioParameterFloatAttributes().withLabel ("dB")),
        std::make_unique<APB> (juce::ParameterID { pid::limiterOn, 1 }, "Limiter", true)));

    juce::ignoreUnused (addFloat, addBool, addChoice);
    return layout;
}

//==============================================================================
const juce::String ToneRigAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

float ToneRigAudioProcessor::valueOf (const char* id) const
{
    if (auto* v = apvts.getRawParameterValue (id))
        return v->load();
    return 0.0f;
}

RigParams ToneRigAudioProcessor::pullParams() const
{
    RigParams p;

    p.inputGainDb = valueOf (pid::inputGain);

    p.gateOn        = valueOf (pid::gateOn) > 0.5f;
    p.gateThreshDb  = valueOf (pid::gateThresh);
    p.gateReleaseMs = valueOf (pid::gateRelease);

    p.odOn    = valueOf (pid::odOn) > 0.5f;
    p.odDrive = valueOf (pid::odDrive);
    p.odTone  = valueOf (pid::odTone);
    p.odLevel = valueOf (pid::odLevel);

    p.ampOn       = valueOf (pid::ampOn) > 0.5f;
    p.ampModel    = juce::jlimit (0, numAmpModels - 1, (int) std::lround (valueOf (pid::ampModel)));
    p.ampGain     = valueOf (pid::ampGain);
    p.ampBass     = valueOf (pid::ampBass);
    p.ampMid      = valueOf (pid::ampMid);
    p.ampTreble   = valueOf (pid::ampTreble);
    p.ampPresence = valueOf (pid::ampPresence);
    p.ampMaster   = valueOf (pid::ampMaster);

    p.cabOn    = valueOf (pid::cabOn) > 0.5f;
    p.cabModel = juce::jlimit (0, numCabModels - 1, (int) std::lround (valueOf (pid::cabModel)));
    p.cabLevel = valueOf (pid::cabLevel);

    p.modOn    = valueOf (pid::modOn) > 0.5f;
    p.modType  = juce::jlimit (0, numModTypes - 1, (int) std::lround (valueOf (pid::modType)));
    p.modRate  = valueOf (pid::modRate);
    p.modDepth = valueOf (pid::modDepth);
    p.modMix   = valueOf (pid::modMix);

    p.delayOn       = valueOf (pid::delayOn) > 0.5f;
    p.delayTimeMs   = valueOf (pid::delayTime);
    p.delayFeedback = valueOf (pid::delayFeedback);
    p.delayMix      = valueOf (pid::delayMix);
    p.delayPingPong = valueOf (pid::delayPingPong) > 0.5f;

    p.revOn    = valueOf (pid::revOn) > 0.5f;
    p.revSize  = valueOf (pid::revSize);
    p.revDamp  = valueOf (pid::revDamp);
    p.revWidth = valueOf (pid::revWidth);
    p.revMix   = valueOf (pid::revMix);

    p.eqOn    = valueOf (pid::eqOn) > 0.5f;
    p.eqLowDb = valueOf (pid::eqLow);
    p.eqMidDb = valueOf (pid::eqMid);
    p.eqHighDb= valueOf (pid::eqHigh);

    p.outputGainDb = valueOf (pid::outputGain);
    p.limiterOn    = valueOf (pid::limiterOn) > 0.5f;

    return p;
}

//==============================================================================
void ToneRigAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    chain.prepare (sampleRate, samplesPerBlock, getTotalNumInputChannels());
}

void ToneRigAudioProcessor::releaseResources()
{
    chain.reset();
}

bool ToneRigAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled())
        return false;

    if (in.size() > 2 || out.size() > 2)
        return false;

    return true;
}

void ToneRigAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    chain.setParams (pullParams());
    chain.process (buffer);
}

//==============================================================================
juce::AudioProcessorEditor* ToneRigAudioProcessor::createEditor()
{
    return new ToneRigAudioProcessorEditor (*this);
}

//==============================================================================
int ToneRigAudioProcessor::getNumPrograms()
{
    return juce::jmax (1, getNumFactoryPresets());
}

const juce::String ToneRigAudioProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    if (juce::isPositiveAndBelow (index, (int) presets.size()))
        return presets[(size_t) index].name;
    return currentProgramName;
}

void ToneRigAudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;
    currentProgramName = presets[(size_t) index].name;
    loadPreset (index);
}

void ToneRigAudioProcessor::changeProgramName (int, const juce::String&) {}

void ToneRigAudioProcessor::loadPreset (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    if (index == 0)
    {
        resetToDefaultPatch();
    }
    else
    {
        for (const auto& kv : presets[(size_t) index].values)
        {
            if (auto* p = apvts.getParameter (kv.first))
                p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (kv.second));
        }
    }

    currentProgram = index;
    currentProgramName = presets[(size_t) index].name;
}

void ToneRigAudioProcessor::resetToDefaultPatch()
{
    for (auto* p : getParameters())
        p->setValueNotifyingHost (p->getDefaultValue());
}

//==============================================================================
void ToneRigAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentProgramName, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ToneRigAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (state);

            if (state.hasProperty ("presetName"))
                currentProgramName = state.getProperty ("presetName").toString();
        }
    }
}

} // namespace tonerig

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new tonerig::ToneRigAudioProcessor();
}
