#pragma once

#include "FxTypes.h"
#include "dsp/Saturation.h"

#include <array>
#include <cmath>
#include <vector>

namespace sonder
{

// Алгоритмический реверб: входная диффузия (цепочка фазовращателей) и сеть из восьми линий задержки
// с обратной связью через матрицу Хаусхолдера (FDN). Время затухания задаётся в секундах,
// верхние частоты гаснут быстрее (Damping), линии слегка модулируются, чтобы хвост не "звенел".
class Reverb
{
public:
    static constexpr int kNumLines = 8;
    static constexpr int kNumDiffusers = 4;

    void prepare (double newSampleRate)
    {
        sampleRate = (float) newSampleRate;

        // Самая длинная линия: 73.1 мс * 1.6 (максимальный Size) плюс запас на модуляцию
        size_t lineSize = 1;
        while (lineSize < (size_t) (newSampleRate * 0.13))
            lineSize <<= 1;

        for (auto& line : lines)
            line.buffer.assign (lineSize, 0.0f);

        lineMask = (int) lineSize - 1;

        size_t preDelaySize = 1;
        while (preDelaySize < (size_t) (newSampleRate * 0.21))
            preDelaySize <<= 1;

        for (auto& buffer : preDelay)
            buffer.assign (preDelaySize, 0.0f);

        preDelayMask = (int) preDelaySize - 1;

        static constexpr float diffuserMs[kNumDiffusers] { 4.7f, 3.6f, 12.7f, 9.3f };
        for (auto& channel : diffusers)
        {
            for (int d = 0; d < kNumDiffusers; ++d)
            {
                // У правого канала диффузоры чуть длиннее: входы декоррелируются
                const float scale = &channel == &diffusers[1] ? 1.07f : 1.0f;
                channel[(size_t) d].buffer.assign ((size_t) juce::jmax (8, juce::roundToInt (diffuserMs[d] * scale * 0.001f * sampleRate)), 0.0f);
                channel[(size_t) d].index = 0;
            }
        }

        reset();
    }

    void reset() noexcept
    {
        for (auto& line : lines)
        {
            std::fill (line.buffer.begin(), line.buffer.end(), 0.0f);
            line.damping = 0.0f;
        }

        for (auto& buffer : preDelay)
            std::fill (buffer.begin(), buffer.end(), 0.0f);

        for (auto& channel : diffusers)
            for (auto& diffuser : channel)
                std::fill (diffuser.buffer.begin(), diffuser.buffer.end(), 0.0f);

        writeIndex = preDelayIndex = 0;
        lowCutLeft = lowCutRight = 0.0f;
        modPhase = 0.0f;
    }

    void process (float* left, float* right, int numSamples, const float* p) noexcept
    {
        static constexpr float lineMs[kNumLines] { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };

        const float sizeScale = 0.4f + 1.2f * p[fxp::reverb::size];
        const float decay = p[fxp::reverb::decay];
        const float mix = p[fxp::reverb::mix];
        const float width = p[fxp::reverb::width];

        // Усиление каждой линии так, чтобы за время Decay хвост упал на 60 дБ
        std::array<float, kNumLines> delaySamples {}, gains {};
        for (int l = 0; l < kNumLines; ++l)
        {
            const float seconds = lineMs[l] * 0.001f * sizeScale;
            delaySamples[(size_t) l] = seconds * sampleRate;
            gains[(size_t) l] = std::exp (-6.9078f * seconds / decay);
        }

        // Damping: срез ФНЧ в петле от 16 кГц до 1.5 кГц
        const float dampingHz = 16000.0f * std::exp2 (-3.4f * p[fxp::reverb::damping]);
        const float dampingCoef = 1.0f - std::exp (-6.2831853f * std::fmin (dampingHz, 0.45f * sampleRate) / sampleRate);
        const float lowCutCoef = 1.0f - std::exp (-6.2831853f * p[fxp::reverb::lowCut] / sampleRate);
        const float preDelaySamples = p[fxp::reverb::preDelay] * 0.001f * sampleRate;
        const float modDepth = p[fxp::reverb::modulation] * 0.0006f * sampleRate; // до ~0.6 мс
        const float modIncrement = 0.35f / sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            // Предзадержка
            preDelay[0][(size_t) preDelayIndex] = left[i];
            preDelay[1][(size_t) preDelayIndex] = right[i];
            float inLeft = readBuffer (preDelay[0], preDelayMask, preDelayIndex, preDelaySamples);
            float inRight = readBuffer (preDelay[1], preDelayMask, preDelayIndex, preDelaySamples);
            preDelayIndex = (preDelayIndex + 1) & preDelayMask;

            // Диффузия: размазывает атаку, чтобы не было слышно отдельных отражений
            for (auto& diffuser : diffusers[0])
                inLeft = diffuser.process (inLeft, 0.6f);
            for (auto& diffuser : diffusers[1])
                inRight = diffuser.process (inRight, 0.6f);

            // Чтение линий (с лёгкой модуляцией длины) и затухание верхов
            std::array<float, kNumLines> outputs {};
            float sum = 0.0f;

            for (int l = 0; l < kNumLines; ++l)
            {
                auto& line = lines[(size_t) l];
                const float modulation = modDepth * fastSinCycles (modPhase + (float) l * 0.125f);
                const float sample = readBuffer (line.buffer, lineMask, writeIndex, delaySamples[(size_t) l] + modulation + 1.0f);

                line.damping += (sample - line.damping) * dampingCoef;
                outputs[(size_t) l] = line.damping * gains[(size_t) l];
                sum += outputs[(size_t) l];
            }

            // Матрица Хаусхолдера: каждая линия получает свой выход минус общую среднюю.
            // Она без потерь перемешивает энергию между линиями.
            const float reflection = sum * (2.0f / (float) kNumLines);
            for (int l = 0; l < kNumLines; ++l)
            {
                const float input = (l % 2 == 0 ? inLeft : inRight);
                lines[(size_t) l].buffer[(size_t) writeIndex] = input + outputs[(size_t) l] - reflection;
            }

            writeIndex = (writeIndex + 1) & lineMask;

            // Стерео-выход: разные знаки линий для левого и правого каналов
            float wetLeft  = outputs[0] - outputs[2] + outputs[4] - outputs[6] + 0.5f * (outputs[1] + outputs[5]);
            float wetRight = outputs[1] - outputs[3] + outputs[5] - outputs[7] + 0.5f * (outputs[0] + outputs[4]);

            lowCutLeft += (wetLeft - lowCutLeft) * lowCutCoef;
            lowCutRight += (wetRight - lowCutRight) * lowCutCoef;
            wetLeft -= lowCutLeft;
            wetRight -= lowCutRight;

            const float mid = 0.5f * (wetLeft + wetRight);
            const float side = 0.5f * (wetLeft - wetRight) * width;

            left[i]  = left[i] * (1.0f - 0.4f * mix) + (mid + side) * mix * 0.45f;
            right[i] = right[i] * (1.0f - 0.4f * mix) + (mid - side) * mix * 0.45f;

            modPhase += modIncrement;
            if (modPhase >= 1.0f)
                modPhase -= 1.0f;
        }
    }

private:
    static float readBuffer (const std::vector<float>& buffer, int mask, int writeIndex, float delaySamples) noexcept
    {
        const float position = (float) writeIndex - delaySamples;
        const float floorPosition = std::floor (position);
        const float frac = position - floorPosition;
        const int i0 = (int) floorPosition & mask;
        return buffer[(size_t) i0] + (buffer[(size_t) ((i0 + 1) & mask)] - buffer[(size_t) i0]) * frac;
    }

    struct Line
    {
        std::vector<float> buffer;
        float damping = 0.0f;
    };

    // Фазовращатель на линии задержки (Шрёдер)
    struct Diffuser
    {
        std::vector<float> buffer;
        int index = 0;

        float process (float x, float g) noexcept
        {
            const float delayed = buffer[(size_t) index];
            const float input = x + delayed * g;
            buffer[(size_t) index] = input;
            index = index + 1 >= (int) buffer.size() ? 0 : index + 1;
            return delayed - input * g;
        }
    };

    std::array<Line, kNumLines> lines;
    std::array<std::vector<float>, 2> preDelay;
    std::array<std::array<Diffuser, kNumDiffusers>, 2> diffusers;

    float sampleRate = 44100.0f;
    int lineMask = 0, writeIndex = 0;
    int preDelayMask = 0, preDelayIndex = 0;
    float lowCutLeft = 0.0f, lowCutRight = 0.0f;
    float modPhase = 0.0f;
};

} // namespace sonder
