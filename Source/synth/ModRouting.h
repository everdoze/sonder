#pragma once

#include "SynthParams.h"

#include <map>

namespace sonder
{

// Масштаб модуляции при глубине 1.0 (общий для DSP и отображения на ручках)
inline constexpr float kModCutoffOctaves = 6.0f;
inline constexpr float kModPitchSemitones = 12.0f;
inline constexpr float kModLfoRateOctaves = 4.0f;
inline constexpr float kModPulseWidth = 0.45f;
inline constexpr float kModNoiseColor = 5.0f; // вся шкала видов шума

// Ручка, которую двигает цель матрицы (пусто - у цели нет своей ручки: Pitch, Amp, Pan, общая ширина импульса)
inline juce::String parameterForDestination (ModDest dest)
{
    using namespace ParamIDs;
    using D = ModDest;

    int slot = 0, param = 0;
    if (fxSlotAndParam (dest, slot, param))
        return fxParam (slot, param);

    for (int osc = 0; osc < kNumOscs; ++osc)
    {
        if (dest == wtPosDestForOsc (osc)) return oscWtPos (osc);
        if (dest == pitchDestForOsc (osc)) return oscSemi (osc);
        if (dest == levelDestForOsc (osc)) return oscLevel (osc);
        if (dest == fineDestForOsc (osc))  return oscFine (osc);
    }

    for (int filter = 0; filter < kNumFilters; ++filter)
    {
        const auto dests = destsForFilter (filter);
        if (dest == dests.cutoff)    return filterParam (filter, FilterParam::cutoff);
        if (dest == dests.resonance) return filterParam (filter, FilterParam::resonance);
        if (dest == dests.drive)     return filterParam (filter, FilterParam::drive);
        if (dest == dests.vowel)     return filterParam (filter, FilterParam::vowel);
        if (dest == dests.envAmount) return filterParam (filter, FilterParam::envAmount);
        if (dest == dests.keyTrack)  return filterParam (filter, FilterParam::keyTrack);
        if (dest == dests.velocity)  return filterParam (filter, FilterParam::velocity);
    }

    for (int lfo = 0; lfo < kNumLfos; ++lfo)
        if (dest == rateDestForLfo (lfo))
            return lfoRate (lfo);

    switch (dest)
    {
        case D::fm:               return fmAmount;
        case D::fold:             return foldAmount;
        case D::sub:              return subLevel;
        case D::noise:            return noiseLevel;
        case D::noiseColor:       return noiseColor;
        case D::distDrive:        return distDrive;
        case D::distMix:          return distMix;
        case D::distTone:         return distTone;
        case D::ring:             return ringLevel;
        case D::ampAttack:        return ampAttack;
        case D::ampDecay:         return ampDecay;
        case D::ampSustain:       return ampSustain;
        case D::ampRelease:       return ampRelease;
        case D::ampVelocity:      return ampVelocity;
        case D::filterEnvAttack:  return filterAttack;
        case D::filterEnvDecay:   return filterDecay;
        case D::filterEnvSustain: return filterSustain;
        case D::filterEnvRelease: return filterRelease;
        case D::unisonDetune:     return unisonDetune;
        case D::unisonWidth:      return unisonWidth;
        case D::glide:            return ParamIDs::glide;
        case D::glideCurve:       return ParamIDs::glideCurve;
        case D::vibrato:          return ParamIDs::vibrato;
        case D::drift:            return ParamIDs::drift;
        case D::jitter:           return ParamIDs::jitter;
        case D::spread:           return ParamIDs::spread;
        case D::master:           return masterGain;
        case D::sag:              return ParamIDs::sag;
        case D::warmup:           return ParamIDs::warmup;
        default:                  break;
    }

    return {};
}

// Какую цель матрицы модулирует ручка параметра (ModDest::off - ручка не модулируется)
inline ModDest destinationForParameter (const juce::String& id)
{
    static const std::map<juce::String, ModDest> table = []
    {
        std::map<juce::String, ModDest> result;
        for (int d = 1; d < (int) ModDest::count; ++d)
            if (const auto parameter = parameterForDestination (static_cast<ModDest> (d)); parameter.isNotEmpty())
                result.emplace (parameter, static_cast<ModDest> (d));

        // Ширина импульса модулируется у всех осцилляторов сразу
        for (int osc = 0; osc < kNumOscs; ++osc)
            result.emplace (ParamIDs::oscPw (osc), ModDest::pulseWidth);

        return result;
    }();

    const auto found = table.find (id);
    return found != table.end() ? found->second : ModDest::off;
}

// Значение параметра после модуляции, в его собственных единицах (повторяет логику голоса).
// Для целей "в долях хода" нужна шкала ручки: range.
inline float applyModulation (ModDest dest, float baseValue, float modulation, const juce::NormalisableRange<float>& range)
{
    if (isNormalisedDest (dest))
        return range.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, range.convertTo0to1 (baseValue) + modulation));

    switch (dest)
    {
        case ModDest::cutoff:
        case ModDest::filter2Cutoff: return baseValue * std::exp2 (modulation * kModCutoffOctaves);
        case ModDest::pulseWidth: return baseValue + modulation * kModPulseWidth;
        case ModDest::noiseColor: return baseValue + modulation * kModNoiseColor;
        case ModDest::osc1Pitch:
        case ModDest::osc2Pitch:
        case ModDest::osc3Pitch:
        case ModDest::osc4Pitch:  return baseValue + modulation * kModPitchSemitones;
        default: break;
    }

    if (dest >= ModDest::lfo1Rate && dest <= ModDest::lfo8Rate)
        return baseValue * std::exp2 (modulation * kModLfoRateOctaves);

    return baseValue + modulation;
}

} // namespace sonder
