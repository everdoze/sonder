#pragma once

#include "FxTypes.h"

#include <atomic>
#include <cmath>

namespace sonder
{

// Компрессор: пиковый детектор с общим для стерео управлением, настраиваемое колено,
// фильтр верхних частот в цепи детектора (чтобы бас не "качал" всё остальное)
// и подмешивание необработанного сигнала (параллельная компрессия).
class Compressor
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = (float) newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        reduction = 0.0f;
        detectorLowLeft = detectorLowRight = 0.0f;
    }

    // Сколько дБ убрать для уровня levelDb (статическая характеристика; нужна и для отрисовки кривой)
    static float gainReductionFor (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept
    {
        const float slope = 1.0f - 1.0f / std::fmax (1.0f, ratio);
        const float over = levelDb - thresholdDb;

        if (kneeDb > 0.01f)
        {
            if (over <= -0.5f * kneeDb)
                return 0.0f;

            if (over < 0.5f * kneeDb)
            {
                const float x = over + 0.5f * kneeDb;
                return slope * x * x / (2.0f * kneeDb);
            }
        }

        return over > 0.0f ? slope * over : 0.0f;
    }

    void process (float* left, float* right, int numSamples, const float* p) noexcept
    {
        const float threshold = p[fxp::compressor::threshold];
        const float ratio = p[fxp::compressor::ratio];
        const float knee = p[fxp::compressor::knee];
        const float makeup = p[fxp::compressor::makeup];
        const float mix = p[fxp::compressor::mix];

        const float attackCoef = 1.0f - std::exp (-1.0f / (p[fxp::compressor::attack] * sampleRate));
        const float releaseCoef = 1.0f - std::exp (-1.0f / (p[fxp::compressor::release] * sampleRate));
        const float detectorCoef = 1.0f - std::exp (-2.0f * 3.14159265f * p[fxp::compressor::sidechainHp] / sampleRate);
        float maxReduction = 0.0f, maxInput = -100.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            // Детектор слушает сигнал без низов
            detectorLowLeft += (left[i] - detectorLowLeft) * detectorCoef;
            detectorLowRight += (right[i] - detectorLowRight) * detectorCoef;
            const float peak = std::fmax (std::abs (left[i] - detectorLowLeft), std::abs (right[i] - detectorLowRight));

            const float levelDb = 20.0f * std::log10 (peak + 1.0e-9f);
            const float target = gainReductionFor (levelDb, threshold, ratio, knee);
            reduction += (target - reduction) * (target > reduction ? attackCoef : releaseCoef);

            // 10^(x/20) = exp (x * ln (10) / 20)
            const float gain = std::exp ((makeup - reduction) * 0.115129255f);
            left[i] += mix * (left[i] * gain - left[i]);
            right[i] += mix * (right[i] * gain - right[i]);

            maxReduction = std::fmax (maxReduction, reduction);
            maxInput = std::fmax (maxInput, levelDb);
        }

        displayReduction.store (maxReduction);
        displayInput.store (maxInput);
    }

    float getDisplayReduction() const noexcept { return displayReduction.load(); }
    float getDisplayInput() const noexcept     { return displayInput.load(); }

private:
    float sampleRate = 44100.0f;
    float reduction = 0.0f;
    float detectorLowLeft = 0.0f, detectorLowRight = 0.0f;
    std::atomic<float> displayReduction { 0.0f }, displayInput { -100.0f };
};

} // namespace sonder
