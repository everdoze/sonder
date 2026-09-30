#pragma once

#include "FxTypes.h"

#include <atomic>
#include <cmath>
#include <vector>

namespace sonder
{

// Лимитер с заглядыванием вперёд на 1.5 мс: сигнал задерживается, а усиление успевает опуститься
// до того, как пик дойдёт до выхода. Gain поднимает вход, Ceiling - потолок выхода, Release - отпускание.
class Limiter
{
public:
    static constexpr float kLookaheadSeconds = 0.0015f;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = (float) newSampleRate;
        lookahead = juce::jmax (1, (int) std::ceil (kLookaheadSeconds * sampleRate));

        size_t size = 1;
        while (size < (size_t) lookahead + 2)
            size <<= 1;

        bufferLeft.assign (size, 0.0f);
        bufferRight.assign (size, 0.0f);
        peaks.assign (size, 0.0f);
        mask = (int) size - 1;
        reset();
    }

    void reset() noexcept
    {
        std::fill (bufferLeft.begin(), bufferLeft.end(), 0.0f);
        std::fill (bufferRight.begin(), bufferRight.end(), 0.0f);
        std::fill (peaks.begin(), peaks.end(), 0.0f);
        writeIndex = 0;
        gain = 1.0f;
    }

    void process (float* left, float* right, int numSamples, const float* p) noexcept
    {
        const float inputGain = juce::Decibels::decibelsToGain (p[fxp::limiter::gain]);
        const float ceiling = juce::Decibels::decibelsToGain (p[fxp::limiter::ceiling]);
        const float attack = 1.0f - std::exp (-5.0f / (float) lookahead); // успевает за время заглядывания
        const float release = 1.0f - std::exp (-1.0f / (juce::jmax (0.001f, p[fxp::limiter::release]) * sampleRate));

        float lowestGain = 1.0f, inputPeak = 0.0f, outputPeak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float inLeft = left[i] * inputGain, inRight = right[i] * inputGain;
            const float peak = std::fmax (std::abs (inLeft), std::abs (inRight));
            inputPeak = std::fmax (inputPeak, peak);

            bufferLeft[(size_t) writeIndex] = inLeft;
            bufferRight[(size_t) writeIndex] = inRight;
            peaks[(size_t) writeIndex] = peak;

            // Самый громкий пик в окне, которое ещё не дошло до выхода
            float windowPeak = 0.0f;
            for (int k = 0; k <= lookahead; ++k)
                windowPeak = std::fmax (windowPeak, peaks[(size_t) ((writeIndex - k) & mask)]);

            const float target = windowPeak > ceiling ? ceiling / windowPeak : 1.0f;
            gain += (target - gain) * (target < gain ? attack : release);
            lowestGain = std::fmin (lowestGain, gain);

            const int readIndex = (writeIndex - lookahead) & mask;
            // Страховка: даже если сглаживание не успело, выход не выходит за потолок
            left[i] = juce::jlimit (-ceiling, ceiling, bufferLeft[(size_t) readIndex] * gain);
            right[i] = juce::jlimit (-ceiling, ceiling, bufferRight[(size_t) readIndex] * gain);
            outputPeak = std::fmax (outputPeak, std::fmax (std::abs (left[i]), std::abs (right[i])));

            writeIndex = (writeIndex + 1) & mask;
        }

        displayReduction.store (juce::Decibels::gainToDecibels (lowestGain));
        displayInput.store (juce::Decibels::gainToDecibels (inputPeak, -60.0f));
        displayOutput.store (juce::Decibels::gainToDecibels (outputPeak, -60.0f));
    }

    // Для экрана, дБ
    float getDisplayReduction() const noexcept { return displayReduction.load(); }
    float getDisplayInput() const noexcept     { return displayInput.load(); }
    float getDisplayOutput() const noexcept    { return displayOutput.load(); }

private:
    std::vector<float> bufferLeft, bufferRight, peaks;
    int mask = 0, writeIndex = 0, lookahead = 1;
    float sampleRate = 44100.0f, gain = 1.0f;
    std::atomic<float> displayReduction { 0.0f }, displayInput { -60.0f }, displayOutput { -60.0f };
};

} // namespace sonder
