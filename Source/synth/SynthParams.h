#pragma once

#include "Parameters.h"
#include "dsp/Distortion.h"
#include "dsp/LadderFilter.h"
#include "dsp/LfoShapes.h"
#include "dsp/PolyBlepOscillator.h"

#include <array>

namespace sonder
{

// Порядок совпадает с Choices::modSources() / modDestinations()
enum class ModSource { off, lfo1, lfo2, filterEnv, ampEnv, velocity, modWheel, aftertouch, key, random,
                       lfo3, lfo4, lfo5, lfo6, lfo7, lfo8, count };

enum class ModDest { off, pitch, osc1Pitch, osc2Pitch, pulseWidth, oscMix, fm, fold, sub, noise,
                     cutoff, resonance, drive, amp, pan, vowel, distDrive, distMix, osc1WtPos, osc2WtPos,
                     lfo1Rate, lfo2Rate, lfo3Rate, lfo4Rate, lfo5Rate, lfo6Rate, lfo7Rate, lfo8Rate, count };

enum class VoiceMode { poly, mono, legato };

// Индекс LFO для источника или -1
inline int lfoIndexForSource (ModSource source) noexcept
{
    if (source == ModSource::lfo1) return 0;
    if (source == ModSource::lfo2) return 1;
    if (source >= ModSource::lfo3 && source <= ModSource::lfo8) return 2 + (int) source - (int) ModSource::lfo3;
    return -1;
}

inline ModSource sourceForLfo (int lfo) noexcept
{
    if (lfo == 0) return ModSource::lfo1;
    if (lfo == 1) return ModSource::lfo2;
    return static_cast<ModSource> ((int) ModSource::lfo3 + lfo - 2);
}

inline ModDest rateDestForLfo (int lfo) noexcept
{
    return static_cast<ModDest> ((int) ModDest::lfo1Rate + lfo);
}

struct ModSlot
{
    ModSource source = ModSource::off;
    ModDest dest = ModDest::off;
    float amount = 0.0f;
};

struct LfoParams
{
    LfoShape shape = LfoShape::sine;
    LfoMode mode = LfoMode::free;
    float rateHz = 1.0f;
};

// Снимок параметров на один аудиоблок: читается один раз, а не каждым голосом по отдельности
struct SynthParams
{
    PolyBlepOscillator::Shape osc1Shape = PolyBlepOscillator::Shape::saw;
    PolyBlepOscillator::Shape osc2Shape = PolyBlepOscillator::Shape::saw;
    float osc1WtPos = 0.0f, osc2WtPos = 0.0f;
    float osc2OffsetCents = 0.0f;
    float oscMix = 0.5f, pulseWidth = 0.5f, subLevel = 0.0f, noiseLevel = 0.0f;
    float fmAmount = 0.0f, ringLevel = 0.0f, foldAmount = 0.0f;

    bool vowelFilter = false;
    LadderFilter::Mode ladderMode = LadderFilter::Mode::lowpass24;
    float cutoff = 2000.0f, resonance = 0.0f, drive = 0.3f, vowel = 0.0f;
    float filterEnvAmount = 0.0f, keyTrack = 0.5f, velocityToCutoff = 0.0f;

    Distortion::Type distType = Distortion::Type::off;
    float distDrive = 0.0f, distMix = 1.0f, distTone = 1.0f;

    float filterAttack = 0.01f, filterDecay = 0.5f, filterSustain = 0.5f, filterRelease = 0.5f;
    float ampAttack = 0.01f, ampDecay = 0.5f, ampSustain = 1.0f, ampRelease = 0.5f;
    float ampVelocity = 0.5f;

    std::array<LfoParams, kNumLfos> lfos {};
    std::array<ModSlot, kNumModSlots> modSlots {};

    float drift = 0.0f, jitter = 0.0f, spread = 0.0f;
    int unit = 0;

    VoiceMode voiceMode = VoiceMode::poly;
    int unisonVoices = 1;
    float unisonDetune = 0.0f, unisonWidth = 0.0f;
    float glide = 0.0f, bendRange = 2.0f, vibrato = 0.0f;

    static SynthParams fromRefs (const ParameterRefs& p, double bpm)
    {
        SynthParams s;
        s.osc1Shape = static_cast<PolyBlepOscillator::Shape> ((int) p.osc1Shape->load());
        s.osc2Shape = static_cast<PolyBlepOscillator::Shape> ((int) p.osc2Shape->load());
        s.osc1WtPos = p.osc1WtPos->load();
        s.osc2WtPos = p.osc2WtPos->load();
        s.osc2OffsetCents = p.osc2Semi->load() * 100.0f + p.osc2Fine->load();
        s.oscMix = p.oscMix->load();
        s.pulseWidth = p.pulseWidth->load();
        s.subLevel = p.subLevel->load();
        s.noiseLevel = p.noiseLevel->load();
        s.fmAmount = p.fmAmount->load();
        s.ringLevel = p.ringLevel->load();
        s.foldAmount = p.foldAmount->load();

        const int mode = (int) p.filterMode->load();
        s.vowelFilter = mode == 4;
        s.ladderMode = static_cast<LadderFilter::Mode> (juce::jmin (mode, 3));
        s.cutoff = p.cutoff->load();
        s.resonance = p.resonance->load();
        s.drive = p.drive->load();
        s.vowel = p.vowel->load();
        s.filterEnvAmount = p.filterEnvAmt->load();
        s.keyTrack = p.keyTrack->load();
        s.velocityToCutoff = p.velToCutoff->load();

        s.distType = static_cast<Distortion::Type> ((int) p.distType->load());
        s.distDrive = p.distDrive->load();
        s.distMix = p.distMix->load();
        s.distTone = p.distTone->load();

        s.filterAttack = p.filterAttack->load();
        s.filterDecay = p.filterDecay->load();
        s.filterSustain = p.filterSustain->load();
        s.filterRelease = p.filterRelease->load();
        s.ampAttack = p.ampAttack->load();
        s.ampDecay = p.ampDecay->load();
        s.ampSustain = p.ampSustain->load();
        s.ampRelease = p.ampRelease->load();
        s.ampVelocity = p.ampVelocity->load();

        for (size_t i = 0; i < s.lfos.size(); ++i)
        {
            auto& lfo = s.lfos[i];
            lfo.shape = static_cast<LfoShape> ((int) p.lfoShape[i]->load());
            lfo.mode = static_cast<LfoMode> ((int) p.lfoMode[i]->load());

            const double beats = syncDivisionInBeats ((int) p.lfoSync[i]->load());
            lfo.rateHz = beats > 0.0 ? (float) (bpm / 60.0 / beats) : p.lfoRate[i]->load();
        }

        for (size_t i = 0; i < s.modSlots.size(); ++i)
        {
            s.modSlots[i].source = static_cast<ModSource> ((int) p.modSource[i]->load());
            s.modSlots[i].dest = static_cast<ModDest> ((int) p.modDest[i]->load());
            s.modSlots[i].amount = p.modAmount[i]->load();
        }

        s.drift = p.drift->load();
        s.jitter = p.jitter->load();
        s.spread = p.spread->load();
        s.unit = (int) p.unit->load() - 1;

        s.voiceMode = static_cast<VoiceMode> ((int) p.voiceMode->load());
        s.unisonVoices = (int) p.unisonVoices->load();
        s.unisonDetune = p.unisonDetune->load();
        s.unisonWidth = p.unisonWidth->load();
        s.glide = p.glide->load();
        s.bendRange = p.bendRange->load();
        s.vibrato = p.vibrato->load();
        return s;
    }
};

// Глобальные модуляции, общие для всех голосов на один блок
struct ModulationBus
{
    const float* modWheel = nullptr;
    const float* aftertouch = nullptr;
    const float* pitchBend = nullptr; // в полутонах

    float globalPitchCents = 0.0f; // просадка питания и прогрев
    float globalGain = 1.0f;
    float driftScale = 1.0f;

    // Общая фаза LFO в режиме Free на начало блока и её приращение на сэмпл
    std::array<float, kNumLfos> lfoPhase {};
    std::array<uint32_t, kNumLfos> lfoCycle {};
    std::array<float, kNumLfos> lfoIncrement {};
    std::array<const float*, kNumLfos> lfoTables {};

    std::array<const Wavetable*, 2> wavetables {};
};

// Разброс "железа" между голосовыми платами. Значения в [-1, 1], итог масштабирует ручка Spread.
struct Tolerances
{
    float osc1Cents = 0.0f, osc2Cents = 0.0f, cutoffOctaves = 0.0f, envelopeTime = 0.0f;
    float pulseWidth = 0.0f, level = 0.0f, pan = 0.0f, vibratoRate = 0.0f;
};

} // namespace sonder
