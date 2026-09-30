#pragma once

#include "FxTypes.h"
#include "dsp/Distortion.h"

#include <juce_dsp/juce_dsp.h>

#include <atomic>

namespace sonder
{

// Дисторшн в рэке эффектов: искажает сумму всех голосов (аккорд "грязнится" целиком,
// в отличие от дисторшна в голосе). Считается на удвоенной частоте, чтобы не было алиасинга.
class MasterDistortion
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (2, 1,
                                                                          juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                          true, false);
        oversampling->initProcessing ((size_t) maxBlockSize);
        oversampledRate = (float) (sampleRate * 2.0);
        left.prepare (sampleRate * 2.0);
        right.prepare (sampleRate * 2.0);
    }

    void reset() noexcept
    {
        if (oversampling != nullptr)
            oversampling->reset();

        left.reset();
        right.reset();
    }

    // Коэффициент ФНЧ для ручки Tone: от 800 Гц до ~19 кГц, на максимуме фильтр выключен
    static float toneCoefficient (float tone, float sampleRate) noexcept
    {
        return tone >= 0.999f ? 1.0f
                              : 1.0f - std::exp (-6.2831853f * 800.0f * std::exp2 (tone * 4.6f) / sampleRate);
    }

    // В рэке типы идут без "Off": 0 - Tube, 1 - Hard, 2 - Fold, 3 - Crush
    static Distortion::Type typeFromParameter (float value) noexcept
    {
        return static_cast<Distortion::Type> (1 + juce::jlimit (0, 3, juce::roundToInt (value)));
    }

    void process (float* leftChannel, float* rightChannel, int numSamples, const float* p) noexcept
    {
        if (oversampling == nullptr)
            return;

        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            peak = std::fmax (peak, std::fmax (std::abs (leftChannel[i]), std::abs (rightChannel[i])));

        displayPeak.store (peak);

        float* channels[2] { leftChannel, rightChannel };
        juce::dsp::AudioBlock<float> block (channels, 2, (size_t) numSamples);
        auto oversampled = oversampling->processSamplesUp (block);

        const auto settings = Distortion::makeSettings (typeFromParameter (p[fxp::distortion::type]), p[fxp::distortion::drive],
                                                        p[fxp::distortion::mix],
                                                        toneCoefficient (p[fxp::distortion::tone], oversampledRate));
        const float level = juce::Decibels::decibelsToGain (p[fxp::distortion::level]);
        float* upLeft = oversampled.getChannelPointer (0);
        float* upRight = oversampled.getChannelPointer (1);

        for (size_t i = 0; i < oversampled.getNumSamples(); ++i)
        {
            upLeft[i] = left.process (upLeft[i], settings) * level;
            upRight[i] = right.process (upRight[i], settings) * level;
        }

        oversampling->processSamplesDown (block);
    }

    // Пиковый уровень на входе для визуализации
    float getDisplayPeak() const noexcept { return displayPeak.load(); }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    Distortion left, right;
    float oversampledRate = 88200.0f;
    std::atomic<float> displayPeak { 0.0f };
};

} // namespace sonder
