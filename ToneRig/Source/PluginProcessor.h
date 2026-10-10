#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/RigChain.h"

#include <vector>

namespace tonerig
{

//==============================================================================
namespace pid
{
    inline constexpr const char* inputGain      = "inputGain";

    inline constexpr const char* gateOn         = "gateOn";
    inline constexpr const char* gateThresh     = "gateThresh";
    inline constexpr const char* gateRelease    = "gateRelease";

    inline constexpr const char* odOn           = "odOn";
    inline constexpr const char* odDrive        = "odDrive";
    inline constexpr const char* odTone         = "odTone";
    inline constexpr const char* odLevel        = "odLevel";

    inline constexpr const char* ampOn          = "ampOn";
    inline constexpr const char* ampModel       = "ampModel";
    inline constexpr const char* ampGain        = "ampGain";
    inline constexpr const char* ampBass        = "ampBass";
    inline constexpr const char* ampMid         = "ampMid";
    inline constexpr const char* ampTreble      = "ampTreble";
    inline constexpr const char* ampPresence    = "ampPresence";
    inline constexpr const char* ampMaster      = "ampMaster";

    inline constexpr const char* cabOn          = "cabOn";
    inline constexpr const char* cabModel       = "cabModel";
    inline constexpr const char* cabLevel       = "cabLevel";

    inline constexpr const char* modOn          = "modOn";
    inline constexpr const char* modType        = "modType";
    inline constexpr const char* modRate        = "modRate";
    inline constexpr const char* modDepth       = "modDepth";
    inline constexpr const char* modMix         = "modMix";

    inline constexpr const char* delayOn        = "delayOn";
    inline constexpr const char* delayTime      = "delayTime";
    inline constexpr const char* delayFeedback  = "delayFeedback";
    inline constexpr const char* delayMix       = "delayMix";
    inline constexpr const char* delayPingPong  = "delayPingPong";

    inline constexpr const char* revOn          = "revOn";
    inline constexpr const char* revSize        = "revSize";
    inline constexpr const char* revDamp        = "revDamp";
    inline constexpr const char* revWidth       = "revWidth";
    inline constexpr const char* revMix         = "revMix";

    inline constexpr const char* eqOn           = "eqOn";
    inline constexpr const char* eqLow          = "eqLow";
    inline constexpr const char* eqMid          = "eqMid";
    inline constexpr const char* eqHigh         = "eqHigh";

    inline constexpr const char* outputGain     = "outputGain";
    inline constexpr const char* limiterOn      = "limiterOn";
}

//==============================================================================
struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;
};

const std::vector<FactoryPreset>& getFactoryPresets();

//==============================================================================
class ToneRigAudioProcessor : public juce::AudioProcessor
{
public:
    ToneRigAudioProcessor();
    ~ToneRigAudioProcessor() override = default;

    //== AudioProcessor ========================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override;

    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //== ToneRig ===============================================================
    /** Grabs the current parameter values in a form the DSP chain understands. */
    RigParams pullParams() const;

    /** Applies a factory preset by index. */
    void loadPreset (int index);
    int  getNumFactoryPresets() const { return (int) getFactoryPresets().size(); }

    void resetToDefaultPatch();

    juce::AudioProcessorValueTreeState apvts;
    RigChain chain;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    float valueOf (const char* id) const;

    int currentProgram = 0;
    juce::String currentProgramName { "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneRigAudioProcessor)
};

} // namespace tonerig
