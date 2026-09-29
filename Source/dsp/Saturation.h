#pragma once

#include <cmath>

namespace sonder
{

// Паде-аппроксимация tanh, точно выходит на ±1 при |x| = 3
inline float fastTanh (float x) noexcept
{
    if (x > 3.0f)  return 1.0f;
    if (x < -3.0f) return -1.0f;

    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// Мягкий лимитер: до 0.8 сигнал не трогает, выше плавно подводит к 1.0
inline float softLimit (float x) noexcept
{
    constexpr float knee = 0.8f;
    const float magnitude = std::abs (x);
    if (magnitude <= knee)
        return x;

    const float limited = knee + (1.0f - knee) * fastTanh ((magnitude - knee) / (1.0f - knee));
    return x < 0.0f ? -limited : limited;
}

// sin (2 * pi * cycles), погрешность ~0.1 %
inline float fastSinCycles (float cycles) noexcept
{
    float t = cycles - std::floor (cycles);             // [0, 1)
    const float x = (t < 0.5f ? t : t - 1.0f) * 2.0f;   // [-1, 1) соответствует [-pi, pi)
    const float y = 4.0f * x * (1.0f - std::abs (x));   // парабола
    return 0.225f * (y * std::abs (y) - y) + y;         // уточнение
}

// Синусный вейвфолдер: при росте усиления волна "заворачивается" обратно, рождая новые гармоники
inline float sineFold (float x, float amount) noexcept
{
    if (amount <= 0.0f)
        return x;

    const float folded = fastSinCycles (0.25f * x * (1.0f + 6.0f * amount));
    const float blend = amount * 4.0f < 1.0f ? amount * 4.0f : 1.0f;
    return x + (folded - x) * blend;
}

} // namespace sonder
