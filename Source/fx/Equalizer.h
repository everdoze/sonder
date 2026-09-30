#pragma once

#include "FxTypes.h"

#include <array>
#include <cmath>
#include <complex>

namespace sonder
{

// Эквалайзер: срез низов, полка низов, две параметрические полосы, полка верхов, срез верхов (формулы RBJ).
// Срезы выключены, когда стоят на краю диапазона (20 Гц и 20 кГц).
class Equalizer
{
public:
    static constexpr int kNumBands = 6;
    static constexpr float kLowCutOff = 21.0f, kHighCutOff = 19900.0f;

    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

        // АЧХ на частоте frequency (для отрисовки)
        float magnitude (float frequency, float sampleRate) const noexcept
        {
            const float w = 6.2831853f * frequency / sampleRate;
            const std::complex<float> z1 = std::polar (1.0f, -w), z2 = std::polar (1.0f, -2.0f * w);
            return std::abs ((b0 + b1 * z1 + b2 * z2) / (1.0f + a1 * z1 + a2 * z2));
        }
    };

    enum class Band { lowCut, lowShelf, peak, highShelf, highCut };

    static Biquad makeBand (Band band, float sampleRate, float frequency, float gainDb, float q) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 6.2831853f * std::fmin (frequency, 0.45f * sampleRate) / sampleRate;
        const float cosw = std::cos (w), sinw = std::sin (w);
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;

        switch (band)
        {
            case Band::peak:
            {
                const float alpha = sinw / (2.0f * q);
                b0 = 1.0f + alpha * A;
                b1 = -2.0f * cosw;
                b2 = 1.0f - alpha * A;
                a0 = 1.0f + alpha / A;
                a1 = -2.0f * cosw;
                a2 = 1.0f - alpha / A;
                break;
            }

            case Band::lowShelf:
            case Band::highShelf:
            {
                // Полки с крутизной S = 1
                const float beta = 2.0f * std::sqrt (A) * (sinw * 0.70710678f);
                const float sign = band == Band::lowShelf ? 1.0f : -1.0f;

                b0 = A * ((A + 1.0f) - sign * (A - 1.0f) * cosw + beta);
                b1 = sign * 2.0f * A * ((A - 1.0f) - sign * (A + 1.0f) * cosw);
                b2 = A * ((A + 1.0f) - sign * (A - 1.0f) * cosw - beta);
                a0 = (A + 1.0f) + sign * (A - 1.0f) * cosw + beta;
                a1 = -sign * 2.0f * ((A - 1.0f) + sign * (A + 1.0f) * cosw);
                a2 = (A + 1.0f) + sign * (A - 1.0f) * cosw - beta;
                break;
            }

            case Band::lowCut:
            case Band::highCut:
            {
                // Срезы 12 дБ/окт (Баттерворт)
                const float alpha = sinw / (2.0f * 0.70710678f);
                const float sign = band == Band::lowCut ? 1.0f : -1.0f;

                b0 = (1.0f + sign * cosw) * 0.5f;
                b1 = -sign * (1.0f + sign * cosw);
                b2 = b0;
                a0 = 1.0f + alpha;
                a1 = -2.0f * cosw;
                a2 = 1.0f - alpha;
                break;
            }
        }

        return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
    }

    // Все полосы по значениям ручек (нейтральная полоса - единичный фильтр)
    static std::array<Biquad, kNumBands> makeBands (const float* p, float sampleRate) noexcept
    {
        using namespace fxp::equalizer;
        std::array<Biquad, kNumBands> bands {};

        if (p[lowCut] > kLowCutOff)
            bands[0] = makeBand (Band::lowCut, sampleRate, p[lowCut], 0.0f, 0.707f);

        bands[1] = makeBand (Band::lowShelf, sampleRate, p[lowFreq], p[lowGain], 0.707f);
        bands[2] = makeBand (Band::peak, sampleRate, p[mid1Freq], p[mid1Gain], p[mid1Q]);
        bands[3] = makeBand (Band::peak, sampleRate, p[mid2Freq], p[mid2Gain], p[mid2Q]);
        bands[4] = makeBand (Band::highShelf, sampleRate, p[highFreq], p[highGain], 0.707f);

        if (p[highCut] < kHighCutOff)
            bands[5] = makeBand (Band::highCut, sampleRate, p[highCut], 0.0f, 0.707f);

        return bands;
    }

    // Общая АЧХ на частоте frequency (для отрисовки)
    static float response (const std::array<Biquad, kNumBands>& bands, float frequency, float sampleRate) noexcept
    {
        float magnitude = 1.0f;
        for (const auto& band : bands)
            magnitude *= band.magnitude (frequency, sampleRate);

        return magnitude;
    }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = (float) newSampleRate;
        reset();
    }

    void reset() noexcept { state = {}; }

    void process (float* left, float* right, int numSamples, const float* p) noexcept
    {
        const auto bands = makeBands (p, sampleRate);
        float* io[2] { left, right };

        for (size_t ch = 0; ch < 2; ++ch)
        {
            for (size_t b = 0; b < bands.size(); ++b)
            {
                const auto& c = bands[b];
                auto& s = state[ch][b];

                // Транспонированная прямая форма II
                for (int i = 0; i < numSamples; ++i)
                {
                    const float x = io[ch][i];
                    const float y = c.b0 * x + s[0];
                    s[0] = c.b1 * x - c.a1 * y + s[1];
                    s[1] = c.b2 * x - c.a2 * y;
                    io[ch][i] = y;
                }
            }
        }
    }

private:
    float sampleRate = 44100.0f;
    std::array<std::array<std::array<float, 2>, kNumBands>, 2> state {};
};

} // namespace sonder
