#pragma once

#include "dsp/Saturation.h"
#include "dsp/SmoothNoise.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace sonder
{

// Пинг-понг дилей с характером ленты: смена времени "тянет" высоту тона, как у тейп-эха,
// в петле обратной связи темнеющий фильтр и насыщение, ручка Tape добавляет детонацию (wow и flutter).
class TapeDelay
{
public:
    static constexpr float kMaxDelaySeconds = 2.0f;

    void prepare (double newSampleRate)
    {
        sampleRate = (float) newSampleRate;

        size_t size = 1;
        while (size < (size_t) (newSampleRate * (kMaxDelaySeconds + 0.1)))
            size <<= 1;

        bufferLeft.assign (size, 0.0f);
        bufferRight.assign (size, 0.0f);
        mask = (int) size - 1;
        writeIndex = 0;

        smoothedDelay = -1.0f;
        currentMix = 0.0f;
        wowPhase = 0.0f;
        lowpassLeft = lowpassRight = highpassLeft = highpassRight = 0.0f;

        flutter.prepare (newSampleRate, 7.0f, 0x7a9e1234u, 1);
        highpassCoef = 1.0f - std::exp (-2.0f * 3.14159265f * 70.0f / sampleRate);
    }

    void process (float* left, float* right, int numSamples,
                  float delaySeconds, float feedback, float mix, float tape) noexcept
    {
        const float targetDelay = std::clamp (delaySeconds * sampleRate, 1.0f, kMaxDelaySeconds * sampleRate);
        if (smoothedDelay < 0.0f)
            smoothedDelay = targetDelay;

        const float delaySlew = 1.0f - std::exp (-1.0f / (0.25f * sampleRate));
        const float mixCoef = 1.0f - std::exp (-1.0f / (0.02f * sampleRate));
        const float toneHz = 12000.0f * std::exp2 (-2.0f * tape);
        const float lowpassCoef = 1.0f - std::exp (-2.0f * 3.14159265f * toneHz / sampleRate);

        const float wowDepth = tape * 0.002f * sampleRate;
        const float flutterDepth = tape * 0.00015f * sampleRate;
        const float wowIncrement = 0.6f / sampleRate;
        const float drive = 1.0f + 2.0f * tape;

        for (int i = 0; i < numSamples; ++i)
        {
            smoothedDelay += (targetDelay - smoothedDelay) * delaySlew;
            currentMix += (mix - currentMix) * mixCoef;

            const float modulation = wowDepth * (1.0f + fastSinCycles (wowPhase)) + flutterDepth * (1.0f + flutter.next());
            const float delay = std::max (1.0f, smoothedDelay + modulation);

            const float readLeft = read (bufferLeft, delay);
            const float readRight = read (bufferRight, delay);

            lowpassLeft  += (readLeft - lowpassLeft) * lowpassCoef;
            lowpassRight += (readRight - lowpassRight) * lowpassCoef;
            highpassLeft  += (lowpassLeft - highpassLeft) * highpassCoef;
            highpassRight += (lowpassRight - highpassRight) * highpassCoef;

            const float wetLeft = lowpassLeft - highpassLeft;
            const float wetRight = lowpassRight - highpassRight;

            // Пинг-понг: вход пишется в левую линию, правая получает только повторы из левой
            const float input = 0.5f * (left[i] + right[i]);
            bufferLeft[(size_t) writeIndex]  = input + saturate (wetRight * feedback, drive);
            bufferRight[(size_t) writeIndex] = saturate (wetLeft * feedback, drive);

            left[i]  += wetLeft * currentMix;
            right[i] += wetRight * currentMix;

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
    float smoothedDelay = -1.0f, currentMix = 0.0f;
    float wowPhase = 0.0f;
    float lowpassLeft = 0.0f, lowpassRight = 0.0f;
    float highpassLeft = 0.0f, highpassRight = 0.0f, highpassCoef = 0.0f;
    SmoothNoise flutter;
};

} // namespace sonder
