#pragma once

#include "FxTypes.h"

#include <array>
#include <atomic>
#include <cmath>

namespace sonder
{

// Многополосный компрессор в духе OTT: звук делится на три полосы (кроссоверы Линквица-Райли 24 дБ/окт),
// в каждой громкое прижимается вниз (Downward), тихое вытягивается вверх (Upward).
// Получается плотный, "прожаренный" звук; Depth смешивает его с исходным.
class Multiband
{
public:
    static constexpr int kNumBands = 3;
    static constexpr float kDownThresholdDb = -30.0f, kUpThresholdDb = -42.0f, kMaxUpwardDb = 18.0f;

    // Сколько дБ добавить полосе при уровне levelDb (отрицательное - прижать)
    static float gainFor (float levelDb, float upward, float downward) noexcept
    {
        float result = 0.0f;

        if (levelDb > kDownThresholdDb)
            result -= (levelDb - kDownThresholdDb) * (1.0f - 1.0f / (1.0f + 19.0f * downward));

        if (levelDb < kUpThresholdDb)
        {
            // Совсем тихое (хвосты, шум) не вытягиваем: иначе поднимется шипение
            const float gate = juce::jlimit (0.0f, 1.0f, (levelDb + 72.0f) / 12.0f);
            result += juce::jmin (kMaxUpwardDb, (kUpThresholdDb - levelDb) * (1.0f - 1.0f / (1.0f + 3.0f * upward))) * gate;
        }

        return result;
    }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = (float) newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        channels = {};
        envelopes.fill (0.0f);
        lastLowSplit = lastHighSplit = -1.0f;
    }

    void process (float* left, float* right, int numSamples, const float* p) noexcept
    {
        const float lowSplit = p[fxp::multiband::lowSplit];
        const float highSplit = juce::jmax (lowSplit * 1.5f, p[fxp::multiband::highSplit]);

        if (lowSplit != lastLowSplit || highSplit != lastHighSplit)
        {
            lastLowSplit = lowSplit;
            lastHighSplit = highSplit;
            lowCoefficients = Biquad::butterworth (lowSplit, sampleRate);
            highCoefficients = Biquad::butterworth (highSplit, sampleRate);
        }

        const float time = p[fxp::multiband::time];
        const float attack = 1.0f - std::exp (-1.0f / (0.0015f * time * sampleRate));
        const float release = 1.0f - std::exp (-1.0f / (0.08f * time * sampleRate));
        const float upward = p[fxp::multiband::upward], downward = p[fxp::multiband::downward];
        const float depth = p[fxp::multiband::depth];
        const float output = juce::Decibels::decibelsToGain (p[fxp::multiband::output]);
        const std::array<float, kNumBands> bandGainDb { p[fxp::multiband::lowGain], p[fxp::multiband::midGain], p[fxp::multiband::highGain] };

        std::array<float, kNumBands> appliedDb {};
        float* io[2] { left, right };

        for (int i = 0; i < numSamples; ++i)
        {
            std::array<std::array<float, kNumBands>, 2> bands {};

            for (size_t ch = 0; ch < 2; ++ch)
            {
                auto& c = channels[ch];
                const float x = io[ch][i];

                // Две ступени Баттерворта подряд - кроссовер Линквица-Райли
                const float low = c.lowPass[1].process (c.lowPass[0].process (x, lowCoefficients.lowPass), lowCoefficients.lowPass);
                const float rest = c.highPass[1].process (c.highPass[0].process (x, lowCoefficients.highPass), lowCoefficients.highPass);
                const float mid = c.midPass[1].process (c.midPass[0].process (rest, highCoefficients.lowPass), highCoefficients.lowPass);
                const float high = c.topPass[1].process (c.topPass[0].process (rest, highCoefficients.highPass), highCoefficients.highPass);

                bands[ch] = { low, mid, high };
            }

            // Сухой сигнал тоже собирается из полос: сумма кроссоверов сдвигает фазу, и смесь с исходным дала бы гребёнку
            float dryLeft = 0.0f, dryRight = 0.0f, wetLeft = 0.0f, wetRight = 0.0f;

            for (size_t band = 0; band < (size_t) kNumBands; ++band)
            {
                // Огибающая по обоим каналам сразу: стерео-картина не плывёт
                const float peak = std::fmax (std::abs (bands[0][band]), std::abs (bands[1][band]));
                auto& envelope = envelopes[band];
                envelope += (peak - envelope) * (peak > envelope ? attack : release);

                const float levelDb = 20.0f * std::log10 (envelope + 1.0e-9f);
                const float gainDb = gainFor (levelDb, upward, downward) + bandGainDb[band];
                const float gain = std::exp2 (gainDb * 0.16609640f); // 10^(dB/20)

                dryLeft += bands[0][band];
                dryRight += bands[1][band];
                wetLeft += bands[0][band] * gain;
                wetRight += bands[1][band] * gain;

                if (i == numSamples - 1)
                {
                    appliedDb[band] = gainDb;
                    displayLevel[band].store (levelDb);
                }
            }

            left[i] = dryLeft + (wetLeft * output - dryLeft) * depth;
            right[i] = dryRight + (wetRight * output - dryRight) * depth;
        }

        for (size_t band = 0; band < (size_t) kNumBands; ++band)
            displayGain[band].store (appliedDb[band]);
    }

    // Для экрана: уровень полосы и сколько дБ ей добавлено
    float getDisplayLevel (int band) const noexcept { return displayLevel[(size_t) band].load(); }
    float getDisplayGain (int band) const noexcept  { return displayGain[(size_t) band].load(); }

private:
    struct Biquad
    {
        struct Coefficients
        {
            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        };

        struct Pair
        {
            Coefficients lowPass, highPass;
        };

        // Баттерворт второго порядка (Q = 0.707), ФНЧ и ФВЧ на одной частоте
        static Pair butterworth (float frequency, float sampleRate) noexcept
        {
            const float w = 6.2831853f * std::fmin (frequency, 0.45f * sampleRate) / sampleRate;
            const float cosw = std::cos (w), alpha = std::sin (w) / (2.0f * 0.70710678f);
            const float a0 = 1.0f + alpha;

            Pair pair;
            pair.lowPass = { (1.0f - cosw) * 0.5f / a0, (1.0f - cosw) / a0, (1.0f - cosw) * 0.5f / a0,
                             -2.0f * cosw / a0, (1.0f - alpha) / a0 };
            pair.highPass = { (1.0f + cosw) * 0.5f / a0, -(1.0f + cosw) / a0, (1.0f + cosw) * 0.5f / a0,
                              -2.0f * cosw / a0, (1.0f - alpha) / a0 };
            return pair;
        }

        float s1 = 0.0f, s2 = 0.0f;

        float process (float x, const Coefficients& c) noexcept
        {
            const float y = c.b0 * x + s1;
            s1 = c.b1 * x - c.a1 * y + s2;
            s2 = c.b2 * x - c.a2 * y;
            return y;
        }
    };

    struct Channel
    {
        std::array<Biquad, 2> lowPass, highPass, midPass, topPass;
    };

    std::array<Channel, 2> channels {};
    std::array<float, kNumBands> envelopes {};
    Biquad::Pair lowCoefficients, highCoefficients;
    float lastLowSplit = -1.0f, lastHighSplit = -1.0f;
    float sampleRate = 44100.0f;

    std::array<std::atomic<float>, kNumBands> displayLevel {}, displayGain {};
};

} // namespace sonder
