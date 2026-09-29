#pragma once

#include "FastRandom.h"
#include "Saturation.h"

#include <cmath>

namespace sonder
{

class Lfo
{
public:
    enum class Shape { sine, triangle, sawUp, sawDown, square, sampleAndHold, smoothRandom };

    void prepare (double newSampleRate, uint32_t seed) noexcept
    {
        invSampleRate = (float) (1.0 / newSampleRate);
        rng.setSeed (seed);
        previousRandom = rng.nextBipolar();
        nextRandom = rng.nextBipolar();
    }

    void setPhase (float newPhase) noexcept { phase = newPhase - std::floor (newPhase); }
    float getPhase() const noexcept { return phase; }

    // Значение в [-1, 1]
    float process (float rateHz, Shape shape) noexcept
    {
        float out = 0.0f;

        switch (shape)
        {
            case Shape::sine:          out = fastSinCycles (phase); break;
            case Shape::triangle:      out = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
            case Shape::sawUp:         out = 2.0f * phase - 1.0f; break;
            case Shape::sawDown:       out = 1.0f - 2.0f * phase; break;
            case Shape::square:        out = phase < 0.5f ? 1.0f : -1.0f; break;
            case Shape::sampleAndHold: out = previousRandom; break;
            case Shape::smoothRandom:
            {
                // Косинусная интерполяция между случайными точками
                const float t = 0.5f - 0.5f * fastSinCycles (0.5f * phase + 0.25f);
                out = previousRandom + (nextRandom - previousRandom) * t;
                break;
            }
        }

        phase += rateHz * invSampleRate;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            previousRandom = nextRandom;
            nextRandom = rng.nextBipolar();
        }

        return out;
    }

private:
    FastRandom rng;
    float phase = 0.0f;
    float invSampleRate = 1.0f / 44100.0f;
    float previousRandom = 0.0f, nextRandom = 0.0f;
};

} // namespace sonder
