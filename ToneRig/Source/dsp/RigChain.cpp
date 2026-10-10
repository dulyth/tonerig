#include "RigChain.h"

#include <cmath>

namespace tonerig
{

namespace
{
    constexpr float kMinusInfDb = -120.0f;

    inline float dbToGain (float db) noexcept
    {
        return db <= kMinusInfDb ? 0.0f : std::pow (10.0f, db * 0.05f);
    }

    inline float softClipSym (float x) noexcept
    {
        return std::tanh (x);
    }

    inline float softClipAsym (float x) noexcept
    {
        return x >= 0.0f ? std::tanh (x) : std::tanh (x * 0.75f) * 0.86f;
    }

    inline float fuzzClip (float x) noexcept
    {
        const float t = std::tanh (x * 0.9f);
        return t >= 0.0f ? t : t * 0.55f;
    }

    /** Per model voicing constants. */
    struct AmpVoicing
    {
        double preHpFreq, preHpQ;
        double driveBase, driveRange;
        double makeupScale;
        double postLpFreq, postLpQ;
        bool   powerSat;
    };

    AmpVoicing getVoicing (int model) noexcept
    {
        switch (model)
        {
            case ampClean:    return {  90.0, 0.7,  0.6,   4.0, 1.00, 12000.0, 0.7, false };
            case ampCrunch:   return { 110.0, 0.7,  1.0,  18.0, 1.30, 10000.0, 0.6, false };
            case ampLead:     return { 130.0, 0.8,  1.5,  55.0, 1.70,  9000.0, 0.6, true  };
            case ampFuzz:     return {  80.0, 0.6,  1.0, 100.0, 1.90,  8000.0, 0.7, true  };
            case ampAcoustic: return {  60.0, 0.5,  0.5,   1.6, 1.00, 14000.0, 0.5, false };
            default:          return { 110.0, 0.7,  1.0,  18.0, 1.30, 10000.0, 0.6, false };
        }
    }

    struct CabVoicing
    {
        double hpFreq, hpQ;
        double peak1Freq, peak1Q, peak1GainDb;
        double peak2Freq, peak2Q, peak2GainDb;
        double lpFreq, lpQ;
    };

    CabVoicing getCabVoicing (int model) noexcept
    {
        switch (model)
        {
            case cabV30:      return {  75.0, 0.70,  105.0, 1.20,  4.5,  3000.0, 1.30,  4.0, 5500.0, 0.70 };
            case cabGreen:    return {  80.0, 0.70,  112.0, 1.10,  3.8,  2400.0, 1.20,  3.0, 5000.0, 0.75 };
            case cabOpen2x12: return {  88.0, 0.70,  122.0, 1.00,  2.8,  2800.0, 1.10,  2.2, 6200.0, 0.70 };
            case cabCombo1x12:return {  95.0, 0.70,  135.0, 1.00,  3.2,  3500.0, 1.30,  2.0, 4800.0, 0.80 };
            case cabDirect:   return {  25.0, 0.70,  120.0, 0.80,  0.0,  1000.0, 0.80,  0.0, 9000.0, 0.70 };
            default:          return {  75.0, 0.70,  105.0, 1.20,  4.5,  3000.0, 1.30,  4.0, 5500.0, 0.70 };
        }
    }

    /** One-pole coefficient for a given time constant in milliseconds. */
    inline float onePoleCoef (double sampleRate, double ms) noexcept
    {
        return (float) (1.0 - std::exp (-1.0 / (juce::jmax (0.01, ms) * 0.001 * sampleRate)));
    }
}

//==============================================================================
RigChain::RigChain()
{
    for (auto& l : modLine)     l.prepare (4096);
    for (auto& l : delayLine)   l.prepare (4096);
}

void RigChain::prepare (double sampleRate, int maximumBlockSize, int numChannels)
{
    fs        = sampleRate > 0.0 ? sampleRate : 48000.0;
    maxBlock  = juce::jmax (1, maximumBlockSize);
    numChannelsPrepared = juce::jlimit (1, 2, numChannels);

    scratch.setSize (2, maxBlock, false, true, true);

    const int channels = 2;
    for (auto* f : { &odPreHp, &odToneLp, &odDcBlock,
                     &ampPreHp, &ampDcBlock, &ampBass, &ampMid, &ampTreble, &ampPresence,
                     &ampPostLp, &ampStageLp,
                     &cabHp, &cabPeak1, &cabPeak2, &cabLp,
                     &modDcBlock,
                     &eqLow, &eqMid, &eqHigh })
        f->prepare (channels);

    for (auto& l : modLine)
        l.prepare ((int) (fs * 0.08) + 8);

    for (auto& l : delayLine)
        l.prepare ((int) (fs * 1.4) + 8);

    delayTimeSmoother.reset (fs, 0.08, (float) (fs * 0.32));
    delayTimePrimed = false;

    reverb.setSampleRate (fs);
    lastReverbParams = {};

    inputLevel.store (0.0f);
    outputLevel.store (0.0f);
    inputLevelFollower = outputLevelFollower = 0.0f;
    gateEnvelope = 0.0f;
    gateGain = 0.0f;
    modPhase = 0.0f;
    limiterEnv = 1.0f;

    tuner.prepare (fs);
    reset();
}

void RigChain::reset()
{
    scratch.clear();

    for (auto* f : { &odPreHp, &odToneLp, &odDcBlock,
                     &ampPreHp, &ampDcBlock, &ampBass, &ampMid, &ampTreble, &ampPresence,
                     &ampPostLp, &ampStageLp,
                     &cabHp, &cabPeak1, &cabPeak2, &cabLp,
                     &modDcBlock,
                     &eqLow, &eqMid, &eqHigh })
        f->clear();

    for (auto& l : modLine)   l.clear();
    for (auto& l : delayLine) l.clear();

    std::fill (std::begin (modFeedbackState), std::end (modFeedbackState), 0.0f);
    std::fill (std::begin (phaserFeedback), std::end (phaserFeedback), 0.0f);
    std::fill (std::begin (delayLpState), std::end (delayLpState), 0.0f);
    std::fill (std::begin (phaserState[0]), std::end (phaserState[0]), 0.0f);
    std::fill (std::begin (phaserState[1]), std::end (phaserState[1]), 0.0f);

    delayTimePrimed = false;
    reverb.reset();
    tuner.clear();
}

//==============================================================================
void RigChain::updateAmpCoefficients()
{
    const auto v = getVoicing (params.ampModel);

    ampPreHp.c.setHighPass (fs, v.preHpFreq, v.preHpQ);
    ampDcBlock.c.setHighPass (fs, 22.0, 0.707);
    ampStageLp.c.setLowPass (fs, 7000.0, 0.707);

    const auto bassDb   = (params.ampBass     - 0.5f) * 24.0f;
    const auto midDb    = (params.ampMid      - 0.5f) * 24.0f;
    const auto trebleDb = (params.ampTreble   - 0.5f) * 24.0f;
    const auto presDb   = (params.ampPresence - 0.5f) * 20.0f;

    ampBass.c.setLowShelf    (fs, 110.0,  0.70, bassDb);
    ampMid.c.setPeak         (fs, 700.0,  0.80, midDb);
    ampTreble.c.setHighShelf (fs, 3200.0, 0.70, trebleDb);
    ampPresence.c.setHighShelf (fs, 4500.0, 0.70, presDb);
    ampPostLp.c.setLowPass   (fs, v.postLpFreq, v.postLpQ);
}

void RigChain::updateCabCoefficients()
{
    const auto c = getCabVoicing (params.cabModel);

    cabHp.c.setHighPass (fs, c.hpFreq, c.hpQ);
    cabPeak1.c.setPeak  (fs, c.peak1Freq, c.peak1Q, c.peak1GainDb);
    cabPeak2.c.setPeak  (fs, c.peak2Freq, c.peak2Q, c.peak2GainDb);
    cabLp.c.setLowPass  (fs, c.lpFreq, c.lpQ);
}

void RigChain::updateEqCoefficients()
{
    eqLow.c.setLowShelf    (fs, 120.0,  0.70, params.eqLowDb);
    eqMid.c.setPeak        (fs, 1000.0, 0.70, params.eqMidDb);
    eqHigh.c.setHighShelf  (fs, 4000.0, 0.70, params.eqHighDb);
}

//==============================================================================
void RigChain::process (juce::AudioBuffer<float>& buffer)
{
    const int numCh = buffer.getNumChannels();
    const int n     = buffer.getNumSamples();

    if (n <= 0 || numCh <= 0)
        return;

    if (n > scratch.getNumSamples())
    {
        jassertfalse;   // host gave us a bigger block than promised
        scratch.setSize (2, n, false, false, true);
    }

    // --- capture input for metering + tuner -------------------------------
    float inPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, n));

    const auto* tunerData = buffer.getReadPointer (0);
    for (int k = 0; k < n; ++k)
        tuner.push (tunerData[k]);

    inputLevelFollower = juce::jmax (inPeak, inputLevelFollower * 0.85f);
    inputLevel.store (inputLevelFollower, std::memory_order_relaxed);

    // --- copy into the stereo working buffer ------------------------------
    scratch.clear();
    if (numCh == 1)
    {
        scratch.copyFrom (0, 0, buffer, 0, 0, n);
        scratch.copyFrom (1, 0, buffer, 0, 0, n);
    }
    else
    {
        scratch.copyFrom (0, 0, buffer, 0, 0, n);
        scratch.copyFrom (1, 0, buffer, 1, 0, n);
    }

    // --- input trim --------------------------------------------------------
    scratch.applyGain (dbToGain (params.inputGainDb));

    if (params.gateOn)   processGate (scratch);
    if (params.odOn)     processOverdrive (scratch);
    if (params.ampOn)    processAmp (scratch);
    if (params.cabOn)    processCab (scratch);
    if (params.modOn)    processModulation (scratch);
    if (params.delayOn)  processDelay (scratch);
    if (params.revOn)    processReverb (scratch);
    if (params.eqOn)     processEq (scratch);

    processOutput (scratch);

    // --- write back --------------------------------------------------------
    for (int ch = 0; ch < numCh; ++ch)
        buffer.copyFrom (ch, 0, scratch, ch % 2, 0, n);

    float outPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        outPeak = juce::jmax (outPeak, buffer.getMagnitude (ch, 0, n));

    outputLevelFollower = juce::jmax (outPeak, outputLevelFollower * 0.85f);
    outputLevel.store (outputLevelFollower, std::memory_order_relaxed);
}

//==============================================================================
void RigChain::processGate (juce::AudioBuffer<float>& b)
{
    const int n  = b.getNumSamples();
    const int ch = b.getNumChannels();

    const float thresh = dbToGain (params.gateThreshDb);
    const float closeThreshold = thresh * 0.6f;
    const float attackCoef  = onePoleCoef (fs, 0.6);
    const float releaseCoef = onePoleCoef (fs, params.gateReleaseMs);
    const float envRelease  = onePoleCoef (fs, 60.0);

    bool open = gateEnvelope > thresh;

    for (int k = 0; k < n; ++k)
    {
        float peak = 0.0f;
        for (int c = 0; c < ch; ++c)
            peak = juce::jmax (peak, std::abs (b.getReadPointer (c)[k]));

        gateEnvelope = peak > gateEnvelope ? peak
                                           : gateEnvelope + (peak - gateEnvelope) * envRelease;

        if (! open && gateEnvelope > thresh)        open = true;
        else if (open && gateEnvelope < closeThreshold) open = false;

        const float target = open ? 1.0f : 0.0f;
        const float coef = target > gateGain ? attackCoef : releaseCoef;
        gateGain += (target - gateGain) * coef;

        for (int c = 0; c < ch; ++c)
            b.getWritePointer (c)[k] *= gateGain;
    }
}

//==============================================================================
void RigChain::processOverdrive (juce::AudioBuffer<float>& b)
{
    const int n  = b.getNumSamples();
    const int ch = b.getNumChannels();

    odPreHp.c.setHighPass (fs, 720.0, 0.70);
    odDcBlock.c.setHighPass (fs, 22.0, 0.707);

    const float toneHz = 800.0f * std::pow (10.0f, juce::jlimit (0.0f, 1.0f, params.odTone));
    odToneLp.c.setLowPass (fs, toneHz, 0.70);

    const float drive  = 1.0f + juce::jlimit (0.0f, 1.0f, params.odDrive) * 30.0f;
    const float makeup = 1.0f / std::sqrt (drive);
    const float level  = juce::jlimit (0.0f, 1.0f, params.odLevel);
    const float outGain = level * level * 2.0f;

    odPreHp.process (b);

    for (int c = 0; c < ch; ++c)
    {
        auto* d = b.getWritePointer (c);
        for (int k = 0; k < n; ++k)
            d[k] = softClipAsym (d[k] * drive) * makeup;
    }

    odDcBlock.process (b);
    odToneLp.process (b);
    b.applyGain (outGain);
}

//==============================================================================
void RigChain::processAmp (juce::AudioBuffer<float>& b)
{
    const int n  = b.getNumSamples();
    const int ch = b.getNumChannels();

    updateAmpCoefficients();

    const auto v = getVoicing (params.ampModel);
    const float drive  = (float) (v.driveBase + juce::jlimit (0.0f, 1.0f, params.ampGain) * v.driveRange);
    const float makeup = (float) (v.makeupScale / std::sqrt (drive));
    const float master = juce::jlimit (0.0f, 1.0f, params.ampMaster);
    const float masterGain = master * master * 2.0f;

    ampPreHp.process (b);

    for (int c = 0; c < ch; ++c)
    {
        auto* d = b.getWritePointer (c);

        switch (params.ampModel)
        {
            case ampClean:
                for (int k = 0; k < n; ++k)
                    d[k] = softClipSym (d[k] * drive) * makeup;
                break;

            case ampCrunch:
                for (int k = 0; k < n; ++k)
                    d[k] = softClipAsym (d[k] * drive) * makeup;
                break;

            case ampLead:
                for (int k = 0; k < n; ++k)
                {
                    float x = softClipSym (d[k] * drive) * 0.7f;
                    x = ampStageLp.processSample (x, c);
                    x = softClipSym (x * 1.8f) * 0.8f;
                    x = ampStageLp.processSample (x, c);
                    x = softClipSym (x * 2.4f);
                    d[k] = x * makeup;
                }
                break;

            case ampFuzz:
                for (int k = 0; k < n; ++k)
                    d[k] = fuzzClip (d[k] * drive) * makeup;
                break;

            case ampAcoustic:
            default:
                for (int k = 0; k < n; ++k)
                {
                    const float x = d[k] * drive;
                    d[k] = (softClipSym (x) * 0.85f + x * 0.15f) * makeup;
                }
                break;
        }
    }

    ampDcBlock.process (b);
    ampBass.process (b);
    ampMid.process (b);
    ampTreble.process (b);
    ampPresence.process (b);
    ampPostLp.process (b);

    for (int c = 0; c < ch; ++c)
    {
        auto* d = b.getWritePointer (c);
        for (int k = 0; k < n; ++k)
        {
            float x = d[k] * masterGain;
            if (v.powerSat)
                x = std::tanh (x * 1.1f) * 0.909f;
            d[k] = x;
        }
    }
}

//==============================================================================
void RigChain::processCab (juce::AudioBuffer<float>& b)
{
    updateCabCoefficients();

    cabHp.process (b);
    cabPeak1.process (b);
    cabPeak2.process (b);
    cabLp.process (b);

    const float levelDb = (juce::jlimit (0.0f, 1.0f, params.cabLevel) - 0.5f) * 24.0f;
    b.applyGain (dbToGain (levelDb));
}

//==============================================================================
void RigChain::processModulation (juce::AudioBuffer<float>& b)
{
    const int n  = b.getNumSamples();
    const int ch = b.getNumChannels();

    modDcBlock.c.setHighPass (fs, 18.0, 0.707);

    const float rateHz = 0.05f * std::pow (160.0f, juce::jlimit (0.0f, 1.0f, params.modRate));
    const float depth  = juce::jlimit (0.0f, 1.0f, params.modDepth);
    const float mix    = juce::jlimit (0.0f, 1.0f, params.modMix);

    const float phaseInc = rateHz / (float) fs;

    if (params.modType == modPhaser)
    {
        for (int k = 0; k < n; ++k)
        {
            modPhase += phaseInc;
            if (modPhase >= 1.0f)
                modPhase -= 1.0f;

            for (int c = 0; c < ch; ++c)
            {
                const float lfo = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi
                                                          * (modPhase + (c == 1 ? 0.25f : 0.0f)));
                const float centre = 250.0f + lfo * depth * 1550.0f;
                const float t = std::tan (juce::MathConstants<float>::pi * centre / (float) fs);
                const float g = (1.0f - t) / (1.0f + t);

                const float dry = b.getReadPointer (c)[k];
                float x = dry + phaserFeedback[c] * 0.35f;

                for (int s = 0; s < 6; ++s)
                {
                    const float y = -g * x + phaserState[c][s];
                    phaserState[c][s] = x + g * y;
                    x = y;
                }

                phaserFeedback[c] = juce::jlimit (-1.5f, 1.5f, x);
                b.getWritePointer (c)[k] = dry * (1.0f - mix) + x * mix;
            }
        }
        return;
    }

    // chorus / flanger share a modulated delay
    const float baseMs  = (params.modType == modChorus) ? 15.0f : 1.2f;
    const float depthMs = (params.modType == modChorus) ? 5.0f * depth : 1.0f * depth;
    const float feedback = (params.modType == modFlanger) ? 0.45f * depth : 0.0f;

    for (int k = 0; k < n; ++k)
    {
        modPhase += phaseInc;
        if (modPhase >= 1.0f)
            modPhase -= 1.0f;

        for (int c = 0; c < ch; ++c)
        {
            const float lfo = std::sin (juce::MathConstants<float>::twoPi
                                        * (modPhase + (c == 1 ? 0.25f : 0.0f)));
            const float delayMs = baseMs + depthMs * (0.5f + 0.5f * lfo);
            const float delaySamples = juce::jlimit (1.0f, (float) modLine[0].buffer.size() - 3.0f,
                                                     delayMs * 0.001f * (float) fs);

            const float dry = b.getReadPointer (c)[k];
            const float wet = modLine[c].read (delaySamples);
            modLine[c].write (dry + wet * feedback);

            b.getWritePointer (c)[k] = dry * (1.0f - mix) + wet * mix;
        }
    }

    modDcBlock.process (b);
}

//==============================================================================
void RigChain::processDelay (juce::AudioBuffer<float>& b)
{
    const int n  = b.getNumSamples();
    const int ch = b.getNumChannels();

    const float timeMs = juce::jlimit (5.0f, 1300.0f, params.delayTimeMs);
    const float feedback = juce::jlimit (0.0f, 0.95f, params.delayFeedback);
    const float mix = juce::jlimit (0.0f, 1.0f, params.delayMix);
    const bool pingPong = params.delayPingPong;

    const float targetSamples = timeMs * 0.001f * (float) fs;
    delayTimeSmoother.setTarget (targetSamples);

    if (! delayTimePrimed)
    {
        // Don't sweep from the default time on the very first block.
        delayTimeSmoother.snapTo (targetSamples);
        delayTimePrimed = true;
    }

    const float lpCoef = onePoleCoef (fs, 2.4);

    for (int k = 0; k < n; ++k)
    {
        const float d = delayTimeSmoother.getNext();

        float wet[2] { 0.0f, 0.0f };
        for (int c = 0; c < ch && c < 2; ++c)
            wet[c] = delayLine[c].read (d);

        for (int c = 0; c < ch && c < 2; ++c)
            delayLpState[c] += (wet[c] - delayLpState[c]) * lpCoef;

        for (int c = 0; c < ch && c < 2; ++c)
        {
            const float dry = b.getReadPointer (c)[k];
            const int other = 1 - c;
            const float fbSource = (pingPong && ch > 1) ? delayLpState[other] : delayLpState[c];
            delayLine[c].write (dry + fbSource * feedback);
            b.getWritePointer (c)[k] = dry * (1.0f - mix) + delayLpState[c] * mix;
        }
    }
}

//==============================================================================
void RigChain::processReverb (juce::AudioBuffer<float>& b)
{
    const int n  = b.getNumSamples();
    const int ch = b.getNumChannels();

    juce::Reverb::Parameters rp;
    rp.roomSize   = juce::jlimit (0.0f, 1.0f, params.revSize);
    rp.damping    = juce::jlimit (0.0f, 1.0f, params.revDamp);
    rp.width      = juce::jlimit (0.0f, 1.0f, params.revWidth);
    rp.freezeMode = 0.0f;
    const float mix = juce::jlimit (0.0f, 1.0f, params.revMix);
    rp.dryLevel = 1.0f - mix;
    rp.wetLevel = mix;

    if (rp.roomSize != lastReverbParams.roomSize
        || rp.damping  != lastReverbParams.damping
        || rp.width    != lastReverbParams.width
        || rp.dryLevel != lastReverbParams.dryLevel
        || rp.wetLevel != lastReverbParams.wetLevel)
    {
        reverb.setParameters (rp);
        lastReverbParams = rp;
    }

    float* left  = b.getWritePointer (0);
    float* right = ch > 1 ? b.getWritePointer (1) : nullptr;

    if (right != nullptr)
        reverb.processStereo (left, right, n);
    else
        reverb.processMono (left, n);
}

//==============================================================================
void RigChain::processEq (juce::AudioBuffer<float>& b)
{
    updateEqCoefficients();

    eqLow.process (b);
    eqMid.process (b);
    eqHigh.process (b);
}

//==============================================================================
void RigChain::processOutput (juce::AudioBuffer<float>& b)
{
    const int n  = b.getNumSamples();
    const int ch = b.getNumChannels();

    b.applyGain (dbToGain (params.outputGainDb));

    if (! params.limiterOn)
        return;

    constexpr float ceiling = 0.955f;       // roughly -0.4 dBFS
    const float relCoef = onePoleCoef (fs, 150.0);

    for (int k = 0; k < n; ++k)
    {
        float peak = 0.0f;
        for (int c = 0; c < ch; ++c)
            peak = juce::jmax (peak, std::abs (b.getReadPointer (c)[k]));

        // Instant attack, slow release: a classic peak limiter envelope.
        if (peak > limiterEnv)
            limiterEnv = peak;
        else
            limiterEnv += (peak - limiterEnv) * relCoef;

        const float g = limiterEnv > ceiling ? ceiling / limiterEnv : 1.0f;

        for (int c = 0; c < ch; ++c)
        {
            float x = b.getWritePointer (c)[k] * g;
            x = juce::jlimit (-1.0f, 1.0f, x);               // final safety clip
            b.getWritePointer (c)[k] = x;
        }
    }
}

} // namespace tonerig
