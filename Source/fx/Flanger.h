#pragma once

#include "FxTypes.h"
#include "dsp/Saturation.h"

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace sonder
{

// Флэнжер: короткая модулируемая задержка с обратной связью (в том числе отрицательной)
class Flanger
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = (float) newSampleRate;

        size_t size = 1;
        while (size < (size_t) (newSampleRate * 0.03))
            size <<= 1;

        for (auto& buffer : buffers)
            buffer.assign (size, 0.0f);

        mask = (int) size - 1;
        reset();
    }

    void reset() noexcept
    {
        for (auto& buffer : buffers)
            std::fill (buffer.begin(), buffer.end(), 0.0f);

        writeIndex = 0;
        lfoPhase = 0.0f;
    }

    // Задержка в миллисекундах как функция значения LFO
    static float delayMs (float lfoValue, float depth, float baseMs) noexcept
    {
        return baseMs + depth * 3.2f * (1.0f + lfoValue);
    }

    void process (float* left, float* right, int numSamples, const float* p) noexcept
    {
        const float depth = p[fxp::flanger::depth];
        const float feedback = p[fxp::flanger::feedback];
        const float base = p[fxp::flanger::delay];
        const float mix = p[fxp::flanger::mix];
        const float spread = 0.5f * p[fxp::flanger::spread];
        const float phaseIncrement = p[fxp::flanger::rate] / sampleRate;
        const float msToSamples = sampleRate * 0.001f;
        float* io[2] { left, right };

        for (int i = 0; i < numSamples; ++i)
        {
            for (size_t ch = 0; ch < 2; ++ch)
            {
                const float delay = delayMs (fastSinCycles (lfoPhase + spread * (float) ch), depth, base);
                const float position = (float) writeIndex - delay * msToSamples;
                const float floorPosition = std::floor (position);
                const float frac = position - floorPosition;
                const int i0 = (int) floorPosition & mask;
                const auto& buffer = buffers[ch];
                const float wet = buffer[(size_t) i0] + (buffer[(size_t) ((i0 + 1) & mask)] - buffer[(size_t) i0]) * frac;

                const float dry = io[ch][i];
                buffers[ch][(size_t) writeIndex] = dry + fastTanh (wet * feedback);
                io[ch][i] = dry + 0.5f * mix * (wet - dry);

                if (ch == 0 && (i & 63) == 0)
                    displayDelayMs.store (delay);
            }

            lfoPhase += phaseIncrement;
            if (lfoPhase >= 1.0f)
                lfoPhase -= 1.0f;

            writeIndex = (writeIndex + 1) & mask;
        }
    }

    // Текущая задержка для визуализации
    float getDisplayDelayMs() const noexcept { return displayDelayMs.load(); }

private:
    std::array<std::vector<float>, 2> buffers;
    int mask = 0, writeIndex = 0;
    float sampleRate = 44100.0f;
    float lfoPhase = 0.0f;
    std::atomic<float> displayDelayMs { 1.0f };
};

} // namespace sonder
