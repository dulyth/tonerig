/*
    Offline verification of the ToneRig DSP chain.

    Runs without a plugin host or GUI: it drives tonerig::RigChain directly and
    checks for silence handling, numerical health, gain staging, gate behaviour,
    delay echoes, reverb tails, amp model sanity and the tuner.
*/
#include <juce_dsp/juce_dsp.h>

#include "dsp/RigChain.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace tonerig;

namespace
{
    int failures = 0;
    int checks   = 0;

    void check (bool condition, const juce::String& what)
    {
        ++checks;
        if (condition)
        {
            std::printf ("  ok   %s\n", what.toRawUTF8());
        }
        else
        {
            ++failures;
            std::printf ("  FAIL %s\n", what.toRawUTF8());
        }
    }

    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 512;

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            const auto* d = b.getReadPointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (d[i]) || std::abs (d[i]) > 1.0e6f)
                    return false;
        }
        return true;
    }

    float rms (const juce::AudioBuffer<float>& b)
    {
        double sum = 0.0;
        int count = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            const auto* d = b.getReadPointer (ch);
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                sum += (double) d[i] * d[i];
                ++count;
            }
        }
        return count > 0 ? (float) std::sqrt (sum / count) : 0.0f;
    }

    float peak (const juce::AudioBuffer<float>& b)
    {
        float p = 0.0f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            p = juce::jmax (p, b.getMagnitude (ch, 0, b.getNumSamples()));
        return p;
    }

    /** Streams a generator through the chain for `seconds` and returns the last block. */
    template <typename Gen>
    juce::AudioBuffer<float> run (RigChain& chain, const RigParams& params, double seconds, Gen&& gen)
    {
        const int total = (int) (kSampleRate * seconds);
        juce::AudioBuffer<float> block (2, kBlockSize);
        juce::AudioBuffer<float> copy (2, kBlockSize);
        juce::AudioBuffer<float> last (2, kBlockSize);

        int produced = 0;
        int n = 0;
        for (int pos = 0; pos < total; pos += kBlockSize)
        {
            n = juce::jmin (kBlockSize, total - pos);
            block.setSize (2, n, false, false, true);

            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = block.getWritePointer (ch);
                for (int i = 0; i < n; ++i)
                    d[i] = gen (produced + i, ch);
            }

            chain.setParams (params);
            chain.process (block);

            if (pos + n >= total)
            {
                last.makeCopyOf (block);
            }
            produced += n;
        }

        juce::ignoreUnused (copy);
        return last;
    }

    struct SineGen
    {
        double freq, amp;
        float operator() (int i, int) const
        {
            return (float) (amp * std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / kSampleRate));
        }
    };

    struct SilenceGen  { float operator() (int, int) const { return 0.0f; } };

    struct NoiseGen
    {
        double amp;
        float operator() (int i, int ch) const
        {
            // deterministic pseudo random, different per channel
            uint32_t x = (uint32_t) (i * 2654435761u + (uint32_t) ch * 0x9e3779b9u);
            x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15;
            return (float) ((double) (x & 0xffff) / 32768.0 - 1.0) * (float) amp;
        }
    };
}

//==============================================================================
int main()
{
    std::printf ("ToneRig DSP smoke test\n=========================\n");

    //--------------------------------------------------------------------------
    std::printf ("\n[1] Silent input -> silent output (no noise, no NaN)\n");
    {
        RigChain chain;
        chain.prepare (kSampleRate, kBlockSize, 2);

        RigParams p;
        p.ampOn = true; p.cabOn = true; p.modOn = true; p.delayOn = true; p.revOn = true;
        p.eqOn = true; p.gateOn = true;

        auto out = run (chain, p, 0.5, SilenceGen {});
        check (allFinite (out), "output is finite");
        check (peak (out) < 1.0e-4f, "silence stays silent (peak < -80 dBFS)");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n[2] Chain is transparent when every module is bypassed\n");
    {
        RigChain chain;
        chain.prepare (kSampleRate, kBlockSize, 2);

        RigParams p;
        p.ampOn = p.cabOn = p.modOn = p.delayOn = p.revOn = p.eqOn = p.gateOn = false;
        p.limiterOn = false;

        const int n = 256;
        juce::AudioBuffer<float> in (2, n);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                in.getWritePointer (ch)[i] = (float) (0.4 * std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / kSampleRate));

        juce::AudioBuffer<float> out;
        out.makeCopyOf (in);
        chain.setParams (p);
        chain.process (out);

        double maxDiff = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                maxDiff = juce::jmax (maxDiff, (double) std::abs (out.getReadPointer (ch)[i]
                                                                  - in.getReadPointer (ch)[i]));
        check (maxDiff < 1.0e-6, "bypassed chain is bit transparent (max diff < 1e-6)");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n[3] Every amp / cab / mod model stays numerically healthy\n");
    {
        bool healthy = true;
        for (int amp = 0; amp < numAmpModels && healthy; ++amp)
        {
            for (int cab = 0; cab < numCabModels && healthy; ++cab)
            {
                for (int mod = 0; mod < numModTypes && healthy; ++mod)
                {
                    RigChain chain;
                    chain.prepare (kSampleRate, kBlockSize, 2);

                    RigParams p;
                    p.ampOn = true; p.ampModel = amp; p.ampGain = 0.8f;
                    p.cabOn = true; p.cabModel = cab;
                    p.modOn = true; p.modType = mod;
                    p.delayOn = true; p.revOn = true; p.eqOn = true; p.gateOn = true;
                    p.odOn = true; p.odDrive = 1.0f;

                    auto out = run (chain, p, 0.2, SineGen { 196.0, 0.7 });

                    if (! allFinite (out) || peak (out) > 1.0001f)
                    {
                        std::printf ("      amp=%d cab=%d mod=%d peak=%f\n", amp, cab, mod, peak (out));
                        healthy = false;
                    }
                }
            }
        }
        check (healthy, "all 5 amps x 5 cabs x 3 mods: finite output, no clipping past full scale");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n[4] Noise gate mutes quiet input and passes loud input\n");
    {
        RigChain gate;

        RigParams p;
        p.gateOn = true; p.gateThreshDb = -40.0f; p.gateReleaseMs = 50.0f;
        p.ampOn = true; p.ampModel = ampClean; p.ampGain = 0.2f; p.ampMaster = 0.6f;
        p.cabOn = false; p.limiterOn = true;

        RigParams q = p;
        q.gateOn = false;

        gate.prepare (kSampleRate, kBlockSize, 2);
        auto quietGated = run (gate, p, 0.4, NoiseGen { 0.002 });       // ~ -54 dBFS
        gate.prepare (kSampleRate, kBlockSize, 2);
        auto quietOpen  = run (gate, q, 0.4, NoiseGen { 0.002 });
        gate.prepare (kSampleRate, kBlockSize, 2);
        auto loudGated  = run (gate, p, 0.4, SineGen { 220.0, 0.5 });

        check (rms (quietGated) < rms (quietOpen) * 0.6f, "gate attenuates low level hum");
        check (peak (loudGated) > 0.01f, "gate stays open for a loud signal");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n[5] Delay produces a repeat at the requested time\n");
    {
        RigChain chain;
        chain.prepare (kSampleRate, kBlockSize, 2);

        RigParams p;
        p.delayOn = true; p.delayTimeMs = 200.0f; p.delayFeedback = 0.0f; p.delayMix = 0.5f;
        p.ampOn = p.cabOn = p.modOn = p.revOn = p.eqOn = p.gateOn = false;
        p.limiterOn = false;

        const int n = (int) (kSampleRate * 0.5);
        juce::AudioBuffer<float> buf (2, n);
        buf.clear();
        // a short, clearly audible burst at t = 0
        const int burst = 2400;   // 50 ms
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < burst; ++i)
                buf.getWritePointer (ch)[i] = (float) (0.8 * std::sin (2.0 * juce::MathConstants<double>::pi
                                                                       * 220.0 * i / kSampleRate));

        chain.setParams (p);
        chain.process (buf);

        const int expected = (int) (0.200 * kSampleRate);
        const int window = burst + 64;
        float energyAtEcho = 0.0f, energyBetween = 0.0f;
        for (int i = expected; i < expected + window && i < n; ++i)
            energyAtEcho = juce::jmax (energyAtEcho, std::abs (buf.getReadPointer (0)[i]));
        for (int i = burst + 200; i < expected - 200; ++i)
            energyBetween = juce::jmax (energyBetween, std::abs (buf.getReadPointer (0)[i]));

        check (energyAtEcho > 0.05f, "a repeat appears around 200 ms");
        check (energyBetween < 1.0e-4f, "nothing appears before the delay time");
        check (allFinite (buf), "delay output is finite");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n[6] Ping pong delay crosses the signal to the other channel\n");
    {
        RigChain chain;
        chain.prepare (kSampleRate, kBlockSize, 2);

        RigParams p;
        p.delayOn = true; p.delayTimeMs = 100.0f; p.delayFeedback = 0.6f; p.delayMix = 1.0f;
        p.delayPingPong = true;
        p.ampOn = p.cabOn = p.modOn = p.revOn = p.eqOn = p.gateOn = false;
        p.limiterOn = false;

        const int n = (int) (kSampleRate * 0.6);
        juce::AudioBuffer<float> buf (2, n);
        buf.clear();
        const int burst = 2400;                     // 50 ms burst, left channel only
        for (int i = 0; i < burst; ++i)
            buf.getWritePointer (0)[i] = (float) (0.8 * std::sin (2.0 * juce::MathConstants<double>::pi
                                                                  * 220.0 * i / kSampleRate));

        chain.setParams (p);
        chain.process (buf);

        // The impulse entered on the left. The first repeat stays on the left,
        // the second one crosses to the right, i.e. the repeats alternate.
        auto channelPeakAround = [&buf, n, burst] (int sample, int channel)
        {
            float p = 0.0f;
            for (int i = juce::jmax (0, sample - 8); i < juce::jmin (n, sample + burst + 64); ++i)
                p = juce::jmax (p, std::abs (buf.getReadPointer (channel)[i]));
            return p;
        };

        const int e1 = (int) (0.100 * kSampleRate);
        const int e2 = (int) (0.200 * kSampleRate);

        const float l1 = channelPeakAround (e1, 0), r1 = channelPeakAround (e1, 1);
        const float l2 = channelPeakAround (e2, 0), r2 = channelPeakAround (e2, 1);

        check (l1 > 0.05f && r1 < 1.0e-3f, "first repeat stays on the input channel");
        check (r2 > 0.05f && r2 > l2, "second repeat crosses to the other channel");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n[7] Reverb adds a decaying tail\n");
    {
        RigChain chain;
        chain.prepare (kSampleRate, kBlockSize, 2);

        RigParams p;
        p.revOn = true; p.revSize = 0.85f; p.revMix = 0.8f; p.revDamp = 0.2f;
        p.ampOn = p.cabOn = p.modOn = p.delayOn = p.eqOn = p.gateOn = false;
        p.limiterOn = false;

        const int n = (int) (kSampleRate * 1.5);
        juce::AudioBuffer<float> buf (2, n);
        buf.clear();
        for (int i = 0; i < 32; ++i)
            buf.getWritePointer (0)[i] = 0.8f;

        chain.setParams (p);
        chain.process (buf);

        float early = 0.0f, late = 0.0f;
        for (int i = 500; i < 5000; ++i)      early = juce::jmax (early, std::abs (buf.getReadPointer (0)[i]));
        for (int i = (int) (0.6 * kSampleRate); i < (int) (0.8 * kSampleRate); ++i)
            late = juce::jmax (late, std::abs (buf.getReadPointer (0)[i]));

        check (early > 1.0e-3f, "reverb tail present shortly after the impulse");
        check (late > 1.0e-5f, "reverb tail is still decaying after 600 ms");
        check (late < early, "reverb tail decays over time");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n[8] Amp drives the signal into saturation\n");
    {
        RigChain chain;
        chain.prepare (kSampleRate, kBlockSize, 2);

        RigParams p;
        p.ampOn = true; p.ampModel = ampLead; p.ampGain = 0.9f; p.ampMaster = 0.5f;
        p.cabOn = false; p.limiterOn = false;

        auto out = run (chain, p, 0.25, SineGen { 220.0, 0.3 });

        // crest factor of a heavily saturated sine drops well below the ~1.41 of a clean sine
        const float r = rms (out);
        const float pk = peak (out);
        const float crest = r > 0.0f ? pk / r : 99.0f;

        check (r > 0.01f, "saturated amp still produces output");
        check (crest < 1.35f, "lead channel squashes the crest factor (saturation)");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n[9] Tuner reports the played pitch\n");
    {
        RigChain chain;
        chain.prepare (kSampleRate, kBlockSize, 2);

        RigParams p;   // everything off, we only care about the input tap
        p.ampOn = p.cabOn = p.modOn = p.delayOn = p.revOn = p.eqOn = p.gateOn = false;

        run (chain, p, 0.4, SineGen { 82.41, 0.4 });     // low E

        while (chain.tunerWantsAnalysis())
            chain.analyseTuner();

        const auto reading = chain.getTunerReading();
        std::printf ("      detected %.2f Hz, midi %d, %.1f cents\n",
                     reading.frequency, reading.midiNote, reading.cents);
        check (reading.midiNote == 40, "low E (82.41 Hz) detected as MIDI note 40");
        check (std::abs (reading.cents) < 12.0f, "pitch is within 12 cents");
    }

    //--------------------------------------------------------------------------
    std::printf ("\n[10] Output limiter keeps the signal inside full scale\n");
    {
        RigChain chain;
        chain.prepare (kSampleRate, kBlockSize, 2);

        RigParams p;
        p.ampOn = true; p.ampModel = ampFuzz; p.ampGain = 1.0f; p.ampMaster = 1.0f;
        p.cabOn = true; p.eqOn = true; p.eqLowDb = 12.0f; p.eqMidDb = 12.0f; p.eqHighDb = 12.0f;
        p.odOn = true; p.odDrive = 1.0f; p.odLevel = 1.0f;
        p.outputGainDb = 12.0f;
        p.limiterOn = true;

        auto out = run (chain, p, 0.5, SineGen { 110.0, 0.9 });

        check (peak (out) <= 0.9561f, "limiter holds the peak at or below -0.39 dBFS");
        check (allFinite (out), "limiter output is finite");
    }

    std::printf ("\n=========================\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
