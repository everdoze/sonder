#pragma once

#include "FxTypes.h"

#include <atomic>
#include <cmath>
#include <vector>

namespace sonder
{

// Расширитель стерео в режиме середина/стороны:
//  - Width: сколько сторон (0 - моно, 100% - как было, 200% - вдвое шире);
//  - Spread и Time: к сторонам добавляется задержанная середина - моно-звук тоже раскрывается,
//    а при сведении в моно добавка исчезает (она только в сторонах);
//  - Bass Mono: ниже этой частоты стороны убираются, бас остаётся в центре (20 Гц - выключено).
class Widener
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = (float) newSampleRate;

        size_t size = 1;
        while (size < (size_t) (newSampleRate * 0.035))
            size <<= 1;

        delay.assign (size, 0.0f);
        mask = (int) size - 1;
        reset();
    }

    void reset() noexcept
    {
        std::fill (delay.begin(), delay.end(), 0.0f);
        writeIndex = 0;
        sideLow = 0.0f;
        correlation = 1.0f;
    }

    void process (float* left, float* right, int numSamples, const float* p) noexcept
    {
        const float width = p[fxp::widener::width];
        const float spread = p[fxp::widener::spread];
        const int delaySamples = juce::jlimit (1, mask, juce::roundToInt (p[fxp::widener::time] * 0.001f * sampleRate));
        const float bassMono = p[fxp::widener::bassMono];
        const float lowCoef = bassMono > 21.0f ? 1.0f - std::exp (-6.2831853f * bassMono / sampleRate) : 0.0f;

        double productSum = 0.0, leftSum = 0.0, rightSum = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const float mid = 0.5f * (left[i] + right[i]);
            float side = 0.5f * (left[i] - right[i]) * width;

            delay[(size_t) writeIndex] = mid;
            side += spread * 0.7f * delay[(size_t) ((writeIndex - delaySamples) & mask)];
            writeIndex = (writeIndex + 1) & mask;

            // Стороны без низа: бас остаётся в центре
            if (lowCoef > 0.0f)
            {
                sideLow += (side - sideLow) * lowCoef;
                side -= sideLow;
            }

            left[i] = mid + side;
            right[i] = mid - side;

            productSum += (double) left[i] * right[i];
            leftSum += (double) left[i] * left[i];
            rightSum += (double) right[i] * right[i];
        }

        // Корреляция каналов для экрана: 1 - моно, 0 - независимые, -1 - в противофазе
        if (leftSum > 1.0e-9 && rightSum > 1.0e-9)
        {
            const float blockCorrelation = (float) (productSum / std::sqrt (leftSum * rightSum));
            correlation += (blockCorrelation - correlation) * 0.2f;
        }

        displayCorrelation.store (correlation);
    }

    float getDisplayCorrelation() const noexcept { return displayCorrelation.load(); }

private:
    std::vector<float> delay;
    int mask = 0, writeIndex = 0;
    float sampleRate = 44100.0f;
    float sideLow = 0.0f, correlation = 1.0f;
    std::atomic<float> displayCorrelation { 1.0f };
};

} // namespace sonder
