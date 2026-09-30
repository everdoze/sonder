#include "FactoryPresets.h"

namespace sonder
{

// Индексы вариантов (см. Parameters.cpp):
//   формы осцилляторов: 0 Saw, 1 Pulse, 2 Triangle, 3 Sine, 4 Wavetable
//   фильтр: 0 LP24, 1 LP12, 2 BP12, 3 HP24, 4 Vowel, 5 HP12, 6 BP24, 7 Notch;  режим голосов: 0 Poly, 1 Mono, 2 Legato
//   второй фильтр: filter2On, filter2Mode, filter2Cutoff, filter2Resonance, filter2Drive, filter2Vowel, filter2EnvAmt,
//   filter2KeyTrack, filter2Vel; filterRouting 0 Serial, 1 Parallel
//   дисторшн: 0 Off, 1 Tube, 2 Hard, 3 Fold, 4 Crush
//   формы LFO: 0 Sine, 1 Triangle, 2 Saw Up, 3 Saw Down, 4 Square, 5 S&H, 6 Smooth;  режимы LFO: 0 Free, 1 Retrig, 2 Env
//   синхронизация: 0 Free, 3 1/16, 6 1/8, 8 1/8., 9 1/4, 11 1/4., 12 1/2, 13 1/1
//   источники: 1 LFO1, 2 LFO2, 3 FEnv, 4 AEnv, 5 Vel, 6 MW, 7 AT, 8 Key, 9 Random, 10..15 LFO3..8, 16 Slide
//   направление связи (modNPolarity): 0 в обе стороны, 1 только вверх, 2 только вниз
//   цели: 1 Pitch, 2 Osc1, 3 Osc2, 4 PW, 5 Osc1 Level, 6 FM, 7 Fold, 8 Sub, 9 Noise, 10 Cutoff, 11 Reso, 12 Drive, 13 Amp, 14 Pan,
//         15 Vowel, 16 Dist Drive, 17 Dist Mix, 18 Osc1 WT Pos, 19 Osc2 WT Pos, 20..27 LFO 1..8 Rate,
//         28 Noise Color, 29 Osc3 Pitch, 30 Osc4 Pitch, 31 Osc3 WT Pos, 32 Osc4 WT Pos, 33..35 Osc 2..4 Level,
//         36 F2 Cutoff, 37 F2 Resonance, 38 F2 Drive, 39 F2 Vowel; дальше - в долях хода ручки: 40..43 Amp A/D/S/R,
//         44 Amp Velocity, 45..48 FEnv A/D/S/R, 49..51 F1 Env Amt/Key Track/Velocity, 52..54 то же у F2, 55 Dist Tone,
//         56 Ring, 57..60 Osc 1..4 Fine, 61 Unison Detune, 62 Unison Width, 63 Glide, 64 Glide Curve, 65 Vibrato,
//         66 Drift, 67 Jitter, 68 Spread, 69 Master, 70 Sag, 71 Warm-up, 72 + слот * 16 + ручка - ручки рэка
//   осцилляторы: osc1..osc4 (On, Shape, WtPos, Semi, Fine, Pw, Level); по умолчанию включены первые два, Level 50%
//   шум: noiseColor - дробное положение между видами (0 White .. 5 Digital)
//   глайд: glideMode 0 Auto, 1 Always, 2 Legato;  дисторшн голоса: distPosition 0 Post, 1 Pre
//   хорус: 0 I, 1 II, 2 I+II, 3 Free;  дисторшн в рэке: 0 Tube, 1 Hard, 2 Fold, 3 Crush
//
// После параметров идёт рэк эффектов в порядке прохождения сигнала: вид эффекта и его ручки
// (номера из fxp::<эффект>, значения в реальных единицах; остальные ручки по умолчанию).

const std::vector<FactoryPreset>& getFactoryPresets()
{
    using namespace fxp;

    static const std::vector<FactoryPreset> presets
    {
        { "Init", "Init", {} },

        // ---------------------------------------------------------------- Bass
        { "Moog Bass", "Bass", {
            { "voiceMode", 1 }, { "osc2Shape", 1 }, { "osc2Semi", -12 }, { "osc2Fine", 0 }, { "osc1Level", 0.55f }, { "osc2Level", 0.45f },
            { "subLevel", 0.5f }, { "cutoff", 180 }, { "resonance", 0.45f }, { "drive", 0.55f },
            { "filterEnvAmt", 0.55f }, { "keyTrack", 0.6f }, { "velToCutoff", 0.4f },
            { "filterAttack", 0.002f }, { "filterDecay", 0.35f }, { "filterSustain", 0.1f }, { "filterRelease", 0.15f },
            { "ampAttack", 0.002f }, { "ampDecay", 0.5f }, { "ampSustain", 0.75f }, { "ampRelease", 0.12f },
            { "glide", 0.06f }, { "spread", 0.2f }, { "masterGain", -7 } } },

        { "Sub Floor", "Bass", {
            { "voiceMode", 1 }, { "osc1Shape", 2 }, { "osc2Shape", 3 }, { "osc2Semi", -12 }, { "osc2Fine", 0 },
            { "subLevel", 0.6f }, { "cutoff", 500 }, { "resonance", 0 }, { "drive", 0.5f }, { "filterEnvAmt", 0.15f },
            { "filterDecay", 0.3f }, { "ampAttack", 0.003f }, { "ampSustain", 1 }, { "ampRelease", 0.1f },
            { "glide", 0.03f } } },

        { "Acid Line", "Bass", {
            { "voiceMode", 2 }, { "osc1Level", 1 }, { "osc2On", 0 }, { "cutoff", 350 }, { "resonance", 0.8f }, { "drive", 0.45f },
            { "filterEnvAmt", 0.6f }, { "velToCutoff", 0.5f },
            { "filterAttack", 0.001f }, { "filterDecay", 0.25f }, { "filterSustain", 0 }, { "filterRelease", 0.2f },
            { "ampAttack", 0.001f }, { "ampDecay", 0.3f }, { "ampSustain", 0.7f }, { "ampRelease", 0.08f },
            { "glide", 0.08f }, { "masterGain", -6 } },
          { { FxType::delay, { { delay::sync, 8 }, { delay::feedback, 0.3f }, { delay::mix, 0.15f } } } } },

        { "Wobble", "Bass", {
            { "voiceMode", 1 }, { "osc2Semi", -12 }, { "osc2Fine", 5 }, { "subLevel", 0.4f }, { "foldAmount", 0.15f },
            { "cutoff", 250 }, { "resonance", 0.55f }, { "drive", 0.6f }, { "filterEnvAmt", 0.1f },
            { "ampSustain", 1 }, { "ampRelease", 0.15f },
            { "lfo1Shape", 0 }, { "lfo1Sync", 6 },
            { "mod1Source", 1 }, { "mod1Dest", 10 }, { "mod1Amount", 0.45f },
            { "mod2Source", 6 }, { "mod2Dest", 7 }, { "mod2Amount", 0.5f }, { "masterGain", -7 } } },

        { "Folded Growl", "Bass", {
            { "voiceMode", 1 }, { "osc1Shape", 2 }, { "osc2Shape", 3 }, { "osc2Semi", -12 }, { "osc2Fine", 0 },
            { "foldAmount", 0.45f }, { "cutoff", 1200 }, { "resonance", 0.3f }, { "drive", 0.6f }, { "filterEnvAmt", 0.3f },
            { "filterDecay", 0.4f }, { "filterSustain", 0.2f }, { "ampSustain", 1 }, { "ampRelease", 0.15f },
            { "lfo1Shape", 6 }, { "lfo1Rate", 4 }, { "glide", 0.04f },
            { "mod1Source", 3 }, { "mod1Dest", 7 }, { "mod1Amount", 0.4f },
            { "mod2Source", 1 }, { "mod2Dest", 7 }, { "mod2Amount", 0.2f } } },

        { "Vocal Growl", "Bass", {
            { "voiceMode", 1 }, { "glide", 0.03f }, { "unisonVoices", 2 }, { "unisonDetune", 0.15f }, { "unisonWidth", 0.15f },
            { "osc1Shape", 4 }, { "osc1WtPos", 0.4f }, { "osc2Fine", 7 }, { "osc1Level", 0.65f }, { "osc2Level", 0.35f }, { "subLevel", 0.4f },
            { "foldAmount", 0.2f },
            { "filterMode", 4 }, { "cutoff", 1000 }, { "resonance", 0.55f }, { "drive", 0.7f }, { "vowel", 0.45f },
            { "filterEnvAmt", 0 },
            { "distType", 1 }, { "distDrive", 0.6f }, { "distTone", 0.75f },
            { "ampAttack", 0.003f }, { "ampSustain", 1 }, { "ampRelease", 0.1f },
            { "lfo1Shape", 1 }, { "lfo1Sync", 6 }, { "lfo1Mode", 1 },
            { "lfo2Shape", 0 }, { "lfo2Rate", 22 },
            { "mod1Source", 1 }, { "mod1Dest", 15 }, { "mod1Amount", 0.45f },
            { "mod2Source", 1 }, { "mod2Dest", 18 }, { "mod2Amount", 0.35f },
            { "mod3Source", 2 }, { "mod3Dest", 10 }, { "mod3Amount", 0.06f },
            { "mod4Source", 2 }, { "mod4Dest", 16 }, { "mod4Amount", 0.15f },
            { "mod5Source", 6 }, { "mod5Dest", 20 }, { "mod5Amount", 0.5f },
            { "mod6Source", 5 }, { "mod6Dest", 16 }, { "mod6Amount", 0.2f },
            { "masterGain", -6 } }, {}, "Vocal" },

        // ---------------------------------------------------------------- Lead
        { "Detuned Lead", "Lead", {
            { "voiceMode", 2 }, { "osc2Fine", 12 }, { "unisonVoices", 3 }, { "unisonDetune", 0.35f }, { "unisonWidth", 0.5f },
            { "cutoff", 2600 }, { "resonance", 0.3f }, { "filterEnvAmt", 0.25f }, { "filterDecay", 0.5f }, { "filterSustain", 0.6f },
            { "ampAttack", 0.01f }, { "ampSustain", 1 }, { "ampRelease", 0.25f }, { "glide", 0.1f }, { "vibrato", 0.5f },
            { "masterGain", -3.5f } },
          { { FxType::delay, { { delay::sync, 8 }, { delay::feedback, 0.4f }, { delay::mix, 0.22f } } },
            { FxType::reverb, { { reverb::decay, 2.2f }, { reverb::mix, 0.08f } } } } },

        { "Screamer", "Lead", {
            { "voiceMode", 1 }, { "osc2Semi", 7 }, { "osc2Fine", 0 }, { "osc1Level", 0.6f }, { "osc2Level", 0.4f }, { "fmAmount", 0.35f },
            { "cutoff", 1800 }, { "resonance", 0.5f }, { "drive", 0.7f }, { "filterEnvAmt", 0.3f },
            { "ampSustain", 1 }, { "ampRelease", 0.2f }, { "glide", 0.05f },
            { "mod1Source", 3 }, { "mod1Dest", 6 }, { "mod1Amount", 0.4f },
            { "mod2Source", 6 }, { "mod2Dest", 7 }, { "mod2Amount", 0.5f },
            { "masterGain", -7 } },
          { { FxType::delay, { { delay::sync, 9 }, { delay::mix, 0.18f } } } } },

        { "Supersaw", "Lead", {
            { "osc2Fine", 14 }, { "unisonVoices", 4 }, { "unisonDetune", 0.6f }, { "unisonWidth", 1 },
            { "cutoff", 6000 }, { "resonance", 0.1f }, { "filterEnvAmt", 0.1f },
            { "ampAttack", 0.01f }, { "ampSustain", 1 }, { "ampRelease", 0.5f },
            { "masterGain", -6.5f } },
          { { FxType::chorus, { { chorus::mode, 0 }, { chorus::mix, 0.3f } } },
            { FxType::delay, { { delay::sync, 8 }, { delay::mix, 0.15f } } },
            { FxType::reverb, { { reverb::decay, 2.2f }, { reverb::mix, 0.14f } } } } },

        { "Hollow Reed", "Lead", {
            { "voiceMode", 1 }, { "osc1Shape", 1 }, { "osc2Shape", 2 }, { "osc1Level", 0.7f }, { "osc2Level", 0.3f }, { "osc1Pw", 0.2f }, { "osc2Pw", 0.2f },
            { "filterMode", 1 }, { "cutoff", 1100 }, { "resonance", 0.35f }, { "filterEnvAmt", 0.2f },
            { "ampAttack", 0.04f }, { "ampSustain", 0.9f }, { "ampRelease", 0.2f }, { "vibrato", 0.6f },
            { "lfo1Rate", 5 },
            { "mod1Source", 7 }, { "mod1Dest", 10 }, { "mod1Amount", 0.3f },
            { "mod2Source", 1 }, { "mod2Dest", 4 }, { "mod2Amount", 0.15f },
            { "masterGain", -3.5f } },
          { { FxType::reverb, { { reverb::decay, 2.2f }, { reverb::mix, 0.11f } } } } },

        { "Poly Brass", "Lead", {
            { "osc2Fine", 8 }, { "cutoff", 700 }, { "resonance", 0.15f }, { "filterEnvAmt", 0.5f }, { "velToCutoff", 0.5f },
            { "filterAttack", 0.08f }, { "filterDecay", 0.6f }, { "filterSustain", 0.45f }, { "filterRelease", 0.3f },
            { "ampAttack", 0.05f }, { "ampDecay", 0.5f }, { "ampSustain", 0.85f }, { "ampRelease", 0.3f },
            { "masterGain", -5.5f } },
          { { FxType::chorus, { { chorus::mode, 0 }, { chorus::mix, 0.4f } } },
            { FxType::reverb, { { reverb::decay, 2.2f }, { reverb::mix, 0.08f } } } } },

        { "Talking Lead", "Lead", {
            { "voiceMode", 2 }, { "glide", 0.06f }, { "osc2Shape", 1 }, { "osc2Fine", 10 },
            { "filterMode", 4 }, { "cutoff", 1200 }, { "resonance", 0.6f }, { "drive", 0.5f }, { "vowel", 0.5f },
            { "distType", 1 }, { "distDrive", 0.3f },
            { "ampSustain", 1 }, { "ampRelease", 0.2f }, { "vibrato", 0.5f },
            { "lfo1Shape", 6 }, { "lfo1Sync", 6 }, { "lfo1Mode", 1 },
            { "mod1Source", 1 }, { "mod1Dest", 15 }, { "mod1Amount", 0.5f },
            { "mod2Source", 6 }, { "mod2Dest", 10 }, { "mod2Amount", 0.3f },
            { "masterGain", -3.5f } },
          { { FxType::delay, { { delay::sync, 8 }, { delay::mix, 0.2f } } },
            { FxType::reverb, { { reverb::decay, 2.2f }, { reverb::mix, 0.08f } } } } },

        { "Jet Lead", "Lead", {
            { "voiceMode", 2 }, { "glide", 0.05f }, { "osc2Fine", 9 }, { "cutoff", 3500 }, { "resonance", 0.2f },
            { "filterEnvAmt", 0.2f }, { "ampSustain", 1 }, { "ampRelease", 0.25f }, { "vibrato", 0.5f },
            { "distType", 1 }, { "distDrive", 0.3f },
            { "masterGain", -5 } },
          { { FxType::flanger, { { flanger::rate, 0.15f }, { flanger::depth, 0.8f }, { flanger::feedback, 0.75f }, { flanger::mix, 0.8f } } },
            { FxType::delay, { { delay::sync, 8 }, { delay::mix, 0.2f } } } } },

        { "8-Bit Hero", "Lead", {
            { "voiceMode", 2 }, { "glide", 0.03f }, { "osc1Shape", 1 }, { "osc2Shape", 1 }, { "osc2Semi", 12 }, { "osc2Fine", 0 },
            { "osc1Level", 0.7f }, { "osc2Level", 0.3f }, { "osc1Pw", 0.25f }, { "osc2Pw", 0.25f }, { "noiseLevel", 0.08f }, { "noiseColor", 5 },
            { "filterMode", 1 }, { "cutoff", 9000 }, { "resonance", 0 }, { "filterEnvAmt", 0 },
            { "distType", 4 }, { "distDrive", 0.35f }, { "distMix", 0.6f },
            { "ampAttack", 0.001f }, { "ampSustain", 1 }, { "ampRelease", 0.06f }, { "vibrato", 0.6f },
            { "lfo1Shape", 4 }, { "lfo1Rate", 12 },
            { "mod1Source", 1 }, { "mod1Dest", 4 }, { "mod1Amount", 0.2f },
            { "drift", 0.1f }, { "jitter", 0 } },
          { { FxType::delay, { { delay::sync, 6 }, { delay::mix, 0.15f } } } } },

        { "Fuzz Chords", "Lead", {
            { "osc2Shape", 1 }, { "osc2Fine", 8 }, { "osc1Pw", 0.35f }, { "osc2Pw", 0.35f }, { "cutoff", 1600 }, { "resonance", 0.2f },
            { "filterEnvAmt", 0.3f }, { "filterDecay", 0.5f }, { "filterSustain", 0.4f },
            { "ampAttack", 0.004f }, { "ampSustain", 0.9f }, { "ampRelease", 0.3f },
            { "masterGain", -10.5f } },
          { { FxType::distortion, { { distortion::type, 0 }, { distortion::drive, 0.6f }, { distortion::tone, 0.6f } } },
            { FxType::chorus, { { chorus::mode, 0 }, { chorus::mix, 0.4f } } },
            { FxType::compressor, { { compressor::threshold, -18 }, { compressor::ratio, 4 }, { compressor::makeup, 2 } } },
            { FxType::reverb, { { reverb::decay, 2.2f }, { reverb::mix, 0.08f } } } } },

        // ---------------------------------------------------------------- Pad
        { "Juno Strings", "Pad", {
            { "osc2Shape", 1 }, { "osc2Fine", 5 }, { "osc1Level", 0.6f }, { "osc2Level", 0.4f }, { "osc1Pw", 0.35f }, { "osc2Pw", 0.35f },
            { "cutoff", 3200 }, { "resonance", 0.1f }, { "filterEnvAmt", 0.1f },
            { "filterAttack", 0.6f }, { "filterDecay", 1.5f }, { "filterSustain", 0.7f }, { "filterRelease", 1.2f },
            { "ampAttack", 0.45f }, { "ampDecay", 1 }, { "ampSustain", 0.85f }, { "ampRelease", 1.4f },
            { "mod1Source", 2 }, { "mod1Dest", 4 }, { "mod1Amount", 0.25f },
            { "drift", 0.35f }, { "masterGain", -7.5f } },
          { { FxType::chorus, { { chorus::mode, 1 }, { chorus::mix, 0.8f } } },
            { FxType::reverb, { { reverb::size, 0.7f }, { reverb::decay, 3 }, { reverb::mix, 0.11f } } } } },

        { "Warm Pad", "Pad", {
            { "osc2Fine", 9 }, { "subLevel", 0.3f }, { "cutoff", 900 }, { "resonance", 0.2f }, { "filterEnvAmt", 0.25f },
            { "filterAttack", 1.2f }, { "filterDecay", 2.5f }, { "filterSustain", 0.5f }, { "filterRelease", 2 },
            { "ampAttack", 1 }, { "ampDecay", 2 }, { "ampSustain", 0.9f }, { "ampRelease", 2.5f },
            { "unisonVoices", 2 }, { "unisonDetune", 0.3f }, { "unisonWidth", 0.8f },
            { "lfo1Shape", 1 }, { "lfo1Rate", 0.25f },
            { "mod1Source", 1 }, { "mod1Dest", 10 }, { "mod1Amount", 0.06f },
            { "drift", 0.45f }, { "masterGain", -3.5f } },
          { { FxType::chorus, { { chorus::mode, 0 }, { chorus::mix, 0.5f } } },
            { FxType::reverb, { { reverb::size, 0.8f }, { reverb::decay, 3.7f }, { reverb::mix, 0.19f } } } } },

        { "Sweep Pad", "Pad", {
            { "osc2Fine", 10 }, { "unisonVoices", 2 }, { "cutoff", 400 }, { "resonance", 0.55f }, { "filterEnvAmt", 0.2f },
            { "filterAttack", 2 }, { "filterSustain", 0.6f }, { "filterRelease", 3 },
            { "ampAttack", 1.5f }, { "ampSustain", 1 }, { "ampRelease", 3 },
            { "lfo1Rate", 0.3f }, { "lfo2Shape", 1 }, { "lfo2Rate", 0.08f },
            { "mod1Source", 2 }, { "mod1Dest", 10 }, { "mod1Amount", 0.35f },
            { "mod2Source", 1 }, { "mod2Dest", 14 }, { "mod2Amount", 0.3f },
            { "masterGain", -3.5f } },
          { { FxType::chorus, { { chorus::mode, 1 }, { chorus::mix, 0.5f } } },
            { FxType::delay, { { delay::sync, 11 }, { delay::feedback, 0.5f }, { delay::mix, 0.15f } } },
            { FxType::reverb, { { reverb::size, 0.85f }, { reverb::decay, 4.1f }, { reverb::mix, 0.25f } } } } },

        { "Space Drone", "Pad", {
            { "osc2Semi", -12 }, { "osc2Fine", 3 }, { "subLevel", 0.3f }, { "noiseLevel", 0.1f },
            { "cutoff", 300 }, { "resonance", 0.6f }, { "drive", 0.4f }, { "filterEnvAmt", 0 },
            { "ampAttack", 3 }, { "ampSustain", 1 }, { "ampRelease", 5 },
            { "lfo1Shape", 6 }, { "lfo1Rate", 0.15f }, { "lfo2Shape", 0 }, { "lfo2Rate", 0.05f },
            { "mod1Source", 1 }, { "mod1Dest", 10 }, { "mod1Amount", 0.3f },
            { "mod2Source", 2 }, { "mod2Dest", 3 }, { "mod2Amount", 0.01f },
            { "mod3Source", 2 }, { "mod3Dest", 7 }, { "mod3Amount", 0.15f },
            { "drift", 0.6f },
            { "masterGain", -3.5f } },
          { { FxType::chorus, { { chorus::mode, 0 }, { chorus::mix, 0.6f } } },
            { FxType::delay, { { delay::sync, 13 }, { delay::feedback, 0.6f }, { delay::mix, 0.25f } } },
            { FxType::reverb, { { reverb::size, 0.95f }, { reverb::decay, 8 }, { reverb::mix, 0.45f } } } } },

        { "Glass Table", "Pad", {
            { "osc1Shape", 4 }, { "osc1WtPos", 0.15f }, { "osc2Shape", 4 }, { "osc2WtPos", 0.6f }, { "osc2Fine", 9 },
            { "cutoff", 3500 }, { "resonance", 0.15f }, { "filterEnvAmt", 0.1f },
            { "ampAttack", 0.8f }, { "ampSustain", 1 }, { "ampRelease", 2.5f },
            { "unisonVoices", 2 }, { "unisonDetune", 0.2f }, { "unisonWidth", 0.9f },
            { "lfo1Shape", 1 }, { "lfo1Rate", 0.07f }, { "lfo2Shape", 6 }, { "lfo2Rate", 0.2f },
            { "mod1Source", 1 }, { "mod1Dest", 18 }, { "mod1Amount", 0.35f },
            { "mod2Source", 2 }, { "mod2Dest", 19 }, { "mod2Amount", 0.3f },
            { "masterGain", -2.5f } },
          { { FxType::chorus, { { chorus::mode, 0 }, { chorus::mix, 0.5f } } },
            { FxType::delay, { { delay::sync, 11 }, { delay::mix, 0.15f } } },
            { FxType::reverb, { { reverb::size, 0.85f }, { reverb::decay, 4.1f }, { reverb::mix, 0.22f } } } }, "Harmonic Sweep", "Vocal" },

        { "Phase Strings", "Pad", {
            { "osc2Shape", 1 }, { "osc2Fine", 6 }, { "osc1Level", 0.55f }, { "osc2Level", 0.45f }, { "osc1Pw", 0.4f }, { "osc2Pw", 0.4f },
            { "cutoff", 2800 }, { "resonance", 0.1f }, { "filterEnvAmt", 0.1f },
            { "filterAttack", 0.5f }, { "filterDecay", 1.5f }, { "filterSustain", 0.7f }, { "filterRelease", 1.2f },
            { "ampAttack", 0.4f }, { "ampDecay", 1 }, { "ampSustain", 0.9f }, { "ampRelease", 1.5f },
            { "masterGain", -6.5f } },
          { { FxType::phaser, { { phaser::rate, 0.25f }, { phaser::depth, 0.8f }, { phaser::feedback, 0.6f }, { phaser::mix, 0.7f } } },
            { FxType::chorus, { { chorus::mode, 0 }, { chorus::mix, 0.4f } } },
            { FxType::reverb, { { reverb::size, 0.7f }, { reverb::decay, 3 }, { reverb::mix, 0.14f } } } } },

        // ---------------------------------------------------------------- Keys
        { "FM Bell", "Keys", {
            { "osc1Shape", 3 }, { "osc2Shape", 3 }, { "osc2Semi", 17 }, { "osc2Fine", 0 }, { "osc1Level", 1 }, { "osc2Level", 0 },
            { "fmAmount", 0.15f }, { "filterMode", 1 }, { "cutoff", 8000 }, { "resonance", 0 }, { "filterEnvAmt", 0 },
            { "velToCutoff", 0 }, { "ampAttack", 0.002f }, { "ampDecay", 2.5f }, { "ampSustain", 0 }, { "ampRelease", 2 },
            { "mod1Source", 4 }, { "mod1Dest", 6 }, { "mod1Amount", 0.5f },
            { "mod2Source", 5 }, { "mod2Dest", 6 }, { "mod2Amount", 0.2f },
            { "masterGain", -7.5f } },
          { { FxType::chorus, { { chorus::mode, 0 }, { chorus::mix, 0.3f } } },
            { FxType::reverb, { { reverb::size, 0.75f }, { reverb::decay, 3.3f }, { reverb::mix, 0.17f } } } } },

        { "Dusty Keys", "Keys", {
            { "osc1Shape", 2 }, { "osc2Shape", 1 }, { "osc1Level", 0.7f }, { "osc2Level", 0.3f }, { "osc1Pw", 0.3f }, { "osc2Pw", 0.3f }, { "noiseLevel", 0.03f },
            { "cutoff", 1500 }, { "resonance", 0.1f }, { "filterEnvAmt", 0.35f },
            { "filterAttack", 0.002f }, { "filterDecay", 0.8f }, { "filterSustain", 0.2f },
            { "ampAttack", 0.003f }, { "ampDecay", 1.2f }, { "ampSustain", 0.4f }, { "ampRelease", 0.5f },
            { "drift", 0.6f }, { "jitter", 0.4f }, { "spread", 0.6f }, { "warmup", 0.5f }, { "unit", 5 },
            { "masterGain", -4 } },
          { { FxType::chorus, { { chorus::mode, 0 }, { chorus::mix, 0.5f } } },
            { FxType::delay, { { delay::sync, 11 }, { delay::mix, 0.12f }, { delay::tape, 0.7f } } } } },

        { "Ring Bells", "Keys", {
            { "osc1Shape", 1 }, { "osc2Shape", 2 }, { "osc2Semi", 7 }, { "osc2Fine", 13 }, { "osc1Level", 0.8f }, { "osc2Level", 0.2f },
            { "osc1Pw", 0.4f }, { "osc2Pw", 0.4f }, { "ringLevel", 0.8f }, { "cutoff", 5000 }, { "resonance", 0.2f }, { "filterEnvAmt", -0.2f },
            { "ampDecay", 1.5f }, { "ampSustain", 0 }, { "ampRelease", 1.5f },
            { "masterGain", -5.5f } },
          { { FxType::delay, { { delay::sync, 9 }, { delay::feedback, 0.45f }, { delay::mix, 0.2f } } },
            { FxType::reverb, { { reverb::decay, 2.2f }, { reverb::mix, 0.19f } } } } },

        { "Hi-Pass Chord", "Keys", {
            { "osc2Shape", 1 }, { "filterMode", 3 }, { "cutoff", 400 }, { "resonance", 0.3f }, { "filterEnvAmt", 0.2f },
            { "ampDecay", 0.8f }, { "ampSustain", 0.5f },
            { "masterGain", -3 } },
          { { FxType::chorus, { { chorus::mode, 1 }, { chorus::mix, 0.7f } } },
            { FxType::delay, { { delay::sync, 9 }, { delay::mix, 0.2f } } } } },

        { "Crushed Keys", "Keys", {
            { "osc1Shape", 4 }, { "osc1WtPos", 0.5f }, { "osc1Level", 0.75f }, { "osc2Level", 0.25f }, { "cutoff", 2500 }, { "resonance", 0.2f },
            { "filterEnvAmt", 0.3f }, { "filterDecay", 0.5f }, { "filterSustain", 0.1f },
            { "distType", 4 }, { "distDrive", 0.55f }, { "distMix", 0.7f }, { "distTone", 0.6f },
            { "ampDecay", 0.9f }, { "ampSustain", 0.3f }, { "ampRelease", 0.4f },
            { "mod1Source", 5 }, { "mod1Dest", 16 }, { "mod1Amount", 0.3f },
            { "masterGain", -4 } },
          { { FxType::chorus, { { chorus::mode, 1 }, { chorus::mix, 0.5f } } },
            { FxType::delay, { { delay::sync, 9 }, { delay::mix, 0.15f } } } }, "PWM" },

        { "Tonewheel", "Keys", {
            { "osc1Shape", 4 }, { "osc1WtPos", 0.7f }, { "osc2Shape", 4 }, { "osc2WtPos", 0.35f },
            { "osc2Semi", 12 }, { "osc2Fine", 0 }, { "osc1Level", 0.7f }, { "osc2Level", 0.3f },
            { "filterMode", 1 }, { "cutoff", 5000 }, { "resonance", 0 }, { "filterEnvAmt", 0 }, { "velToCutoff", 0 },
            { "ampAttack", 0.004f }, { "ampDecay", 0.3f }, { "ampSustain", 1 }, { "ampRelease", 0.08f }, { "ampVelocity", 0.2f },
            { "distType", 1 }, { "distDrive", 0.25f },
            { "lfo1Rate", 6.2f },
            { "mod1Source", 1 }, { "mod1Dest", 1 }, { "mod1Amount", 0.006f },
            { "mod2Source", 1 }, { "mod2Dest", 14 }, { "mod2Amount", 0.25f },
            { "masterGain", -11.5f } },
          { { FxType::chorus, { { chorus::mode, 2 }, { chorus::mix, 0.5f } } },
            { FxType::reverb, { { reverb::decay, 2.2f }, { reverb::mix, 0.08f } } } }, "Organ", "Organ" },

        { "Vinyl Keys", "Keys", {
            { "osc1Shape", 2 }, { "osc2Shape", 1 }, { "osc1Level", 0.7f }, { "osc2Level", 0.3f }, { "osc1Pw", 0.3f }, { "osc2Pw", 0.3f },
            { "noiseLevel", 0.35f }, { "noiseColor", 3 },
            { "cutoff", 1800 }, { "resonance", 0.1f }, { "filterEnvAmt", 0.3f }, { "filterDecay", 0.7f }, { "filterSustain", 0.2f },
            { "ampAttack", 0.003f }, { "ampDecay", 1.2f }, { "ampSustain", 0.4f }, { "ampRelease", 0.6f },
            { "drift", 0.5f } },
          { { FxType::chorus, { { chorus::mode, 0 }, { chorus::mix, 0.4f } } },
            { FxType::compressor, { { compressor::threshold, -20 }, { compressor::ratio, 3 }, { compressor::makeup, 3 } } },
            { FxType::equalizer, { { equalizer::lowGain, 3 }, { equalizer::mid1Freq, 900 }, { equalizer::mid1Gain, 2 }, { equalizer::mid1Q, 0.8f }, { equalizer::highGain, -6 } } } } },

        // ---------------------------------------------------------------- Pluck
        { "Glass Pluck", "Pluck", {
            { "osc1Shape", 1 }, { "osc2Semi", 12 }, { "osc1Level", 0.7f }, { "osc2Level", 0.3f }, { "osc1Pw", 0.25f }, { "osc2Pw", 0.25f },
            { "cutoff", 900 }, { "resonance", 0.35f }, { "filterEnvAmt", 0.55f }, { "velToCutoff", 0.6f },
            { "filterAttack", 0.001f }, { "filterDecay", 0.25f }, { "filterSustain", 0 }, { "filterRelease", 0.25f },
            { "ampAttack", 0.001f }, { "ampDecay", 0.45f }, { "ampSustain", 0 }, { "ampRelease", 0.35f },
            { "masterGain", -2.5f } },
          { { FxType::delay, { { delay::sync, 8 }, { delay::feedback, 0.45f }, { delay::mix, 0.25f } } },
            { FxType::reverb, { { reverb::decay, 2.2f }, { reverb::mix, 0.11f } } } } },

        { "Arp Pluck", "Pluck", {
            { "osc2Shape", 1 }, { "osc2Fine", 6 }, { "cutoff", 600 }, { "resonance", 0.4f }, { "filterEnvAmt", 0.5f },
            { "filterDecay", 0.18f }, { "filterSustain", 0 },
            { "ampDecay", 0.3f }, { "ampSustain", 0 }, { "ampRelease", 0.2f },
            { "masterGain", -2 } },
          { { FxType::chorus, { { chorus::mode, 0 }, { chorus::mix, 0.4f } } },
            { FxType::delay, { { delay::sync, 6 }, { delay::feedback, 0.3f }, { delay::mix, 0.2f } } } } },

        { "Steel Drum", "Pluck", {
            { "osc1Shape", 4 }, { "osc1WtPos", 0.25f }, { "osc1Level", 1 }, { "osc2On", 0 },
            { "cutoff", 4000 }, { "resonance", 0.1f }, { "filterEnvAmt", 0.3f }, { "velToCutoff", 0.5f },
            { "filterAttack", 0.001f }, { "filterDecay", 0.5f }, { "filterSustain", 0 },
            { "ampAttack", 0.001f }, { "ampDecay", 1 }, { "ampSustain", 0 }, { "ampRelease", 0.8f },
            { "mod1Source", 3 }, { "mod1Dest", 18 }, { "mod1Amount", 0.3f },
            { "masterGain", -1.5f } },
          { { FxType::reverb, { { reverb::size, 0.6f }, { reverb::decay, 2.2f }, { reverb::mix, 0.14f } } } }, "Metallic" },

        // Перенос "PL - Dark Room" (Serum 2, Electric Himalaya): тёмный клубный pluck.
        // Аналоговая таблица в унисоне 4 через грязный ladder; огибающая громкости открывает срез на ~4 октавы
        // и медленно закрывает за 2.3 с. На атаке - короткий суб-удар (Env2 Serum, 0.12 с -> LFO 3 в режиме Env),
        // треугольный корпус и шум (Env3, 0.33 с -> LFO 4), шумовой хвост (Env4, 0.85 с -> огибающая фильтра),
        // щелчок синк-таблицы. Макросы Serum запечены в текущих положениях (TONE 100%, REVERB MIX 48%, NOISY ATTACK 81%).
        // Две реверберации (plate и hall), EQ с подъёмом 2 кГц на атаке, компрессор.
        { "Dark Room", "Pluck", {
            { "osc1Shape", 4 }, { "osc1WtPos", 0.29f }, { "osc1Fine", 0 }, { "osc1Level", 0.68f },
            { "osc2Shape", 4 }, { "osc2WtPos", 0.27f }, { "osc2Fine", 0 }, { "osc2Level", 0 },
            { "osc3On", 1 }, { "osc3Shape", 2 }, { "osc3Fine", 0 }, { "osc3Level", 0 },
            { "noiseColor", 0.3f },
            { "unisonVoices", 4 }, { "unisonDetune", 0.3f }, { "unisonWidth", 0.8f },
            { "cutoff", 500 }, { "resonance", 0.07f }, { "drive", 0.3f }, { "keyTrack", 0.3f },
            { "filterEnvAmt", 0 }, { "velToCutoff", 0.25f },
            { "filterAttack", 0.001f }, { "filterDecay", 0.85f }, { "filterSustain", 0 }, { "filterRelease", 0.17f },
            { "ampAttack", 0.0004f }, { "ampDecay", 2.27f }, { "ampSustain", 0 }, { "ampRelease", 1.34f }, { "ampVelocity", 0.5f },
            // Короткие "огибающие" Serum: LFO в режиме Env со своей формой - быстрый экспоненциальный спад
            { "lfo3Shape", 7 }, { "lfo3Mode", 2 }, { "lfo3Rate", 8.3f },
            { "lfo4Shape", 7 }, { "lfo4Mode", 2 }, { "lfo4Rate", 3.0f },
            // Огибающая громкости открывает срез (Env1 Serum -> cutoff +35% шкалы)
            { "mod1Source", 4 }, { "mod1Dest", 10 }, { "mod1Amount", 0.66f },
            // Суб-удар
            { "mod2Source", 10 }, { "mod2Dest", 8 }, { "mod2Amount", 0.65f }, { "mod2Polarity", 1 },
            // Треугольный корпус и немного шума на атаке
            { "mod3Source", 11 }, { "mod3Dest", 34 }, { "mod3Amount", 0.23f }, { "mod3Polarity", 1 },
            { "mod4Source", 11 }, { "mod4Dest", 9 }, { "mod4Amount", 0.12f }, { "mod4Polarity", 1 },
            // Шумовой хвост за огибающей фильтра
            { "mod5Source", 3 }, { "mod5Dest", 9 }, { "mod5Amount", 0.36f },
            // Щелчок синк-таблицы второго осциллятора
            { "mod6Source", 10 }, { "mod6Dest", 33 }, { "mod6Amount", 0.4f }, { "mod6Polarity", 1 },
            // EQ в рэке (слот 1, ручка M2 Gain): подъём 2 кГц на атаке за огибающей громкости
            { "mod7Source", 4 }, { "mod7Dest", 72 + 1 * 16 + 7 }, { "mod7Amount", 0.15f },
            { "masterGain", -7 } },
          { { FxType::compressor, { { compressor::threshold, -16 }, { compressor::ratio, 2.5f }, { compressor::attack, 0.01f },
                                    { compressor::release, 0.12f }, { compressor::makeup, 2 } } },
            { FxType::equalizer, { { equalizer::mid2Freq, 2000 }, { equalizer::mid2Q, 2.0f }, { equalizer::lowCut, 30 } } },
            { FxType::reverb, { { reverb::size, 0.3f }, { reverb::decay, 1.2f }, { reverb::preDelay, 20 }, { reverb::mix, 0.17f } } },
            { FxType::reverb, { { reverb::size, 0.75f }, { reverb::decay, 2.8f }, { reverb::preDelay, 16 }, { reverb::damping, 0.6f },
                                { reverb::lowCut, 200 }, { reverb::mix, 0.3f } } } },
          "Analog", "Sync",
          { { 2, { { 0.0f, 1.0f, -0.7f }, { 1.0f, -1.0f, 0.0f } } },
            { 3, { { 0.0f, 1.0f, -0.7f }, { 1.0f, -1.0f, 0.0f } } } } },

        // ---------------------------------------------------------------- FX
        { "Broken Tape", "FX", {
            { "osc2Shape", 1 }, { "cutoff", 1400 }, { "resonance", 0.3f },
            { "ampAttack", 0.2f }, { "ampSustain", 0.9f }, { "ampRelease", 1.2f },
            { "drift", 1 }, { "jitter", 0.8f }, { "spread", 1 }, { "sag", 0.8f }, { "warmup", 1 }, { "unit", 7 },
            { "lfo2Shape", 6 }, { "lfo2Rate", 0.7f },
            { "mod1Source", 2 }, { "mod1Dest", 1 }, { "mod1Amount", 0.02f },
            { "masterGain", -5.5f } },
          { { FxType::chorus, { { chorus::mode, 0 } } },
            { FxType::delay, { { delay::sync, 11 }, { delay::feedback, 0.6f }, { delay::mix, 0.35f }, { delay::tape, 1 } } },
            { FxType::reverb, { { reverb::decay, 2.2f }, { reverb::mix, 0.17f } } } } },

        { "Noise Sweep", "FX", {
            { "osc1Shape", 3 }, { "osc1Level", 1 }, { "osc2On", 0 }, { "noiseLevel", 0.9f }, { "filterMode", 2 },
            { "cutoff", 800 }, { "resonance", 0.75f }, { "filterEnvAmt", 0.6f },
            { "filterAttack", 2.5f }, { "filterDecay", 3 }, { "filterSustain", 0.1f }, { "filterRelease", 3 },
            { "ampAttack", 1 }, { "ampSustain", 1 }, { "ampRelease", 3 },
            { "lfo1Shape", 6 }, { "lfo1Rate", 0.5f },
            { "mod1Source", 1 }, { "mod1Dest", 10 }, { "mod1Amount", 0.25f },
            { "masterGain", -10.5f } },
          { { FxType::delay, { { delay::mix, 0.2f } } },
            { FxType::reverb, { { reverb::size, 0.9f }, { reverb::decay, 4.7f }, { reverb::mix, 0.28f } } } } },
    };

    return presets;
}

} // namespace sonder
