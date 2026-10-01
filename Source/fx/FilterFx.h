#pragma once

#include "FxTypes.h"
#include "Parameters.h"
#include "dsp/LadderFilter.h"
#include "dsp/Saturation.h"

#include <atomic>
#include <cmath>

namespace sonder
{

// Фильтр в рэке: тот же ladder, что в голосе, со своим LFO на срезе.
// LFO синхронизируется с темпом, правый канал можно сдвинуть по фазе (Spread) - срез "ходит" по стерео.
class FilterFx
{
public:
    static LadderFilter::Mode modeFromParameter (float value) noexcept
    {
        using M = LadderFilter::Mode;
        static constexpr M modes[] { M::lowpass24, M::lowpass12, M::highpass24, M::highpass12, M::bandpass24, M::bandpass12, M::notch };
        return modes[juce::jlimit (0, (int) std::size (modes) - 1, juce::roundToInt (value))];
    }

    // Формы LFO: синус, треугольник, пила вверх, пила вниз, меандр; значение от -1 до 1
    static float lfoValue (int shape, float phase) noexcept
    {
        phase -= std::floor (phase);

        switch (shape)
        {
            case 1:  return 1.0f - 4.0f * std::abs (phase - 0.5f);
            case 2:  return 2.0f * phase - 1.0f;
            case 3:  return 1.0f - 2.0f * phase;
            case 4:  return phase < 0.5f ? 1.0f : -1.0f;
            default: return fastSinCycles (phase);
        }
    }

    static float rateHz (const float* p, double bpm) noexcept
    {
        const double beats = syncDivisionInBeats (juce::roundToInt (p[fxp::filter::sync]));
        return beats > 0.0 ? (float) (juce::jmax (20.0, bpm) / 60.0 / beats) : p[fxp::filter::rate];
    }

    // Срез при данном значении LFO: Depth - в октавах в каждую сторону
    static float cutoffFor (const float* p, float lfo) noexcept
    {
        return juce::jlimit (20.0f, 20000.0f, p[fxp::filter::cutoff] * std::exp2 (p[fxp::filter::depth] * lfo));
    }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = (float) newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        left.reset();
        right.reset();
        phase = 0.0f;
        controlCounter = 0;
    }

    void process (float* leftChannel, float* rightChannel, int numSamples, const float* p, double bpm) noexcept
    {
        const auto mode = modeFromParameter (p[fxp::filter::mode]);
        const float resonance = p[fxp::filter::resonance];
        // Без драйва фильтр в полосе пропускания не меняет громкость
        const float driveGain = std::exp2 (2.5f * p[fxp::filter::drive]);
        const float makeup = 1.0f / std::sqrt (driveGain);
        const int shape = juce::roundToInt (p[fxp::filter::shape]);
        const float spread = 0.5f * p[fxp::filter::spread];
        const float mix = p[fxp::filter::mix];
        const float increment = rateHz (p, bpm) / sampleRate;
        const float piOverSampleRate = 3.14159265f / sampleRate;
        const float maxCutoff = 0.45f * sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            // Коэффициенты раз в 16 сэмплов: LFO медленный, а tan не бесплатный
            if (--controlCounter <= 0)
            {
                controlCounter = 16;
                const float cutoffLeft = cutoffFor (p, lfoValue (shape, phase));
                const float cutoffRight = cutoffFor (p, lfoValue (shape, phase + spread));
                coefficientsLeft = LadderFilter::makeCoefficients (cutoffLeft, resonance, mode, piOverSampleRate, maxCutoff);
                coefficientsRight = LadderFilter::makeCoefficients (cutoffRight, resonance, mode, piOverSampleRate, maxCutoff);
                displayCutoff.store (cutoffLeft);
            }

            const float dryLeft = leftChannel[i], dryRight = rightChannel[i];
            const float wetLeft = left.process (dryLeft, coefficientsLeft, driveGain) * makeup;
            const float wetRight = right.process (dryRight, coefficientsRight, driveGain) * makeup;

            leftChannel[i] = dryLeft + (wetLeft - dryLeft) * mix;
            rightChannel[i] = dryRight + (wetRight - dryRight) * mix;

            phase += increment;
            if (phase >= 1.0f)
                phase -= 1.0f;
        }

        displayPhase.store (phase);
    }

    // Для экрана
    float getDisplayCutoff() const noexcept { return displayCutoff.load(); }
    float getDisplayPhase() const noexcept  { return displayPhase.load(); }

private:
    LadderFilter left, right;
    LadderFilter::Coefficients coefficientsLeft, coefficientsRight;
    float sampleRate = 44100.0f;
    float phase = 0.0f;
    int controlCounter = 0;
    std::atomic<float> displayCutoff { 1000.0f }, displayPhase { 0.0f };
};

} // namespace sonder
