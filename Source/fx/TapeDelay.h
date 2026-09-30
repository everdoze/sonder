#pragma once

#include "FxTypes.h"
#include "Parameters.h"
#include "dsp/Saturation.h"
#include "dsp/SmoothNoise.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace sonder
{

// Стерео-дилей с характером ленты: смена времени "тянет" высоту тона, как у тейп-эха,
// в петле обратной связи фильтры и насыщение, ручка Tape добавляет детонацию (wow и flutter).
// Width = 100% - пинг-понг, 0% - обычное моно-эхо; Offset сдвигает время правого канала.
class TapeDelay
{
public:
    static constexpr float kMaxDelaySeconds = 2.0f;

    void prepare (double newSampleRate)
    {
        sampleRate = (float) newSampleRate;

        // Запас на сдвиг правого канала (до +50%) и детонацию
        size_t size = 1;
        while (size < (size_t) (newSampleRate * (kMaxDelaySeconds * 1.5 + 0.1)))
            size <<= 1;

        bufferLeft.assign (size, 0.0f);
        bufferRight.assign (size, 0.0f);
        mask = (int) size - 1;
        flutter.prepare (newSampleRate, 7.0f, 0x7a9e1234u, 1);
        reset();
    }

    void reset() noexcept
    {
        std::fill (bufferLeft.begin(), bufferLeft.end(), 0.0f);
        std::fill (bufferRight.begin(), bufferRight.end(), 0.0f);
        writeIndex = 0;
        smoothedDelay = -1.0f;
        wowPhase = 0.0f;
        lowpassLeft = lowpassRight = highpassLeft = highpassRight = 0.0f;
    }

    // Время задержки в секундах с учётом синхронизации с темпом
    static float delaySeconds (const float* p, double bpm) noexcept
    {
        const double beats = syncDivisionInBeats (juce::roundToInt (p[fxp::delay::sync]));
        const float seconds = beats > 0.0 ? (float) (beats * 60.0 / juce::jmax (20.0, bpm)) : p[fxp::delay::time];
        return juce::jlimit (0.001f, kMaxDelaySeconds, seconds);
    }

    void process (float* left, float* right, int numSamples, const float* p, double bpm) noexcept
    {
        const float feedback = p[fxp::delay::feedback];
        const float mix = p[fxp::delay::mix];
        const float tape = p[fxp::delay::tape];
        const float width = p[fxp::delay::width];
        const float rightRatio = 1.0f + p[fxp::delay::offset];

        const float targetDelay = delaySeconds (p, bpm) * sampleRate;
        if (smoothedDelay < 0.0f)
            smoothedDelay = targetDelay;

        // Лента сама темнит повторы, поэтому Tape дополнительно опускает верхний срез
        const float delaySlew = 1.0f - std::exp (-1.0f / (0.25f * sampleRate));
        const float highCut = p[fxp::delay::highCut] * std::exp2 (-1.5f * tape);
        const float lowpassCoef = 1.0f - std::exp (-2.0f * 3.14159265f * std::fmin (highCut, 0.45f * sampleRate) / sampleRate);
        const float highpassCoef = 1.0f - std::exp (-2.0f * 3.14159265f * p[fxp::delay::lowCut] / sampleRate);

        const float wowDepth = tape * 0.002f * sampleRate;
        const float flutterDepth = tape * 0.00015f * sampleRate;
        const float wowIncrement = 0.6f / sampleRate;
        const float drive = 1.0f + 2.0f * tape;

        for (int i = 0; i < numSamples; ++i)
        {
            smoothedDelay += (targetDelay - smoothedDelay) * delaySlew;

            const float modulation = wowDepth * (1.0f + fastSinCycles (wowPhase)) + flutterDepth * (1.0f + flutter.next());
            const float delayLeft = std::max (1.0f, smoothedDelay + modulation);
            const float delayRight = std::max (1.0f, smoothedDelay * rightRatio + modulation);

            const float readLeft = read (bufferLeft, delayLeft);
            const float readRight = read (bufferRight, delayRight);

            lowpassLeft  += (readLeft - lowpassLeft) * lowpassCoef;
            lowpassRight += (readRight - lowpassRight) * lowpassCoef;
            highpassLeft  += (lowpassLeft - highpassLeft) * highpassCoef;
            highpassRight += (lowpassRight - highpassRight) * highpassCoef;

            const float wetLeft = lowpassLeft - highpassLeft;
            const float wetRight = lowpassRight - highpassRight;

            // Пинг-понг: вход пишется в левую линию, повторы перекрёстно переходят из канала в канал.
            // При Width = 0 обе линии получают вход и собственные повторы: обычное моно-эхо.
            const float input = 0.5f * (left[i] + right[i]);
            const float feedLeft = width * wetRight + (1.0f - width) * wetLeft;
            const float feedRight = width * wetLeft + (1.0f - width) * wetRight;

            bufferLeft[(size_t) writeIndex]  = input + saturate (feedLeft * feedback, drive);
            bufferRight[(size_t) writeIndex] = (1.0f - width) * input + saturate (feedRight * feedback, drive);

            left[i]  += wetLeft * mix;
            right[i] += wetRight * mix;

            wowPhase += wowIncrement;
            if (wowPhase >= 1.0f)
                wowPhase -= 1.0f;

            writeIndex = (writeIndex + 1) & mask;
        }
    }

private:
    static float saturate (float x, float drive) noexcept { return fastTanh (x * drive) / drive; }

    float read (const std::vector<float>& buffer, float delaySamples) const noexcept
    {
        const float position = (float) writeIndex - delaySamples;
        const float floorPosition = std::floor (position);
        const float frac = position - floorPosition;
        const int i0 = (int) floorPosition & mask;
        const int i1 = (i0 + 1) & mask;
        return buffer[(size_t) i0] + (buffer[(size_t) i1] - buffer[(size_t) i0]) * frac;
    }

    std::vector<float> bufferLeft, bufferRight;
    int mask = 0, writeIndex = 0;
    float sampleRate = 44100.0f;
    float smoothedDelay = -1.0f;
    float wowPhase = 0.0f;
    float lowpassLeft = 0.0f, lowpassRight = 0.0f;
    float highpassLeft = 0.0f, highpassRight = 0.0f;
    SmoothNoise flutter;
};

} // namespace sonder
