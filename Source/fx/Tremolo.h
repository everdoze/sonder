#pragma once

#include "FxTypes.h"
#include "Parameters.h"
#include "dsp/Saturation.h"

#include <atomic>
#include <cmath>

namespace sonder
{

// Тремоло и автопан: LFO качает громкость (Tremolo) или положение в стерео (Autopan).
// Phase сдвигает LFO правого канала (для тремоло - "стерео-тремоло"), Smooth скругляет углы меандра и пилы.
class Tremolo
{
public:
    // Формы: синус, треугольник, меандр, пила вверх, пила вниз; значение от -1 до 1
    static float shapeValue (int shape, float phase) noexcept
    {
        phase -= std::floor (phase);

        switch (shape)
        {
            case 1:  return 1.0f - 4.0f * std::abs (phase - 0.5f);
            case 2:  return phase < 0.5f ? 1.0f : -1.0f;
            case 3:  return 2.0f * phase - 1.0f;
            case 4:  return 1.0f - 2.0f * phase;
            default: return fastSinCycles (phase);
        }
    }

    static float rateHz (const float* p, double bpm) noexcept
    {
        const double beats = syncDivisionInBeats (juce::roundToInt (p[fxp::tremolo::sync]));
        return beats > 0.0 ? (float) (juce::jmax (20.0, bpm) / 60.0 / beats) : p[fxp::tremolo::rate];
    }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = (float) newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        phase = 0.0f;
        smoothLeft = smoothRight = 0.0f;
    }

    void process (float* left, float* right, int numSamples, const float* p, double bpm) noexcept
    {
        const bool autopan = p[fxp::tremolo::mode] > 0.5f;
        const float depth = p[fxp::tremolo::depth];
        const int shape = juce::roundToInt (p[fxp::tremolo::shape]);
        const float offset = 0.5f * p[fxp::tremolo::phase];
        const float increment = rateHz (p, bpm) / sampleRate;

        // Сглаживание формы: от 0.2 до 25 мс
        const float smoothSeconds = 0.0002f * std::pow (125.0f, p[fxp::tremolo::smooth]);
        const float smoothing = 1.0f - std::exp (-1.0f / (smoothSeconds * sampleRate));

        for (int i = 0; i < numSamples; ++i)
        {
            smoothLeft += (shapeValue (shape, phase) - smoothLeft) * smoothing;
            smoothRight += (shapeValue (shape, phase + offset) - smoothRight) * smoothing;

            if (autopan)
            {
                // Панорама с постоянной мощностью: в центре оба канала без изменений
                const float angle = (juce::jlimit (-1.0f, 1.0f, depth * smoothLeft) + 1.0f) * 0.78539816f;
                left[i] *= std::cos (angle) * 1.41421356f;
                right[i] *= std::sin (angle) * 1.41421356f;
            }
            else
            {
                left[i] *= 1.0f - depth * (0.5f - 0.5f * smoothLeft);
                right[i] *= 1.0f - depth * (0.5f - 0.5f * smoothRight);
            }

            phase += increment;
            if (phase >= 1.0f)
                phase -= 1.0f;
        }

        displayPhase.store (phase);
    }

    float getDisplayPhase() const noexcept { return displayPhase.load(); }

private:
    float sampleRate = 44100.0f;
    float phase = 0.0f, smoothLeft = 0.0f, smoothRight = 0.0f;
    std::atomic<float> displayPhase { 0.0f };
};

} // namespace sonder
