#pragma once

#include <algorithm>
#include <cmath>

namespace sonder
{

// ADSR с экспоненциальными сегментами, как у RC-цепочки:
// атака заряжает конденсатор к 1.3 и обрывается на 1.0 (выпуклая кривая, как у аналога),
// спад и затухание экспоненциальные. Повторная нота стартует с текущего уровня, без сброса в 0.
class AnalogEnvelope
{
public:
    void setSampleRate (double newSampleRate) noexcept { sampleRate = (float) newSampleRate; }

    void setParameters (float attackSeconds, float decaySeconds, float sustainLevel, float releaseSeconds) noexcept
    {
        attackCoef  = coefficient (attackSeconds, 1.0f - 1.0f / kAttackTarget);
        decayCoef   = coefficient (decaySeconds, kDecayRatio);
        releaseCoef = coefficient (releaseSeconds, kDecayRatio);
        sustain = sustainLevel;
    }

    void noteOn() noexcept { stage = Stage::attack; }

    void noteOff() noexcept
    {
        if (stage != Stage::idle)
            stage = Stage::release;
    }

    void reset() noexcept
    {
        stage = Stage::idle;
        level = 0.0f;
    }

    bool isActive() const noexcept { return stage != Stage::idle; }

    float process() noexcept
    {
        switch (stage)
        {
            case Stage::attack:
                level += (kAttackTarget - level) * attackCoef;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::decay;
                }
                break;

            case Stage::decay:
                level += (sustain - level) * decayCoef;
                break;

            case Stage::release:
                level -= level * releaseCoef;
                if (level < 1.0e-4f)
                    reset();
                break;

            case Stage::idle:
                break;
        }

        return level;
    }

private:
    enum class Stage { idle, attack, decay, release };

    static constexpr float kAttackTarget = 1.3f;
    static constexpr float kDecayRatio = 0.001f; // время сегмента = спад до -60 дБ

    // Коэффициент, при котором (1 - c)^n == ratio за n = seconds * sampleRate сэмплов
    float coefficient (float seconds, float ratio) const noexcept
    {
        const float samples = std::max (1.0f, seconds * sampleRate);
        return 1.0f - std::pow (ratio, 1.0f / samples);
    }

    Stage stage = Stage::idle;
    float level = 0.0f, sustain = 1.0f;
    float attackCoef = 1.0f, decayCoef = 1.0f, releaseCoef = 1.0f;
    float sampleRate = 44100.0f;
};

} // namespace sonder
