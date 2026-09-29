#pragma once

#include "Saturation.h"

#include <algorithm>
#include <cmath>

namespace sonder
{

// 4-полюсный ladder-фильтр (Moog) на TPT/ZDF-звеньях (Zavalishin, "The Art of VA Filter Design").
// Мгновенная обратная связь решается линейно, затем вход петли проходит через tanh:
// резонанс насыщается, "сжимает" басы и может самовозбуждаться, не уходя в бесконечность.
// Режимы кроме LP24 получаются смешиванием выходов звеньев (как в Oberheim Xpander).
class LadderFilter
{
public:
    enum class Mode { lowpass24, lowpass12, bandpass, highpass };

    struct Coefficients
    {
        float G = 0.0f, beta = 1.0f, k = 0.0f;
    };

    // Коэффициенты зависят только от среза и резонанса, поэтому стерео-пара фильтров считает их один раз
    static Coefficients makeCoefficients (float cutoffHz, float resonance, float piOverSampleRate, float maxCutoff) noexcept
    {
        const float fc = std::clamp (cutoffHz, 10.0f, maxCutoff);
        const float g = std::tan (piOverSampleRate * fc);

        Coefficients c;
        c.G = g / (1.0f + g);
        c.beta = 1.0f / (1.0f + g);
        c.k = 4.5f * resonance;
        return c;
    }

    void reset() noexcept { s1 = s2 = s3 = s4 = 0.0f; }

    // inputGain: усиление перед нелинейностью (драйв)
    float process (float x, const Coefficients& c, float inputGain, Mode mode) noexcept
    {
        const float G = c.G;
        const float in = x * inputGain;

        // y4 = G^4 * u + sigma, u = in - k * y4  =>  решаем относительно y4
        const float G2 = G * G;
        const float G4 = G2 * G2;
        const float sigma = c.beta * (G2 * G * s1 + G2 * s2 + G * s3 + s4);
        const float y4Estimate = (G4 * in + sigma) / (1.0f + c.k * G4);

        const float u = fastTanh (in - c.k * y4Estimate);

        const float y1 = onePole (u, s1, G);
        const float y2 = onePole (y1, s2, G);
        const float y3 = onePole (y2, s3, G);
        const float y4 = onePole (y3, s4, G);

        switch (mode)
        {
            // Частичная компенсация падения басов на высоком резонансе. Делается на выходе:
            // если усилить вход, tanh насытится сильнее и "съест" резонанс
            case Mode::lowpass24: return y4 * (1.0f + 0.3f * c.k);
            case Mode::lowpass12: return y2 * (1.0f + 0.2f * c.k);
            case Mode::bandpass:  return 2.0f * (y1 - y2);
            case Mode::highpass:  return u - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4;
        }

        return y4;
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
