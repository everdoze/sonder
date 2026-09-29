#pragma once

#include "dsp/FastRandom.h"

#include <cmath>
#include <vector>

namespace sonder
{

// Хорус в духе Juno-60: одна BBD-линия на канал, треугольный LFO в противофазе,
// тёмный фильтр на мокром сигнале и едва слышное шипение "вёдер".
class Chorus
{
public:
    enum class Mode { off, one, two, both };

    void prepare (double newSampleRate)
    {
        sampleRate = (float) newSampleRate;

        size_t size = 1;
        while (size < (size_t) (newSampleRate * 0.02))
            size <<= 1;

        buffer.assign (size, 0.0f);
        mask = (int) size - 1;
        writeIndex = 0;
        lfoPhase = 0.0f;
        lowpassLeft = lowpassRight = 0.0f;
        currentMix = 0.0f;
        lowpassCoef = 1.0f - std::exp (-2.0f * 3.14159265f * 8000.0f / sampleRate);
    }

    void process (float* left, float* right, int numSamples, Mode mode, float mix) noexcept
    {
        float rateHz = 0.513f, centreMs = 3.5f, depthMs = 1.85f;

        if (mode == Mode::two)
            rateHz = 0.863f;
        else if (mode == Mode::both)
        {
            rateHz = 9.75f;
            depthMs = 0.2f;
        }

        const float targetMix = mode == Mode::off ? 0.0f : mix;
        const float mixCoef = 1.0f - std::exp (-1.0f / (0.02f * sampleRate));
        const float msToSamples = sampleRate * 0.001f;
        const float phaseIncrement = rateHz / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = 0.5f * (left[i] + right[i]);
            buffer[(size_t) writeIndex] = dry + noise.nextBipolar() * 2.0e-5f;

            currentMix += (targetMix - currentMix) * mixCoef;

            if (currentMix > 1.0e-4f)
            {
                const float triangle = 1.0f - 4.0f * std::abs (lfoPhase - 0.5f);
                const float wetLeft  = read ((centreMs + depthMs * triangle) * msToSamples);
                const float wetRight = read ((centreMs - depthMs * triangle) * msToSamples);

                lowpassLeft  += (wetLeft - lowpassLeft) * lowpassCoef;
                lowpassRight += (wetRight - lowpassRight) * lowpassCoef;

                left[i]  += currentMix * (0.65f * lowpassLeft - 0.35f * left[i]);
                right[i] += currentMix * (0.65f * lowpassRight - 0.35f * right[i]);
            }

            lfoPhase += phaseIncrement;
            if (lfoPhase >= 1.0f)
                lfoPhase -= 1.0f;

            writeIndex = (writeIndex + 1) & mask;
        }
    }

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
    float lowpassLeft = 0.0f, lowpassRight = 0.0f, lowpassCoef = 1.0f;
    float currentMix = 0.0f;
    FastRandom noise { 0x0c0ffee1u };
};

} // namespace sonder
