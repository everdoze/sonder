#pragma once

#include "Saturation.h"

#include <algorithm>
#include <cmath>

namespace sonder
{

// Осциллятор с PolyBLEP-коррекцией разрывов: пила и пульс без жёсткого алиасинга.
// Фаза не сбрасывается при нажатии ноты: как у аналогового VCO, он "бежит" всегда.
// Попутно формирует суб-осциллятор: меандр на октаву ниже, переключаемый на каждом цикле.
class PolyBlepOscillator
{
public:
    enum class Shape { saw, pulse, triangle, sine };

    void setSampleRate (double sampleRate) noexcept { invSampleRate = (float) (1.0 / sampleRate); }

    void setPhase (float newPhase) noexcept { phase = newPhase - std::floor (newPhase); }

    float getPhase() const noexcept { return phase; }

    float process (float frequencyHz, Shape shape, float pulseWidth) noexcept
    {
        const float dt = std::clamp (frequencyHz * invSampleRate, 0.0f, 0.5f);
        float out = 0.0f;

        switch (shape)
        {
            case Shape::saw:
                out = 2.0f * phase - 1.0f - polyBlep (phase, dt);
                break;

            case Shape::pulse:
            {
                out = phase < pulseWidth ? 1.0f : -1.0f;
                out += polyBlep (phase, dt);

                float fallPhase = phase - pulseWidth;
                if (fallPhase < 0.0f)
                    fallPhase += 1.0f;

                out -= polyBlep (fallPhase, dt);

                // Аналоговый выход развязан по постоянному току: убираем DC от несимметричного пульса
                out -= 2.0f * pulseWidth - 1.0f;
                break;
            }

            case Shape::triangle:
                // Спектр треугольника спадает на 12 дБ/окт, при передискретизации алиасинг незаметен
                out = 1.0f - 4.0f * std::abs (phase - 0.5f);
                break;

            case Shape::sine:
                out = fastSinCycles (phase);
                break;
        }

        // Суб: скачок на границе цикла, сглаживается тем же PolyBLEP
        subOut = subState;
        if (phase < dt)
            subOut += subState * polyBlep (phase, dt);
        else if (phase > 1.0f - dt)
            subOut -= subState * polyBlep (phase, dt);

        phase += dt;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            subState = -subState;
        }

        return out;
    }

    // Значение суб-осциллятора для последнего вызова process()
    float getSub() const noexcept { return subOut; }

private:
    static float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }

        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }

        return 0.0f;
    }

    float phase = 0.0f;
    float subState = 1.0f, subOut = 0.0f;
    float invSampleRate = 1.0f / 44100.0f;
};

} // namespace sonder
