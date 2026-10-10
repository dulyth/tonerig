/*
    ToneRig - the whole guitar rig signal chain.

    Deliberately independent of juce::AudioProcessor so it can be unit tested
    from a plain console binary (see Tests/SmokeTest.cpp).
*/
#pragma once

#include "RigDsp.h"

#include <juce_dsp/juce_dsp.h>

#include <atomic>

namespace tonerig
{

//==============================================================================
enum AmpModelId
{
    ampClean = 0,
    ampCrunch,
    ampLead,
    ampFuzz,
    ampAcoustic,
    numAmpModels
};

enum CabModelId
{
    cabV30 = 0,
    cabGreen,
    cabOpen2x12,
    cabCombo1x12,
    cabDirect,
    numCabModels
};

enum ModTypeId
{
    modChorus = 0,
    modFlanger,
    modPhaser,
    numModTypes
};

//==============================================================================
/** Plain value object describing the current state of the rig. */
struct RigParams
{
    float inputGainDb   = 0.0f;

    bool  gateOn        = false;
    float gateThreshDb  = -60.0f;
    float gateReleaseMs = 150.0f;

    bool  odOn          = false;
    float odDrive       = 0.30f;
    float odTone        = 0.50f;
    float odLevel       = 0.50f;

    bool  ampOn         = true;
    int   ampModel      = ampCrunch;
    float ampGain       = 0.50f;
    float ampBass       = 0.50f;
    float ampMid        = 0.50f;
    float ampTreble     = 0.50f;
    float ampPresence   = 0.50f;
    float ampMaster     = 0.50f;

    bool  cabOn         = true;
    int   cabModel      = cabV30;
    float cabLevel      = 0.50f;

    bool  modOn         = false;
    int   modType       = modChorus;
    float modRate       = 0.30f;    // normalised, mapped to 0.05 .. 8 Hz
    float modDepth      = 0.40f;
    float modMix        = 0.50f;

    bool  delayOn       = false;
    float delayTimeMs   = 320.0f;
    float delayFeedback = 0.35f;
    float delayMix      = 0.25f;
    bool  delayPingPong = false;

    bool  revOn         = false;
    float revSize       = 0.55f;
    float revDamp       = 0.50f;
    float revWidth      = 1.00f;
    float revMix        = 0.20f;

    bool  eqOn          = false;
    float eqLowDb       = 0.0f;
    float eqMidDb       = 0.0f;
    float eqHighDb      = 0.0f;

    float outputGainDb  = 0.0f;
    bool  limiterOn     = true;
};

//==============================================================================
class RigChain
{
public:
    RigChain();

    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    /** Snapshot the parameters. Called at the top of every block. */
    void setParams (const RigParams& newParams) noexcept { params = newParams; }

    /** Audio-thread entry point. Works in place on any channel count. */
    void process (juce::AudioBuffer<float>& buffer);

    float getInputLevel()  const noexcept { return inputLevel.load (std::memory_order_relaxed); }
    float getOutputLevel() const noexcept { return outputLevel.load (std::memory_order_relaxed); }

    /** Tuner data is captured on the audio thread and analysed on demand. */
    TunerReading getTunerReading() const noexcept { return tuner.getReading(); }
    bool tunerWantsAnalysis() noexcept { return tuner.consumeAnalysisRequest(); }
    void analyseTuner() noexcept { tuner.analyse(); }

    double getSampleRate() const noexcept { return fs; }

private:
    void updateAmpCoefficients();
    void updateCabCoefficients();
    void updateEqCoefficients();

    void processGate (juce::AudioBuffer<float>& b);
    void processOverdrive (juce::AudioBuffer<float>& b);
    void processAmp (juce::AudioBuffer<float>& b);
    void processCab (juce::AudioBuffer<float>& b);
    void processModulation (juce::AudioBuffer<float>& b);
    void processDelay (juce::AudioBuffer<float>& b);
    void processReverb (juce::AudioBuffer<float>& b);
    void processEq (juce::AudioBuffer<float>& b);
    void processOutput (juce::AudioBuffer<float>& b);

    //== state =================================================================
    RigParams params;
    double fs = 48000.0;
    int maxBlock = 512;
    int numChannelsPrepared = 2;

    juce::AudioBuffer<float> scratch;

    // input / output
    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f };
    float inputLevelFollower = 0.0f, outputLevelFollower = 0.0f;

    // gate
    float gateEnvelope = 0.0f;
    float gateGain = 0.0f;

    // overdrive
    ChanFilter odPreHp, odToneLp, odDcBlock;

    // amp
    ChanFilter ampPreHp, ampDcBlock, ampBass, ampMid, ampTreble, ampPresence, ampPostLp;
    ChanFilter ampStageLp;          // inter-stage low pass for cascaded models

    // cabinet
    ChanFilter cabHp, cabPeak1, cabPeak2, cabLp;

    // modulation
    DelayLine modLine[2];
    ChanFilter modDcBlock;
    float modPhase = 0.0f;
    float modFeedbackState[2] { 0.0f, 0.0f };
    float phaserState[2][6] = {};
    float phaserFeedback[2] { 0.0f, 0.0f };

    // delay
    DelayLine delayLine[2];
    SmoothedValue<float> delayTimeSmoother;
    bool delayTimePrimed = false;
    float delayLpState[2] { 0.0f, 0.0f };

    // reverb
    juce::Reverb reverb;
    juce::Reverb::Parameters lastReverbParams {};

    // eq
    ChanFilter eqLow, eqMid, eqHigh;

    // output
    float limiterEnv = 1.0f;

    // tuner
    Tuner tuner;
};

} // namespace tonerig
