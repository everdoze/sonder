#pragma once

#include "FastRandom.h"
#include "SmoothNoise.h"

#include <algorithm>
#include <cmath>

namespace sonder
{

// Источник шума с разными "цветами". Уровни подобраны так, чтобы типы были сопоставимы по громкости.
class NoiseGenerator
{
public:
    // Порядок совпадает с Choices::noiseTypes()
    enum class Type { white, pink, brown, crackle, hiss, digital };

    void prepare (double newSampleRate, uint32_t seed) noexcept
    {
        sampleRate = (float) newSampleRate;
        rng.setSeed (seed);
        flutter.prepare (newSampleRate, 0.7f, seed ^ 0x51f1u, 32);
        hissCoef = 1.0f - std::exp (-2.0f * 3.14159265f * 4000.0f / sampleRate);
        popDecay = std::exp (-1.0f / (0.0004f * sampleRate));
        pink0 = pink1 = pink2 = brown = hissLow = pop = held = 0.0f;
        holdCounter = 0.0f;
    }

    static constexpr int kNumTypes = 6;

    // Плавный переход между соседними видами шума: position от 0 (white) до kNumTypes - 1 (digital).
    // Между видами звучат оба сразу; источники у них независимые, поэтому смешиваем с равной мощностью.
    float processMorph (float position, float noteFrequency) noexcept
    {
        position = std::fmin (std::fmax (position, 0.0f), (float) (kNumTypes - 1));
        const int lower = std::min (kNumTypes - 2, (int) position);
        const float fraction = position - (float) lower;

        if (fraction < 0.002f)
            return process (static_cast<Type> (lower), noteFrequency);

        if (fraction > 0.998f)
            return process (static_cast<Type> (lower + 1), noteFrequency);

        return process (static_cast<Type> (lower), noteFrequency) * std::sqrt (1.0f - fraction)
             + process (static_cast<Type> (lower + 1), noteFrequency) * std::sqrt (fraction);
    }

    // noteFrequency нужна "цифровому" шуму: его частота обновления следует за нотой
    float process (Type type, float noteFrequency) noexcept
    {
        const float white = rng.nextBipolar();

        switch (type)
        {
            case Type::white:
                return white;

            case Type::pink:
            {
                // Фильтр Пола Келлета: спад 3 дБ/окт
                pink0 = 0.99765f * pink0 + white * 0.0990460f;
                pink1 = 0.96300f * pink1 + white * 0.2965164f;
                pink2 = 0.57000f * pink2 + white * 1.0526913f;
                return (pink0 + pink1 + pink2 + white * 0.1848f) * 0.35f;
            }

            case Type::brown:
                // Утекающий интегратор: спад 6 дБ/окт, глухой гул
                brown = (brown + 0.02f * white) / 1.02f;
                return brown * 6.0f;

            case Type::crackle:
            {
                // Редкие щелчки винила поверх едва слышного шипения
                if (rng.nextFloat() < 45.0f / sampleRate)
                    pop = rng.nextBipolar() * (0.5f + 0.5f * rng.nextFloat()) * 3.0f;

                const float out = pop + white * 0.03f;
                pop *= popDecay;
                return out;
            }

            case Type::hiss:
            {
                // Шипение ленты: только верх спектра, уровень слегка "дышит"
                hissLow += (white - hissLow) * hissCoef;
                return (white - hissLow) * (1.0f + 0.3f * flutter.next()) * 1.2f;
            }

            case Type::digital:
            {
                // Шум старых чипов: значение держится несколько сэмплов, частота обновления привязана к ноте
                holdCounter -= 1.0f;
                if (holdCounter <= 0.0f)
                {
                    holdCounter += std::fmax (1.0f, sampleRate / std::fmax (20.0f, noteFrequency * 16.0f));
                    held = std::round (white * 8.0f) / 8.0f;
                }

                return held;
            }
        }

        return white;
    }

private:
    FastRandom rng;
    SmoothNoise flutter;
    float sampleRate = 44100.0f;
    float pink0 = 0.0f, pink1 = 0.0f, pink2 = 0.0f, brown = 0.0f;
    float hissLow = 0.0f, hissCoef = 0.5f;
    float pop = 0.0f, popDecay = 0.9f;
    float held = 0.0f, holdCounter = 0.0f;
};

} // namespace sonder
