#pragma once

#include "Saturation.h"

#include <array>
#include <cmath>

namespace sonder
{

// Формантный фильтр "гласных": три полосовых SVF на частотах формант A-E-I-O-U
// плюс немного низа, чтобы бас не превращался в писк.
class FormantFilter
{
public:
    struct Band
    {
        float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.0f, gain = 0.0f;
    };

    struct Coefficients
    {
        std::array<Band, 3> bands {};
        float bodyCoef = 0.0f;
    };

    struct Formant
    {
        float frequency, gain;
    };

    // vowel 0..1 (A..U), shift - множитель частот формант (от среза фильтра)
    static std::array<Formant, 3> formantsFor (float vowel, float shift) noexcept
    {
        struct Vowel { float freq[3]; float gain[3]; };
        static constexpr Vowel table[5] {
            { { 730.0f, 1090.0f, 2440.0f }, { 1.0f, 0.50f, 0.25f } }, // A
            { { 530.0f, 1840.0f, 2480.0f }, { 1.0f, 0.40f, 0.30f } }, // E
            { { 270.0f, 2290.0f, 3010.0f }, { 1.0f, 0.30f, 0.20f } }, // I
            { { 570.0f,  840.0f, 2410.0f }, { 1.0f, 0.45f, 0.20f } }, // O
            { { 300.0f,  870.0f, 2240.0f }, { 1.0f, 0.35f, 0.15f } }, // U
        };

        const float position = std::fmin (std::fmax (vowel, 0.0f), 1.0f) * 4.0f;
        const int v0 = position >= 4.0f ? 3 : (int) position;
        const float a = position - (float) v0;

        std::array<Formant, 3> result {};
        for (int i = 0; i < 3; ++i)
        {
            // Частоты интерполируются в логарифмической шкале
            const float logFrequency = std::log (table[v0].freq[i]) * (1.0f - a) + std::log (table[v0 + 1].freq[i]) * a;
            result[(size_t) i].frequency = std::exp (logFrequency) * shift;
            result[(size_t) i].gain = table[v0].gain[i] * (1.0f - a) + table[v0 + 1].gain[i] * a;
        }

        return result;
    }

    static Coefficients makeCoefficients (float vowel, float shift, float resonance,
                                          float piOverSampleRate, float maxFrequency) noexcept
    {
        Coefficients c;
        const float q = 3.0f + resonance * 17.0f;
        const auto formants = formantsFor (vowel, shift);

        for (size_t i = 0; i < 3; ++i)
        {
            const float frequency = std::fmin (std::fmax (formants[i].frequency, 40.0f), maxFrequency);
            const float g = std::tan (piOverSampleRate * frequency);
            auto& band = c.bands[i];
            band.k = 1.0f / q;
            band.a1 = 1.0f / (1.0f + g * (g + band.k));
            band.a2 = g * band.a1;
            band.a3 = g * band.a2;
            band.gain = formants[i].gain;
        }

        c.bodyCoef = 1.0f - std::exp (-2.0f * 180.0f * piOverSampleRate); // ФНЧ ~180 Гц
        return c;
    }

    void reset() noexcept
    {
        state = {};
        body1 = body2 = 0.0f;
    }

    float process (float x, const Coefficients& c, float inputGain) noexcept
    {
        const float in = fastTanh (x * inputGain);
        float out = 0.0f;

        for (size_t i = 0; i < 3; ++i)
        {
            // TPT SVF (Simper): полосовой выход с единичным усилением на резонансе
            const auto& band = c.bands[i];
            auto& s = state[i];
            const float v3 = in - s[1];
            const float v1 = band.a1 * s[0] + band.a2 * v3;
            const float v2 = s[1] + band.a2 * s[0] + band.a3 * v3;
            s[0] = 2.0f * v1 - s[0];
            s[1] = 2.0f * v2 - s[1];
            out += band.gain * band.k * v1 * 2.0f;
        }

        body1 += (in - body1) * c.bodyCoef;
        body2 += (body1 - body2) * c.bodyCoef;
        return out + 0.5f * body2;
    }

private:
    std::array<std::array<float, 2>, 3> state {};
    float body1 = 0.0f, body2 = 0.0f;
};

} // namespace sonder
