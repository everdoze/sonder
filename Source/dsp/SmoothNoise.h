#pragma once

#include "FastRandom.h"

#include <cmath>

namespace sonder
{

// Белый шум через однополюсный ФНЧ, нормированный к единичной дисперсии.
// Медленный вариант (rate ~0.1-1 Гц, controlInterval > 1) даёт дрейф: новое значение шума
// считается раз в controlInterval сэмплов, между точками идёт линейная интерполяция.
// Быстрый вариант (controlInterval = 1) даёт джиттер.
class SmoothNoise
{
public:
    void prepare (double sampleRate, float rateHz, uint32_t seed, int controlInterval) noexcept
    {
        rng.setSeed (seed);
        interval = controlInterval > 0 ? controlInterval : 1;

        const double updateRate = sampleRate / interval;
        coeff = (float) (1.0 - std::exp (-2.0 * 3.14159265358979 * rateHz / updateRate));

        // Дисперсия однополюсного ФНЧ от белого шума: sigma^2 * c / (2 - c),
        // у равномерного шума на [-1, 1] sigma^2 = 1/3
        norm = std::sqrt (3.0f * (2.0f - coeff) / coeff);

        // Стартуем из случайной точки стационарного распределения, а не из нуля,
        // иначе все голоса начинали бы дрейфовать синхронно
        state = (rng.nextBipolar() + rng.nextBipolar() + rng.nextBipolar()) / norm;
        current = target = state * norm;
        step = 0.0f;
        counter = 0;
    }

    // Значение с дисперсией ~1 (примерно гауссово)
    float next() noexcept
    {
        if (--counter <= 0)
        {
            counter = interval;
            state += coeff * (rng.nextBipolar() - state);
            target = state * norm;
            step = (target - current) / (float) interval;
        }

        current += step;
        return current;
    }

private:
    FastRandom rng;
    float coeff = 1.0f, norm = 1.0f;
    float state = 0.0f, current = 0.0f, target = 0.0f, step = 0.0f;
    int interval = 1, counter = 0;
};

} // namespace sonder
