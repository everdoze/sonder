#pragma once

#include "FxTypes.h"

#include <array>
#include <atomic>
#include <cmath>

namespace sonder
{

// Сдвиг частоты: все частоты спектра сдвигаются на одно и то же число герц (в отличие от сдвига высоты,
// гармоники перестают быть кратными - звук становится металлическим, "неземным").
// Аналитический сигнал строится двумя цепочками фазовращателей с разностью фаз 90 градусов
// (коэффициенты Olli Niemitalo), потом умножается на комплексную синусоиду.
// Feedback пропускает выход снова через сдвиг - получается "лестница" сдвигов.
class FreqShifter
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = (float) newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        channels = {};
        phase = { 0.0, 0.0 };
    }

    static float shiftHz (const float* p) noexcept
    {
        return p[fxp::shifter::shift] + p[fxp::shifter::fine];
    }

    void process (float* left, float* right, int numSamples, const float* p) noexcept
    {
        const float shift = shiftHz (p);
        const float spread = p[fxp::shifter::spread];
        const float feedback = p[fxp::shifter::feedback];
        const float mix = p[fxp::shifter::mix];

        // У правого канала сдвиг по мере Spread уходит в обратную сторону
        const std::array<float, 2> shifts { shift, shift * (1.0f - 2.0f * spread) };
        float* io[2] { left, right };

        for (size_t ch = 0; ch < 2; ++ch)
        {
            auto& c = channels[ch];
            const double increment = (double) shifts[ch] / sampleRate;

            for (int i = 0; i < numSamples; ++i)
            {
                const float dry = io[ch][i];
                const float input = dry + feedback * c.lastOutput;

                // Два пути фазовращателей; первый задержан на сэмпл - вместе они дают сдвиг на 90 градусов
                float a = input, b = input;
                for (size_t s = 0; s < 4; ++s)
                {
                    a = c.pathA[s].process (a, coefficientsA[s]);
                    b = c.pathB[s].process (b, coefficientsB[s]);
                }

                const float inPhase = c.delayedA;
                c.delayedA = a;

                const double angle = 6.283185307179586 * phase[ch];
                const float shifted = inPhase * (float) std::cos (angle) + b * (float) std::sin (angle);
                c.lastOutput = std::isfinite (shifted) ? shifted : 0.0f;

                io[ch][i] = dry + (c.lastOutput - dry) * mix;

                phase[ch] += increment;
                phase[ch] -= std::floor (phase[ch]);
            }
        }

        displayPhase.store ((float) phase[0]);
    }

    float getDisplayPhase() const noexcept { return displayPhase.load(); }

private:
    struct Allpass
    {
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

        float process (float x, float a2) noexcept
        {
            const float y = a2 * (x + y2) - x2;
            x2 = x1;
            x1 = x;
            y2 = y1;
            y1 = y;
            return y;
        }
    };

    struct Channel
    {
        std::array<Allpass, 4> pathA, pathB;
        float delayedA = 0.0f, lastOutput = 0.0f;
    };

    // Квадраты коэффициентов Niemitalo
    static constexpr std::array<float, 4> coefficientsA { 0.6923878f * 0.6923878f, 0.9360654322959f * 0.9360654322959f,
                                                          0.9882295226860f * 0.9882295226860f, 0.9987488452737f * 0.9987488452737f };
    static constexpr std::array<float, 4> coefficientsB { 0.4021921162426f * 0.4021921162426f, 0.8561710882420f * 0.8561710882420f,
                                                          0.9722909545651f * 0.9722909545651f, 0.9952884791278f * 0.9952884791278f };

    std::array<Channel, 2> channels {};
    std::array<double, 2> phase { 0.0, 0.0 };
    float sampleRate = 44100.0f;
    std::atomic<float> displayPhase { 0.0f };
};

} // namespace sonder
