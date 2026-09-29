#pragma once

#include "Parameters.h"
#include "dsp/LadderFilter.h"
#include "dsp/Lfo.h"
#include "dsp/PolyBlepOscillator.h"

#include <array>

namespace sonder
{

// Порядок совпадает с Choices::modSources() / modDestinations()
enum class ModSource { off, lfo1, lfo2, filterEnv, ampEnv, velocity, modWheel, aftertouch, key, random, count };
enum class ModDest { off, pitch, osc1Pitch, osc2Pitch, pulseWidth, oscMix, fm, fold, sub, noise,
                     cutoff, resonance, drive, amp, pan, count };

enum class VoiceMode { poly, mono, legato };

struct ModSlot
{
    ModSource source = ModSource::off;
    ModDest dest = ModDest::off;
    float amount = 0.0f;
};

// Снимок параметров на один аудиоблок: читается один раз, а не каждым голосом по отдельности
struct SynthParams
{
    PolyBlepOscillator::Shape osc1Shape = PolyBlepOscillator::Shape::saw;
    PolyBlepOscillator::Shape osc2Shape = PolyBlepOscillator::Shape::saw;
    float osc2OffsetCents = 0.0f;
    float oscMix = 0.5f, pulseWidth = 0.5f, subLevel = 0.0f, noiseLevel = 0.0f;
    float fmAmount = 0.0f, ringLevel = 0.0f, foldAmount = 0.0f;

    LadderFilter::Mode filterMode = LadderFilter::Mode::lowpass24;
    float cutoff = 2000.0f, resonance = 0.0f, drive = 0.3f;
    float filterEnvAmount = 0.0f, keyTrack = 0.5f, velocityToCutoff = 0.0f;

    float filterAttack = 0.01f, filterDecay = 0.5f, filterSustain = 0.5f, filterRelease = 0.5f;
    float ampAttack = 0.01f, ampDecay = 0.5f, ampSustain = 1.0f, ampRelease = 0.5f;
    float ampVelocity = 0.5f;

    std::array<ModSlot, kNumModSlots> modSlots {};

    float drift = 0.0f, jitter = 0.0f, spread = 0.0f;
    int unit = 0;

    VoiceMode voiceMode = VoiceMode::poly;
    int unisonVoices = 1;
    float unisonDetune = 0.0f, unisonWidth = 0.0f;
    float glide = 0.0f, bendRange = 2.0f, vibrato = 0.0f;

    static SynthParams fromRefs (const ParameterRefs& p)
    {
        SynthParams s;
        s.osc1Shape = static_cast<PolyBlepOscillator::Shape> ((int) p.osc1Shape->load());
        s.osc2Shape = static_cast<PolyBlepOscillator::Shape> ((int) p.osc2Shape->load());
        s.osc2OffsetCents = p.osc2Semi->load() * 100.0f + p.osc2Fine->load();
        s.oscMix = p.oscMix->load();
        s.pulseWidth = p.pulseWidth->load();
        s.subLevel = p.subLevel->load();
        s.noiseLevel = p.noiseLevel->load();
        s.fmAmount = p.fmAmount->load();
        s.ringLevel = p.ringLevel->load();
        s.foldAmount = p.foldAmount->load();

        s.filterMode = static_cast<LadderFilter::Mode> ((int) p.filterMode->load());
        s.cutoff = p.cutoff->load();
        s.resonance = p.resonance->load();
        s.drive = p.drive->load();
        s.filterEnvAmount = p.filterEnvAmt->load();
        s.keyTrack = p.keyTrack->load();
        s.velocityToCutoff = p.velToCutoff->load();

        s.filterAttack = p.filterAttack->load();
        s.filterDecay = p.filterDecay->load();
        s.filterSustain = p.filterSustain->load();
        s.filterRelease = p.filterRelease->load();
        s.ampAttack = p.ampAttack->load();
        s.ampDecay = p.ampDecay->load();
        s.ampSustain = p.ampSustain->load();
        s.ampRelease = p.ampRelease->load();
        s.ampVelocity = p.ampVelocity->load();

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

// Глобальные модуляции, общие для всех голосов. Массивы заполнены посэмплово на весь блок.
struct ModulationBus
{
    const float* lfo1 = nullptr;
    const float* lfo2 = nullptr;
    const float* modWheel = nullptr;
    const float* aftertouch = nullptr;
    const float* pitchBend = nullptr; // в полутонах

    float globalPitchCents = 0.0f; // просадка питания и прогрев
    float globalGain = 1.0f;
    float driftScale = 1.0f;
};

// Разброс "железа" между голосовыми платами. Значения в [-1, 1], итог масштабирует ручка Spread.
struct Tolerances
{
    float osc1Cents = 0.0f, osc2Cents = 0.0f, cutoffOctaves = 0.0f, envelopeTime = 0.0f;
    float pulseWidth = 0.0f, level = 0.0f, pan = 0.0f, vibratoRate = 0.0f;
};

} // namespace sonder
