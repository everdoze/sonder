#pragma once

#include "Saturation.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace sonder
{

// 4-полюсный ladder-фильтр (Moog) на TPT/ZDF-звеньях (Zavalishin, "The Art of VA Filter Design").
// Мгновенная обратная связь решается линейно, затем вход петли проходит через tanh:
// резонанс насыщается, "сжимает" басы и может самовозбуждаться, не уходя в бесконечность.
// Режимы кроме LP24 получаются смешиванием выходов звеньев (как в Oberheim Xpander).
class LadderFilter
{
public:
    enum class Mode { lowpass24, lowpass12, bandpass12, highpass24, highpass12, bandpass24, notch };

    // Выход как смесь входа петли (a) и выходов четырёх звеньев (b..e), плюс компенсация резонанса
    struct Mix
    {
        float a, b, c, d, e, resonanceCompensation;
    };

    static constexpr Mix mixFor (Mode mode) noexcept
    {
        switch (mode)
        {
            case Mode::lowpass24:  return { 0.0f,  0.0f, 0.0f,  0.0f, 1.0f, 0.3f };
            case Mode::lowpass12:  return { 0.0f,  0.0f, 1.0f,  0.0f, 0.0f, 0.2f };
            case Mode::bandpass12: return { 0.0f,  2.0f, -2.0f, 0.0f, 0.0f, 0.0f };
            case Mode::highpass24: return { 1.0f, -4.0f, 6.0f, -4.0f, 1.0f, 0.0f };
            case Mode::highpass12: return { 1.0f, -2.0f, 1.0f,  0.0f, 0.0f, 0.0f };
            case Mode::bandpass24: return { 0.0f,  0.0f, 4.0f, -8.0f, 4.0f, 0.0f };
            case Mode::notch:      return { 1.0f, -2.0f, 2.0f,  0.0f, 0.0f, 0.0f };
        }

        return { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f };
    }

    // Всё, что не зависит от сигнала, считается здесь заранее: в самом фильтре остаются умножения
    // и одно деление внутри tanh
    struct Coefficients
    {
        float G = 0.0f, k = 0.0f;
        float G4 = 0.0f, feedbackNorm = 1.0f;             // 1 / (1 + k G^4)
        float w1 = 0.0f, w2 = 0.0f, w3 = 0.0f, w4 = 1.0f; // вклад состояний звеньев в оценку y4
        Mix mix { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };   // уже умножена на компенсацию резонанса
    };

    // Коэффициенты зависят только от среза, резонанса и режима, поэтому стерео-пара фильтров считает их один раз
    static Coefficients makeCoefficients (float cutoffHz, float resonance, Mode mode, float piOverSampleRate, float maxCutoff) noexcept
    {
        const float fc = std::clamp (cutoffHz, 10.0f, maxCutoff);
        const float g = std::tan (piOverSampleRate * fc);
        const float G = g / (1.0f + g);
        const float beta = 1.0f / (1.0f + g);

        Coefficients c;
        c.G = G;
        c.k = 4.5f * resonance;
        c.G4 = G * G * G * G;
        c.feedbackNorm = 1.0f / (1.0f + c.k * c.G4);
        c.w1 = beta * G * G * G;
        c.w2 = beta * G * G;
        c.w3 = beta * G;
        c.w4 = beta;

        // Частичная компенсация падения басов на высоком резонансе у LP делается на выходе:
        // если усилить вход, tanh насытится сильнее и "съест" резонанс
        const auto m = mixFor (mode);
        const float compensation = 1.0f + m.resonanceCompensation * c.k;
        c.mix = { m.a * compensation, m.b * compensation, m.c * compensation, m.d * compensation, m.e * compensation, 0.0f };
        return c;
    }

    void reset() noexcept { s1 = s2 = s3 = s4 = 0.0f; }

    // Линейная модель (без насыщения) для экранов: комплексный коэффициент передачи на частоте frequency
    static std::complex<float> response (float frequency, float cutoff, float resonance, Mode mode, float sampleRate) noexcept
    {
        using Complex = std::complex<float>;
        const float pi = 3.14159265f;
        const float g = std::tan (pi * std::fmin (cutoff, 0.45f * sampleRate) / sampleRate);
        const float omega = std::tan (pi * std::fmin (frequency, 0.49f * sampleRate) / sampleRate) / g;
        const Complex h1 = 1.0f / Complex (1.0f, omega);
        const float k = 4.5f * resonance;
        const Complex h2 = h1 * h1, h3 = h2 * h1, h4 = h2 * h2;

        const auto m = mixFor (mode);
        const Complex mixed = m.a + m.b * h1 + m.c * h2 + m.d * h3 + m.e * h4;
        return mixed / (1.0f + k * h4) * (1.0f + m.resonanceCompensation * k);
    }

    // inputGain: усиление перед нелинейностью (драйв)
    float process (float x, const Coefficients& c, float inputGain) noexcept
    {
        const float G = c.G;
        const float in = x * inputGain;

        // y4 = G^4 * u + sigma, u = in - k * y4  =>  решаем относительно y4
        const float sigma = c.w1 * s1 + c.w2 * s2 + c.w3 * s3 + c.w4 * s4;
        const float y4Estimate = (c.G4 * in + sigma) * c.feedbackNorm;

        const float u = fastTanh (in - c.k * y4Estimate);

        const float y1 = onePole (u, s1, G);
        const float y2 = onePole (y1, s2, G);
        const float y3 = onePole (y2, s3, G);
        const float y4 = onePole (y3, s4, G);

        const auto& m = c.mix;
        return m.a * u + m.b * y1 + m.c * y2 + m.d * y3 + m.e * y4;
    }

private:
    static float onePole (float x, float& s, float G) noexcept
    {
        const float v = (x - s) * G;
        const float y = v + s;
        s = y + v;
        return y;
    }

    float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;
};

} // namespace sonder
