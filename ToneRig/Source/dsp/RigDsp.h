/*
    ToneRig - shared low level DSP building blocks.
    Everything here is header-only, real-time safe and dependency free
    apart from the JUCE audio buffer type.
*/
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace tonerig
{

//==============================================================================
/** A plain RBJ biquad. Coefficients are shared, state lives in ChanFilter. */
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

    void setBypass() noexcept
    {
        b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f;
    }

    void normalise (double a0) noexcept
    {
        const auto inv = (float) (1.0 / a0);
        b0 = (float) (b0 * inv);
        b1 = (float) (b1 * inv);
        b2 = (float) (b2 * inv);
        a1 = (float) (a1 * inv);
        a2 = (float) (a2 * inv);
    }

    void setPeak (double fs, double f0, double q, double dbGain) noexcept
    {
        f0 = juce::jlimit (10.0, fs * 0.45, f0);
        const double A     = std::pow (10.0, dbGain / 40.0);
        const double w0    = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);

        const double a0 = 1.0 + alpha / A;
        b0 = (float) (1.0 + alpha * A);
        b1 = (float) (-2.0 * cosw);
        b2 = (float) (1.0 - alpha * A);
        a1 = (float) (-2.0 * cosw);
        a2 = (float) (1.0 - alpha / A);
        normalise (a0);
    }

    void setLowShelf (double fs, double f0, double q, double dbGain) noexcept
    {
        f0 = juce::jlimit (10.0, fs * 0.45, f0);
        const double A     = std::pow (10.0, dbGain / 40.0);
        const double w0    = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double sqA   = std::sqrt (A);

        const double a0 = (A + 1.0) + (A - 1.0) * cosw + 2.0 * sqA * alpha;
        b0 = (float) (A * ((A + 1.0) - (A - 1.0) * cosw + 2.0 * sqA * alpha));
        b1 = (float) (2.0 * A * ((A - 1.0) - (A + 1.0) * cosw));
        b2 = (float) (A * ((A + 1.0) - (A - 1.0) * cosw - 2.0 * sqA * alpha));
        a1 = (float) (-2.0 * ((A - 1.0) + (A + 1.0) * cosw));
        a2 = (float) ((A + 1.0) + (A - 1.0) * cosw - 2.0 * sqA * alpha);
        normalise (a0);
    }

    void setHighShelf (double fs, double f0, double q, double dbGain) noexcept
    {
        f0 = juce::jlimit (10.0, fs * 0.45, f0);
        const double A     = std::pow (10.0, dbGain / 40.0);
        const double w0    = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        const double sqA   = std::sqrt (A);

        const double a0 = (A + 1.0) - (A - 1.0) * cosw + 2.0 * sqA * alpha;
        b0 = (float) (A * ((A + 1.0) + (A - 1.0) * cosw + 2.0 * sqA * alpha));
        b1 = (float) (-2.0 * A * ((A - 1.0) + (A + 1.0) * cosw));
        b2 = (float) (A * ((A + 1.0) + (A - 1.0) * cosw - 2.0 * sqA * alpha));
        a1 = (float) (2.0 * ((A - 1.0) - (A + 1.0) * cosw));
        a2 = (float) ((A + 1.0) - (A - 1.0) * cosw - 2.0 * sqA * alpha);
        normalise (a0);
    }

    void setLowPass (double fs, double f0, double q) noexcept
    {
        f0 = juce::jlimit (10.0, fs * 0.45, f0);
        const double w0    = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);

        const double a0 = 1.0 + alpha;
        b0 = (float) ((1.0 - cosw) * 0.5);
        b1 = (float) (1.0 - cosw);
        b2 = (float) ((1.0 - cosw) * 0.5);
        a1 = (float) (-2.0 * cosw);
        a2 = (float) (1.0 - alpha);
        normalise (a0);
    }

    void setHighPass (double fs, double f0, double q) noexcept
    {
        f0 = juce::jlimit (10.0, fs * 0.45, f0);
        const double w0    = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);

        const double a0 = 1.0 + alpha;
        b0 = (float) ((1.0 + cosw) * 0.5);
        b1 = (float) (-(1.0 + cosw));
        b2 = (float) ((1.0 + cosw) * 0.5);
        a1 = (float) (-2.0 * cosw);
        a2 = (float) (1.0 - alpha);
        normalise (a0);
    }

    void setBandPass (double fs, double f0, double q) noexcept
    {
        f0 = juce::jlimit (10.0, fs * 0.45, f0);
        const double w0    = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);

        const double a0 = 1.0 + alpha;
        b0 = (float) alpha;
        b1 = 0.0f;
        b2 = (float) (-alpha);
        a1 = (float) (-2.0 * cosw);
        a2 = (float) (1.0 - alpha);
        normalise (a0);
    }

    inline float process (float x, float& z1, float& z2) const noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

//==============================================================================
/** A biquad plus per-channel state, applied to a whole buffer. */
struct ChanFilter
{
    Biquad c;
    std::vector<float> s1, s2;

    void prepare (int numChannels)
    {
        s1.assign ((size_t) numChannels, 0.0f);
        s2.assign ((size_t) numChannels, 0.0f);
    }

    void clear() noexcept
    {
        std::fill (s1.begin(), s1.end(), 0.0f);
        std::fill (s2.begin(), s2.end(), 0.0f);
    }

    void process (juce::AudioBuffer<float>& b) noexcept
    {
        const int n  = b.getNumSamples();
        const int ch = juce::jmin (b.getNumChannels(), (int) s1.size());

        for (int i = 0; i < ch; ++i)
        {
            auto* d = b.getWritePointer (i);
            float z1 = s1[(size_t) i];
            float z2 = s2[(size_t) i];

            for (int k = 0; k < n; ++k)
                d[k] = c.process (d[k], z1, z2);

            s1[(size_t) i] = z1;
            s2[(size_t) i] = z2;
        }
    }

    inline float processSample (float x, int channel) noexcept
    {
        return c.process (x, s1[(size_t) channel], s2[(size_t) channel]);
    }
};

//==============================================================================
/** One-pole smoother for parameter changes. */
template <typename T>
struct SmoothedValue
{
    void reset (double sampleRate, double rampSeconds, T startValue)
    {
        stepsToTarget = juce::jmax (1, (int) (rampSeconds * sampleRate));
        target = current = startValue;
        countdown = 0;
    }

    inline void setTarget (T v) noexcept
    {
        if (v != target)
        {
            target = v;
            countdown = stepsToTarget;
        }
    }

    inline T getNext() noexcept
    {
        if (countdown > 0)
        {
            current += (target - current) / (T) countdown;
            --countdown;
        }
        else
        {
            current = target;
        }
        return current;
    }

    /** Jump straight to a value without ramping (used on the first block). */
    void snapTo (T v) noexcept
    {
        current = target = v;
        countdown = 0;
    }

    T getCurrent() const noexcept { return current; }

    T current = T(), target = T();
    int stepsToTarget = 1, countdown = 0;
};

//==============================================================================
/** Fractional delay line used for modulation, delay and ping-pong. */
struct DelayLine
{
    std::vector<float> buffer;
    int writeIndex = 0;

    void prepare (int maxSamples)
    {
        buffer.assign ((size_t) juce::jmax (8, maxSamples), 0.0f);
        writeIndex = 0;
    }

    void clear() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    inline void write (float v) noexcept
    {
        buffer[(size_t) writeIndex] = v;
        if (++writeIndex >= (int) buffer.size())
            writeIndex = 0;
    }

    inline float read (float delaySamples) const noexcept
    {
        const int size = (int) buffer.size();
        delaySamples = juce::jlimit (0.0f, (float) size - 2.0f, delaySamples);

        float pos = (float) writeIndex - delaySamples;
        while (pos < 0.0f)
            pos += (float) size;

        int i0 = (int) pos;
        const float frac = pos - (float) i0;
        int i1 = i0 + 1;
        if (i1 >= size)
            i1 -= size;

        return buffer[(size_t) i0] * (1.0f - frac) + buffer[(size_t) i1] * frac;
    }
};

//==============================================================================
struct TunerReading
{
    float frequency = 0.0f;
    float cents     = 0.0f;
    int   midiNote  = -1;    // -1 == no reliable pitch
    float confidence = 0.0f;
};

/** Cheap autocorrelation tuner, fed from the (pre-gain) input. */
class Tuner
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        buffer.assign (kBufferSize, 0.0f);
        writeIndex = 0;
        sinceAnalysis = 0;
        reading = {};
    }

    void clear() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        reading = {};
    }

    void push (float s) noexcept
    {
        buffer[(size_t) writeIndex] = s;
        if (++writeIndex >= kBufferSize)
            writeIndex = 0;

        if (++sinceAnalysis >= kAnalyseInterval)
        {
            sinceAnalysis = 0;
            analysisPending = true;
        }
    }

    /** Runs the (relatively expensive) analysis. Safe to call from the audio
        thread but only every kAnalyseInterval samples; better still the editor
        can call it from a timer. */
    void analyse() noexcept
    {
        analysisPending = false;

        const int window = kWindowSize;
        std::vector<float> x ((size_t) window);

        // Copy the newest `window` samples in chronological order, DC removed.
        float mean = 0.0f;
        for (int i = 0; i < window; ++i)
        {
            const int idx = (writeIndex - window + i + kBufferSize * 2) % kBufferSize;
            x[(size_t) i] = buffer[(size_t) idx];
            mean += x[(size_t) i];
        }
        mean /= (float) window;

        float rms = 0.0f;
        for (auto& v : x)
        {
            v -= mean;
            rms += v * v;
        }
        rms = std::sqrt (rms / (float) window);

        if (rms < 0.0025f)          // roughly -52 dBFS, treat as silence
        {
            reading = {};
            return;
        }

        const int minLag = juce::jmax (2, (int) (fs / 1300.0));   // ~1300 Hz ceiling
        const int maxLag = juce::jmin (window - 2, (int) (fs / 65.0)); // ~65 Hz floor

        // Normalised autocorrelation (Hunt/nsdf-lite).
        float best = 0.0f;
        int   bestLag = -1;

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            float sum = 0.0f, e1 = 0.0f, e2 = 0.0f;
            const int n = window - lag;
            for (int i = 0; i < n; ++i)
            {
                const float a = x[(size_t) i];
                const float b = x[(size_t) (i + lag)];
                sum += a * b;
                e1  += a * a;
                e2  += b * b;
            }
            const float denom = std::sqrt (e1 * e2);
            if (denom <= 1.0e-9f)
                continue;

            const float v = sum / denom;
            if (v > best)
            {
                best = v;
                bestLag = lag;
            }
        }

        if (bestLag <= 0 || best < 0.55f)
        {
            reading = {};
            return;
        }

        // Parabolic interpolation around the peak for sub-sample accuracy.
        float refinedLag = (float) bestLag;
        if (bestLag > minLag && bestLag < maxLag)
        {
            const auto corr = [&] (int lag)
            {
                float sum = 0.0f, e1 = 0.0f, e2 = 0.0f;
                const int n = window - lag;
                for (int i = 0; i < n; ++i)
                {
                    const float a = x[(size_t) i];
                    const float b = x[(size_t) (i + lag)];
                    sum += a * b; e1 += a * a; e2 += b * b;
                }
                const float d = std::sqrt (e1 * e2);
                return d > 1.0e-9f ? sum / d : 0.0f;
            };

            const float y0 = corr (bestLag - 1);
            const float y1 = best;
            const float y2 = corr (bestLag + 1);
            const float denom = 2.0f * (2.0f * y1 - y0 - y2);
            if (std::abs (denom) > 1.0e-9f)
                refinedLag += (y2 - y0) / denom;
        }

        const float freq = (float) (fs) / refinedLag;
        if (freq < 55.0f || freq > 1400.0f)
        {
            reading = {};
            return;
        }

        const float midi = 69.0f + 12.0f * std::log2 (freq / 440.0f);
        const int   note = (int) std::llround (midi);
        const float cents = (midi - (float) note) * 100.0f;

        reading.frequency  = freq;
        reading.midiNote   = note;
        reading.cents      = cents;
        reading.confidence = best;
    }

    bool consumeAnalysisRequest() noexcept
    {
        if (! analysisPending)
            return false;
        analysisPending = false;
        return true;
    }

    TunerReading getReading() const noexcept { return reading; }

private:
    static constexpr int kBufferSize      = 4096;
    static constexpr int kWindowSize      = 1024;
    static constexpr int kAnalyseInterval = 2048;

    std::vector<float> buffer;
    int writeIndex = 0;
    int sinceAnalysis = 0;
    bool analysisPending = false;
    double fs = 48000.0;
    TunerReading reading {};
};

} // namespace tonerig
