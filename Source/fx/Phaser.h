#pragma once

#include "FxTypes.h"
#include "dsp/Saturation.h"

#include <array>
#include <atomic>
#include <cmath>

namespace sonder
{

// Фейзер: цепочка фазовращателей первого порядка с обратной связью.
// Число ступеней выбирается (4-12), LFO каналов можно развести по фазе ручкой Spread.
class Phaser
{
public:
    static constexpr int kMaxStages = 12;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = (float) newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        channels = {};
        lfoPhase = 0.0f;
        controlCounter = 0;
    }

    static int stagesFromParameter (float value) noexcept
    {
        static constexpr int stages[] { 4, 6, 8, 12 };
        return stages[juce::jlimit (0, 3, juce::roundToInt (value))];
    }

    // Частота провалов как функция значения LFO: качается на ±2.25 октавы вокруг Centre
    static float sweepFrequency (float lfoValue, float depth, float centre) noexcept
    {
        return centre * std::exp2 (2.25f * depth * lfoValue);
    }

    void process (float* left, float* right, int numSamples, const float* p) noexcept
    {
        const float depth = p[fxp::phaser::depth];
        const float feedback = p[fxp::phaser::feedback];
        const float centre = p[fxp::phaser::centre];
        const float mix = p[fxp::phaser::mix];
        const float spread = 0.5f * p[fxp::phaser::spread]; // сдвиг фазы LFO правого канала, до полупериода
        const int stages = stagesFromParameter (p[fxp::phaser::stages]);
        const float phaseIncrement = p[fxp::phaser::rate] / sampleRate;
        float* io[2] { left, right };

        for (int i = 0; i < numSamples; ++i)
        {
            // Коэффициенты фазовращателей обновляются раз в 16 сэмплов: LFO медленный
            if (--controlCounter <= 0)
            {
                controlCounter = 16;
                for (size_t ch = 0; ch < 2; ++ch)
                {
                    const float lfo = fastSinCycles (lfoPhase + spread * (float) ch);
                    const float frequency = sweepFrequency (lfo, depth, centre);
                    const float t = std::tan (3.14159265f * std::fmin (frequency, 0.45f * sampleRate) / sampleRate);
                    channels[ch].coefficient = (t - 1.0f) / (t + 1.0f);

                    if (ch == 0)
                        displayFrequency.store (frequency);
                }
            }

            for (size_t ch = 0; ch < 2; ++ch)
            {
                auto& channel = channels[ch];
                const float dry = io[ch][i];
                float x = dry + feedback * channel.feedback;

                for (int s = 0; s < stages; ++s)
                {
                    auto& z = channel.state[(size_t) s];
                    const float y = channel.coefficient * x + z;
                    z = x - channel.coefficient * y;
                    x = y;
                }

                channel.feedback = x;

                // Провалы глубже всего, когда сухой и обработанный сигналы равны
                io[ch][i] = dry + 0.5f * mix * (x - dry);
            }

            lfoPhase += phaseIncrement;
            if (lfoPhase >= 1.0f)
                lfoPhase -= 1.0f;
        }
    }

    // Текущая частота для визуализации
    float getDisplayFrequency() const noexcept { return displayFrequency.load(); }

private:
    struct Channel
    {
        std::array<float, kMaxStages> state {};
        float feedback = 0.0f, coefficient = 0.0f;
    };

    std::array<Channel, 2> channels {};
    float sampleRate = 44100.0f;
    float lfoPhase = 0.0f;
    int controlCounter = 0;
    std::atomic<float> displayFrequency { 950.0f };
};

} // namespace sonder
