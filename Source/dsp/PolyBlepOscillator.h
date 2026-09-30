#pragma once

#include "Saturation.h"
#include "Wavetable.h"

#include <algorithm>
#include <cmath>

namespace sonder
{

// Осциллятор с PolyBLEP-коррекцией разрывов: пила и пульс без жёсткого алиасинга.
// Фаза не сбрасывается при нажатии ноты: как у аналогового VCO, он "бежит" всегда.
// Попутно формирует суб-осциллятор: меандр на октаву ниже, переключаемый на каждом цикле.
// Частота может быть отрицательной (through-zero FM): тогда фаза идёт назад.
// Hard sync: ведомый осциллятор сбрасывает фазу, когда ведущий проходит конец цикла (processSynced).
class PolyBlepOscillator
{
public:
    // Порядок совпадает с Choices::oscShapes()
    enum class Shape { saw, pulse, triangle, sine, wavetable };

    void setSampleRate (double sampleRate) noexcept { invSampleRate = (float) (1.0 / sampleRate); }

    void setPhase (float newPhase) noexcept { phase = newPhase - std::floor (newPhase); }

    float getPhase() const noexcept { return phase; }

    float process (float frequencyHz, Shape shape, float pulseWidth) noexcept
    {
        const float dt = std::clamp (frequencyHz * invSampleRate, -0.5f, 0.5f);
        const float out = shapeValue (shape, pulseWidth, dt);
        advance (dt);
        return out;
    }

    // Чтение wavetable с той же фазой (суб-осциллятор и FM работают так же)
    float processWavetable (float frequencyHz, const Wavetable& table, float position, Wavetable::MipSelection mip) noexcept
    {
        const float dt = std::clamp (frequencyHz * invSampleRate, -0.5f, 0.5f);
        const float out = table.sample (phase, position, mip);
        advance (dt);
        return out;
    }

    // Классические формы из таблиц с ограниченным спектром (Wavetables::classic*): алиасинга заметно меньше,
    // чем у PolyBLEP, синус без искажений быстрого приближения. Пульс - разность двух пил, сдвинутых на ширину.
    float processClassic (float frequencyHz, Shape shape, float pulseWidth, Wavetable::MipSelection mip) noexcept
    {
        const float dt = std::clamp (frequencyHz * invSampleRate, -0.5f, 0.5f);
        float out = 0.0f;

        switch (shape)
        {
            case Shape::saw:
                out = Wavetables::classicSaw().sampleSingle (phase, mip);
                break;

            case Shape::pulse:
            {
                float shifted = phase - pulseWidth;
                shifted -= std::floor (shifted);
                const auto& saw = Wavetables::classicSaw();
                out = saw.sampleSingle (shifted, mip) - saw.sampleSingle (phase, mip);
                break;
            }

            case Shape::triangle:  out = Wavetables::classicTriangle().sampleSingle (phase, mip); break;
            case Shape::sine:      out = Wavetables::classicSine().sampleSingle (phase, {}); break;
            case Shape::wavetable: break;
        }

        advance (dt);
        return out;
    }

    // Ведомый осциллятор. wrap - getWrapFraction() ведущего после его шага на этом же сэмпле:
    // сброс фазы приходится между этим сэмплом и следующим. Скачок формы в момент сброса сглаживается
    // PolyBLEP на обоих соседних сэмплах. table != nullptr - wavetable.
    float processSynced (float frequencyHz, Shape shape, float pulseWidth, float wrap,
                         const Wavetable* table, float position, Wavetable::MipSelection mip) noexcept
    {
        const float dt = std::clamp (frequencyHz * invSampleRate, 0.0f, 0.5f);
        const auto naive = [&] (float at)
        {
            return table != nullptr ? table->sample (at, position, mip) : naiveValue (shape, at, pulseWidth);
        };

        float out = 0.0f;

        if (justSynced || wrap >= 0.0f)
        {
            // Рядом со сбросом собственная поправка формы не годится: она рассчитана на обычный конец цикла
            out = naive (phase) + pendingCorrection;
            justSynced = false;
            pendingCorrection = 0.0f;
        }
        else
        {
            out = table != nullptr ? table->sample (phase, position, mip) : shapeValue (shape, pulseWidth, dt);
        }

        if (wrap < 0.0f)
        {
            advance (dt);
            return out;
        }

        // Фаза в момент сброса и скачок формы, который он даёт
        float before = phase + dt * (1.0f - wrap);
        before -= std::floor (before);
        const float step = naive (0.0f) - naive (before);

        out += 0.5f * step * wrap * wrap;
        pendingCorrection = -0.5f * step * (1.0f - wrap) * (1.0f - wrap);
        justSynced = true;

        phase = dt * wrap;
        wrapFraction = -1.0f;
        return out;
    }

    // Значение суб-осциллятора для последнего вызова process()
    float getSub() const noexcept { return subOut; }

    // Для ведущего в hard sync: прошёл ли последний шаг через конец цикла и где именно.
    // Доля периода сэмпла от сброса до следующего сэмпла (0..1) или -1, если цикл не закончился.
    float getWrapFraction() const noexcept { return wrapFraction; }

private:
    // Значение формы в текущей фазе с поправкой разрывов
    float shapeValue (Shape shape, float pulseWidth, float dt) const noexcept
    {
        // Сглаженный разрыв как функция фазы симметричен, поэтому при обратном ходе
        // годится та же поправка с шириной |dt|
        const float width = std::abs (dt);

        switch (shape)
        {
            case Shape::saw:
                return 2.0f * phase - 1.0f - polyBlep (phase, width);

            case Shape::pulse:
            {
                float out = phase < pulseWidth ? 1.0f : -1.0f;
                out += polyBlep (phase, width);

                float fallPhase = phase - pulseWidth;
                if (fallPhase < 0.0f)
                    fallPhase += 1.0f;

                out -= polyBlep (fallPhase, width);

                // Аналоговый выход развязан по постоянному току: убираем DC от несимметричного пульса
                return out - (2.0f * pulseWidth - 1.0f);
            }

            case Shape::triangle:
                // Спектр треугольника спадает на 12 дБ/окт, при передискретизации алиасинг незаметен
                return 1.0f - 4.0f * std::abs (phase - 0.5f);

            case Shape::sine:
                return fastSinCycles (phase);

            case Shape::wavetable:
                break;
        }

        return 0.0f;
    }

    // Форма без поправок (для величины скачка при синке)
    static float naiveValue (Shape shape, float at, float pulseWidth) noexcept
    {
        switch (shape)
        {
            case Shape::saw:       return 2.0f * at - 1.0f;
            case Shape::pulse:     return (at < pulseWidth ? 1.0f : -1.0f) - (2.0f * pulseWidth - 1.0f);
            case Shape::triangle:  return 1.0f - 4.0f * std::abs (at - 0.5f);
            case Shape::sine:      return fastSinCycles (at);
            case Shape::wavetable: break;
        }

        return 0.0f;
    }

    void advance (float dt) noexcept
    {
        // Суб: скачок на границе цикла, сглаживается тем же PolyBLEP
        const float width = std::abs (dt);
        subOut = subState;
        if (phase < width)
            subOut += subState * polyBlep (phase, width);
        else if (phase > 1.0f - width)
            subOut -= subState * polyBlep (phase, width);

        phase += dt;
        wrapFraction = -1.0f;

        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            subState = -subState;
            wrapFraction = width > 0.0f ? std::min (phase / width, 0.999999f) : 0.0f;
        }
        else if (phase < 0.0f)
        {
            phase += 1.0f;
            subState = -subState;
            wrapFraction = width > 0.0f ? std::min ((1.0f - phase) / width, 0.999999f) : 0.0f;
        }
    }

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
    float wrapFraction = -1.0f;
    float pendingCorrection = 0.0f;
    bool justSynced = false;
    float invSampleRate = 1.0f / 44100.0f;
};

} // namespace sonder
