#pragma once

#include "SynthParams.h"

namespace sonder
{

// Масштаб модуляции при глубине 1.0 (общий для DSP и отображения на ручках)
inline constexpr float kModCutoffOctaves = 6.0f;
inline constexpr float kModPitchSemitones = 12.0f;
inline constexpr float kModLfoRateOctaves = 4.0f;
inline constexpr float kModPulseWidth = 0.45f;

// Какую цель матрицы модулирует ручка параметра (ModDest::off - ручка не модулируется)
inline ModDest destinationForParameter (const juce::String& id)
{
    using namespace ParamIDs;

    static const std::pair<const char*, ModDest> table[] {
        { osc1WtPos, ModDest::osc1WtPos }, { osc2WtPos, ModDest::osc2WtPos }, { osc2Semi, ModDest::osc2Pitch },
        { pulseWidth, ModDest::pulseWidth }, { oscMix, ModDest::oscMix }, { fmAmount, ModDest::fm },
        { foldAmount, ModDest::fold }, { subLevel, ModDest::sub }, { noiseLevel, ModDest::noise },
        { cutoff, ModDest::cutoff }, { resonance, ModDest::resonance }, { drive, ModDest::drive },
        { vowel, ModDest::vowel }, { distDrive, ModDest::distDrive }, { distMix, ModDest::distMix },
    };

    for (const auto& [parameter, dest] : table)
        if (id == parameter)
            return dest;

    for (int lfo = 0; lfo < kNumLfos; ++lfo)
        if (id == lfoRate (lfo))
            return rateDestForLfo (lfo);

    return ModDest::off;
}

// LFO, нота и случайное значение качаются в обе стороны; огибающие, velocity и контроллеры - только вверх
inline bool isBipolarSource (ModSource source)
{
    return lfoIndexForSource (source) >= 0 || source == ModSource::key || source == ModSource::random;
}

// Значение параметра после модуляции, в его собственных единицах (повторяет логику голоса)
inline float applyModulation (ModDest dest, float baseValue, float modulation)
{
    switch (dest)
    {
        case ModDest::cutoff:     return baseValue * std::exp2 (modulation * kModCutoffOctaves);
        case ModDest::pulseWidth: return baseValue + modulation * kModPulseWidth;
        case ModDest::osc2Pitch:  return baseValue + modulation * kModPitchSemitones;
        default: break;
    }

    if (dest >= ModDest::lfo1Rate && dest <= ModDest::lfo8Rate)
        return baseValue * std::exp2 (modulation * kModLfoRateOctaves);

    return baseValue + modulation;
}

} // namespace sonder
