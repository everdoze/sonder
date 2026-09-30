#pragma once

#include "FxTypes.h"
#include "dsp/FastRandom.h"

#include <atomic>
#include <cmath>
#include <vector>

namespace sonder
{

// Хорус в духе Juno-60: BBD-линия, треугольный LFO в противофазе по каналам,
// тёмный фильтр на мокром сигнале и едва слышное шипение "вёдер".
// Режимы I, II и I+II повторяют оригинал; в режиме Free скорость и глубина задаются ручками.
class Chorus
{
public:
    enum class Mode { one, two, both, free };

    struct Motion
    {
        float rateHz, depthMs;
    };

    // Скорость и глубина качания задержки для режима
    static Motion motionFor (Mode mode, float freeRate, float freeDepth) noexcept
    {
        switch (mode)
        {
            case Mode::one:  return { 0.513f, 1.85f };
            case Mode::two:  return { 0.863f, 1.85f };
            case Mode::both: return { 9.75f, 0.2f };
            case Mode::free: return { freeRate, 3.0f * freeDepth };
        }

        return { 0.513f, 1.85f };
    }

    void prepare (double newSampleRate)
    {
        sampleRate = (float) newSampleRate;

        size_t size = 1;
        while (size < (size_t) (newSampleRate * 0.03))
            size <<= 1;

        buffer.assign (size, 0.0f);
        mask = (int) size - 1;
        reset();
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
        lfoPhase = 0.0f;
        lowpassLeft = lowpassRight = 0.0f;
    }

    void process (float* left, float* right, int numSamples, const float* p) noexcept
    {
        const auto mode = static_cast<Mode> (juce::jlimit (0, 3, juce::roundToInt (p[fxp::chorus::mode])));
        const auto motion = motionFor (mode, p[fxp::chorus::rate], p[fxp::chorus::depth]);
        const float mix = p[fxp::chorus::mix];
        const float width = p[fxp::chorus::width];
        const float lowpassCoef = 1.0f - std::exp (-2.0f * 3.14159265f * p[fxp::chorus::tone] / sampleRate);

        constexpr float centreMs = 3.5f;
        const float msToSamples = sampleRate * 0.001f;
        const float phaseIncrement = motion.rateHz / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = 0.5f * (left[i] + right[i]);
            buffer[(size_t) writeIndex] = dry + noise.nextBipolar() * 2.0e-5f;

            // Width = 100%: каналы качаются в противофазе, 0%: одинаково (моно)
            const float triangle = 1.0f - 4.0f * std::abs (lfoPhase - 0.5f);
            const float wetLeft  = read ((centreMs + motion.depthMs * triangle) * msToSamples);
            const float wetRight = read ((centreMs + motion.depthMs * triangle * (1.0f - 2.0f * width)) * msToSamples);

            lowpassLeft  += (wetLeft - lowpassLeft) * lowpassCoef;
            lowpassRight += (wetRight - lowpassRight) * lowpassCoef;

            left[i]  += mix * (0.65f * lowpassLeft - 0.35f * left[i]);
            right[i] += mix * (0.65f * lowpassRight - 0.35f * right[i]);

            lfoPhase += phaseIncrement;
            if (lfoPhase >= 1.0f)
                lfoPhase -= 1.0f;

            writeIndex = (writeIndex + 1) & mask;
        }

        displayPhase.store (lfoPhase);
    }

    // Фаза LFO для визуализации
    float getDisplayPhase() const noexcept { return displayPhase.load(); }

private:
    float read (float delaySamples) const noexcept
    {
        const float position = (float) writeIndex - delaySamples;
        const float floorPosition = std::floor (position);
        const float frac = position - floorPosition;
        const int i0 = (int) floorPosition & mask;
        const int i1 = (i0 + 1) & mask;
        return buffer[(size_t) i0] + (buffer[(size_t) i1] - buffer[(size_t) i0]) * frac;
    }

    std::vector<float> buffer;
    int mask = 0, writeIndex = 0;
    float sampleRate = 44100.0f;
    float lfoPhase = 0.0f;
    float lowpassLeft = 0.0f, lowpassRight = 0.0f;
    std::atomic<float> displayPhase { 0.0f };
    FastRandom noise { 0x0c0ffee1u };
};

} // namespace sonder
