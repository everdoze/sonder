#pragma once

#include "Saturation.h"

#include <cmath>

namespace sonder
{

// Дисторшн после фильтра, по одному на канал голоса
class Distortion
{
public:
    // Порядок совпадает с Choices::distTypes()
    enum class Type { off, tube, hard, fold, crush };

    void prepare (double sampleRate) noexcept
    {
        dcCoef = (float) (1.0 - std::exp (-2.0 * 3.14159265358979 * 20.0 / sampleRate));
        reset();
    }

    void reset() noexcept
    {
        dcState = toneState = held = 0.0f;
        holdCounter = 0;
    }

    struct Settings
    {
        Type type = Type::off;
        float drive = 0.0f, gain = 1.0f, compensation = 1.0f, mix = 0.0f, toneCoef = 1.0f;
        float crushLevels = 4096.0f;
        int crushHold = 1;
    };

    // drive 0..1, mix 0..1, toneCoef - коэффициент ФНЧ после искажения (1 = без фильтра)
    static Settings makeSettings (Type type, float drive, float mix, float toneCoef) noexcept
    {
        Settings s;
        s.type = type;
        s.drive = drive;
        s.gain = 1.0f + 29.0f * drive * drive;
        // Примерная компенсация громкости: сильнее драйв - тише, чтобы звук не "взрывался"
        s.compensation = std::pow (s.gain, -0.275f);
        s.mix = mix;
        s.toneCoef = toneCoef;
        s.crushLevels = std::exp2 (12.0f - 10.0f * drive);
        s.crushHold = 1 + (int) (drive * drive * 24.0f);
        return s;
    }

    float process (float x, const Settings& s) noexcept
    {
        if (s.type == Type::off || s.mix <= 0.0f)
            return x;

        float y = 0.0f;

        switch (s.type)
        {
            case Type::tube:
            {
                // Смещение рабочей точки даёт чётные гармоники, как у лампы
                constexpr float bias = 0.25f;
                y = (fastTanh (x * s.gain + bias) - fastTanh (bias)) * s.compensation;
                break;
            }

            case Type::hard:
                y = std::fmin (std::fmax (x * s.gain, -1.0f), 1.0f) * s.compensation;
                break;

            case Type::fold:
                y = fastSinCycles (0.25f * x * s.gain) * s.compensation;
                break;

            case Type::crush:
            {
                // Понижение разрядности и частоты дискретизации
                if (--holdCounter <= 0)
                {
                    holdCounter = s.crushHold;
                    held = std::round (x * s.crushLevels) / s.crushLevels;
                }

                y = held;
                break;
            }

            case Type::off:
                break;
        }

        // Асимметричные кривые рождают постоянную составляющую - убираем
        dcState += (y - dcState) * dcCoef;
        y -= dcState;

        toneState += (y - toneState) * s.toneCoef;
        y = toneState;

        return x + (y - x) * s.mix;
    }

private:
    float dcCoef = 0.001f;
    float dcState = 0.0f, toneState = 0.0f, held = 0.0f;
    int holdCounter = 0;
};

} // namespace sonder
