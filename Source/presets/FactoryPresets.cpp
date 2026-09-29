#include "FactoryPresets.h"

namespace sonder
{

// Индексы вариантов (см. Parameters.cpp):
//   формы осцилляторов: 0 Saw, 1 Pulse, 2 Triangle, 3 Sine
//   фильтр: 0 LP24, 1 LP12, 2 Band, 3 High;  режим голосов: 0 Poly, 1 Mono, 2 Legato
//   формы LFO: 0 Sine, 1 Triangle, 2 Saw Up, 3 Saw Down, 4 Square, 5 S&H, 6 Smooth
//   синхронизация: 0 Free, 3 1/16, 6 1/8, 8 1/8., 9 1/4, 11 1/4., 12 1/2, 13 1/1
//   источники: 1 LFO1, 2 LFO2, 3 FEnv, 4 AEnv, 5 Vel, 6 MW, 7 AT, 8 Key, 9 Random
//   цели: 1 Pitch, 2 Osc1, 3 Osc2, 4 PW, 5 Mix, 6 FM, 7 Fold, 8 Sub, 9 Noise, 10 Cutoff, 11 Reso, 12 Drive, 13 Amp, 14 Pan
//   хорус: 0 Off, 1 I, 2 II, 3 I+II

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets
    {
        { "Init", "Init", {} },

        // ---------------------------------------------------------------- Bass
        { "Moog Bass", "Bass", {
            { "voiceMode", 1 }, { "osc2Shape", 1 }, { "osc2Semi", -12 }, { "osc2Fine", 0 }, { "oscMix", 0.45f },
            { "subLevel", 0.5f }, { "cutoff", 180 }, { "resonance", 0.45f }, { "drive", 0.55f },
            { "filterEnvAmt", 0.55f }, { "keyTrack", 0.6f }, { "velToCutoff", 0.4f },
            { "filterAttack", 0.002f }, { "filterDecay", 0.35f }, { "filterSustain", 0.1f }, { "filterRelease", 0.15f },
            { "ampAttack", 0.002f }, { "ampDecay", 0.5f }, { "ampSustain", 0.75f }, { "ampRelease", 0.12f },
            { "glide", 0.06f }, { "spread", 0.2f } , { "masterGain", -7 } } },

        { "Sub Floor", "Bass", {
            { "voiceMode", 1 }, { "osc1Shape", 2 }, { "osc2Shape", 3 }, { "osc2Semi", -12 }, { "osc2Fine", 0 },
            { "subLevel", 0.6f }, { "cutoff", 500 }, { "resonance", 0 }, { "drive", 0.5f }, { "filterEnvAmt", 0.15f },
            { "filterDecay", 0.3f }, { "ampAttack", 0.003f }, { "ampSustain", 1 }, { "ampRelease", 0.1f },
            { "glide", 0.03f } } },

        { "Acid Line", "Bass", {
            { "voiceMode", 2 }, { "oscMix", 0 }, { "cutoff", 350 }, { "resonance", 0.8f }, { "drive", 0.45f },
            { "filterEnvAmt", 0.6f }, { "velToCutoff", 0.5f },
            { "filterAttack", 0.001f }, { "filterDecay", 0.25f }, { "filterSustain", 0 }, { "filterRelease", 0.2f },
            { "ampAttack", 0.001f }, { "ampDecay", 0.3f }, { "ampSustain", 0.7f }, { "ampRelease", 0.08f },
            { "glide", 0.08f }, { "delaySync", 8 }, { "delayFeedback", 0.3f }, { "delayMix", 0.15f } , { "masterGain", -6 } } },

        { "Wobble", "Bass", {
            { "voiceMode", 1 }, { "osc2Semi", -12 }, { "osc2Fine", 5 }, { "subLevel", 0.4f }, { "foldAmount", 0.15f },
            { "cutoff", 250 }, { "resonance", 0.55f }, { "drive", 0.6f }, { "filterEnvAmt", 0.1f },
            { "ampSustain", 1 }, { "ampRelease", 0.15f },
            { "lfo1Shape", 0 }, { "lfo1Sync", 6 },
            { "mod1Source", 1 }, { "mod1Dest", 10 }, { "mod1Amount", 0.45f },
            { "mod2Source", 6 }, { "mod2Dest", 7 }, { "mod2Amount", 0.5f } , { "masterGain", -7 } } },

        { "Folded Growl", "Bass", {
            { "voiceMode", 1 }, { "osc1Shape", 2 }, { "osc2Shape", 3 }, { "osc2Semi", -12 }, { "osc2Fine", 0 },
            { "foldAmount", 0.45f }, { "cutoff", 1200 }, { "resonance", 0.3f }, { "drive", 0.6f }, { "filterEnvAmt", 0.3f },
            { "filterDecay", 0.4f }, { "filterSustain", 0.2f }, { "ampSustain", 1 }, { "ampRelease", 0.15f },
            { "lfo1Shape", 6 }, { "lfo1Rate", 4 }, { "glide", 0.04f },
            { "mod1Source", 3 }, { "mod1Dest", 7 }, { "mod1Amount", 0.4f },
            { "mod2Source", 1 }, { "mod2Dest", 7 }, { "mod2Amount", 0.2f } } },

        // ---------------------------------------------------------------- Lead
        { "Detuned Lead", "Lead", {
            { "voiceMode", 2 }, { "osc2Fine", 12 }, { "unisonVoices", 3 }, { "unisonDetune", 0.35f }, { "unisonWidth", 0.5f },
            { "cutoff", 2600 }, { "resonance", 0.3f }, { "filterEnvAmt", 0.25f }, { "filterDecay", 0.5f }, { "filterSustain", 0.6f },
            { "ampAttack", 0.01f }, { "ampSustain", 1 }, { "ampRelease", 0.25f }, { "glide", 0.1f }, { "vibrato", 0.5f },
            { "delaySync", 8 }, { "delayFeedback", 0.4f }, { "delayMix", 0.22f }, { "reverbMix", 0.15f } } },

        { "Screamer", "Lead", {
            { "voiceMode", 1 }, { "osc2Semi", 7 }, { "osc2Fine", 0 }, { "oscMix", 0.4f }, { "fmAmount", 0.35f },
            { "cutoff", 1800 }, { "resonance", 0.5f }, { "drive", 0.7f }, { "filterEnvAmt", 0.3f },
            { "ampSustain", 1 }, { "ampRelease", 0.2f }, { "glide", 0.05f },
            { "mod1Source", 3 }, { "mod1Dest", 6 }, { "mod1Amount", 0.4f },
            { "mod2Source", 6 }, { "mod2Dest", 7 }, { "mod2Amount", 0.5f },
            { "delaySync", 9 }, { "delayMix", 0.18f } , { "masterGain", -7 } } },

        { "Supersaw", "Lead", {
            { "osc2Fine", 14 }, { "unisonVoices", 4 }, { "unisonDetune", 0.6f }, { "unisonWidth", 1 },
            { "cutoff", 6000 }, { "resonance", 0.1f }, { "filterEnvAmt", 0.1f },
            { "ampAttack", 0.01f }, { "ampSustain", 1 }, { "ampRelease", 0.5f },
            { "chorusMode", 1 }, { "chorusMix", 0.3f }, { "delaySync", 8 }, { "delayMix", 0.15f }, { "reverbMix", 0.25f },
            { "masterGain", -12 } } },

        { "Hollow Reed", "Lead", {
            { "voiceMode", 1 }, { "osc1Shape", 1 }, { "osc2Shape", 2 }, { "oscMix", 0.3f }, { "pulseWidth", 0.2f },
            { "filterMode", 1 }, { "cutoff", 1100 }, { "resonance", 0.35f }, { "filterEnvAmt", 0.2f },
            { "ampAttack", 0.04f }, { "ampSustain", 0.9f }, { "ampRelease", 0.2f }, { "vibrato", 0.6f },
            { "lfo1Rate", 5 },
            { "mod1Source", 7 }, { "mod1Dest", 10 }, { "mod1Amount", 0.3f },
            { "mod2Source", 1 }, { "mod2Dest", 4 }, { "mod2Amount", 0.15f },
            { "reverbMix", 0.2f } } },

        { "Poly Brass", "Lead", {
            { "osc2Fine", 8 }, { "cutoff", 700 }, { "resonance", 0.15f }, { "filterEnvAmt", 0.5f }, { "velToCutoff", 0.5f },
            { "filterAttack", 0.08f }, { "filterDecay", 0.6f }, { "filterSustain", 0.45f }, { "filterRelease", 0.3f },
            { "ampAttack", 0.05f }, { "ampDecay", 0.5f }, { "ampSustain", 0.85f }, { "ampRelease", 0.3f },
            { "chorusMode", 1 }, { "chorusMix", 0.4f }, { "reverbMix", 0.15f } , { "masterGain", -11 } } },

        // ---------------------------------------------------------------- Pad
        { "Juno Strings", "Pad", {
            { "osc2Shape", 1 }, { "osc2Fine", 5 }, { "oscMix", 0.4f }, { "pulseWidth", 0.35f },
            { "cutoff", 3200 }, { "resonance", 0.1f }, { "filterEnvAmt", 0.1f },
            { "filterAttack", 0.6f }, { "filterDecay", 1.5f }, { "filterSustain", 0.7f }, { "filterRelease", 1.2f },
            { "ampAttack", 0.45f }, { "ampDecay", 1 }, { "ampSustain", 0.85f }, { "ampRelease", 1.4f },
            { "mod1Source", 2 }, { "mod1Dest", 4 }, { "mod1Amount", 0.25f },
            { "drift", 0.35f }, { "chorusMode", 2 }, { "chorusMix", 0.8f }, { "reverbSize", 0.7f }, { "reverbMix", 0.2f } , { "masterGain", -13 } } },

        { "Warm Pad", "Pad", {
            { "osc2Fine", 9 }, { "subLevel", 0.3f }, { "cutoff", 900 }, { "resonance", 0.2f }, { "filterEnvAmt", 0.25f },
            { "filterAttack", 1.2f }, { "filterDecay", 2.5f }, { "filterSustain", 0.5f }, { "filterRelease", 2 },
            { "ampAttack", 1 }, { "ampDecay", 2 }, { "ampSustain", 0.9f }, { "ampRelease", 2.5f },
            { "unisonVoices", 2 }, { "unisonDetune", 0.3f }, { "unisonWidth", 0.8f },
            { "lfo1Shape", 1 }, { "lfo1Rate", 0.25f },
            { "mod1Source", 1 }, { "mod1Dest", 10 }, { "mod1Amount", 0.06f },
            { "drift", 0.45f }, { "chorusMode", 1 }, { "chorusMix", 0.5f }, { "reverbSize", 0.8f }, { "reverbMix", 0.35f } } },

        { "Sweep Pad", "Pad", {
            { "osc2Fine", 10 }, { "unisonVoices", 2 }, { "cutoff", 400 }, { "resonance", 0.55f }, { "filterEnvAmt", 0.2f },
            { "filterAttack", 2 }, { "filterSustain", 0.6f }, { "filterRelease", 3 },
            { "ampAttack", 1.5f }, { "ampSustain", 1 }, { "ampRelease", 3 },
            { "lfo1Rate", 0.3f }, { "lfo2Shape", 1 }, { "lfo2Rate", 0.08f },
            { "mod1Source", 2 }, { "mod1Dest", 10 }, { "mod1Amount", 0.35f },
            { "mod2Source", 1 }, { "mod2Dest", 14 }, { "mod2Amount", 0.3f },
            { "chorusMode", 2 }, { "chorusMix", 0.5f }, { "delaySync", 11 }, { "delayFeedback", 0.5f }, { "delayMix", 0.15f },
            { "reverbSize", 0.85f }, { "reverbMix", 0.45f } } },

        { "Space Drone", "Pad", {
            { "osc2Semi", -12 }, { "osc2Fine", 3 }, { "subLevel", 0.3f }, { "noiseLevel", 0.1f },
            { "cutoff", 300 }, { "resonance", 0.6f }, { "drive", 0.4f }, { "filterEnvAmt", 0 },
            { "ampAttack", 3 }, { "ampSustain", 1 }, { "ampRelease", 5 },
            { "lfo1Shape", 6 }, { "lfo1Rate", 0.15f }, { "lfo2Shape", 0 }, { "lfo2Rate", 0.05f },
            { "mod1Source", 1 }, { "mod1Dest", 10 }, { "mod1Amount", 0.3f },
            { "mod2Source", 2 }, { "mod2Dest", 3 }, { "mod2Amount", 0.01f },
            { "mod3Source", 2 }, { "mod3Dest", 7 }, { "mod3Amount", 0.15f },
            { "drift", 0.6f }, { "chorusMode", 1 }, { "chorusMix", 0.6f },
            { "delaySync", 13 }, { "delayFeedback", 0.6f }, { "delayMix", 0.25f }, { "reverbSize", 0.95f }, { "reverbMix", 0.6f } } },

        // ---------------------------------------------------------------- Keys
        { "FM Bell", "Keys", {
            { "osc1Shape", 3 }, { "osc2Shape", 3 }, { "osc2Semi", 17 }, { "osc2Fine", 0 }, { "oscMix", 0 },
            { "fmAmount", 0.15f }, { "filterMode", 1 }, { "cutoff", 8000 }, { "resonance", 0 }, { "filterEnvAmt", 0 },
            { "velToCutoff", 0 }, { "ampAttack", 0.002f }, { "ampDecay", 2.5f }, { "ampSustain", 0 }, { "ampRelease", 2 },
            { "mod1Source", 4 }, { "mod1Dest", 6 }, { "mod1Amount", 0.5f },
            { "mod2Source", 5 }, { "mod2Dest", 6 }, { "mod2Amount", 0.2f },
            { "chorusMode", 1 }, { "chorusMix", 0.3f }, { "reverbSize", 0.75f }, { "reverbMix", 0.3f } , { "masterGain", -13 } } },

        { "Dusty Keys", "Keys", {
            { "osc1Shape", 2 }, { "osc2Shape", 1 }, { "oscMix", 0.3f }, { "pulseWidth", 0.3f }, { "noiseLevel", 0.03f },
            { "cutoff", 1500 }, { "resonance", 0.1f }, { "filterEnvAmt", 0.35f },
            { "filterAttack", 0.002f }, { "filterDecay", 0.8f }, { "filterSustain", 0.2f },
            { "ampAttack", 0.003f }, { "ampDecay", 1.2f }, { "ampSustain", 0.4f }, { "ampRelease", 0.5f },
            { "drift", 0.6f }, { "jitter", 0.4f }, { "spread", 0.6f }, { "warmup", 0.5f }, { "unit", 5 },
            { "chorusMode", 1 }, { "chorusMix", 0.5f },
            { "delaySync", 11 }, { "delayMix", 0.12f }, { "delayTape", 0.7f } , { "masterGain", -4 } } },

        { "Ring Bells", "Keys", {
            { "osc1Shape", 1 }, { "osc2Shape", 2 }, { "osc2Semi", 7 }, { "osc2Fine", 13 }, { "oscMix", 0.2f },
            { "pulseWidth", 0.4f }, { "ringLevel", 0.8f }, { "cutoff", 5000 }, { "resonance", 0.2f }, { "filterEnvAmt", -0.2f },
            { "ampDecay", 1.5f }, { "ampSustain", 0 }, { "ampRelease", 1.5f },
            { "delaySync", 9 }, { "delayFeedback", 0.45f }, { "delayMix", 0.2f }, { "reverbMix", 0.35f } , { "masterGain", -11 } } },

        { "Hi-Pass Chord", "Keys", {
            { "osc2Shape", 1 }, { "filterMode", 3 }, { "cutoff", 400 }, { "resonance", 0.3f }, { "filterEnvAmt", 0.2f },
            { "ampDecay", 0.8f }, { "ampSustain", 0.5f },
            { "chorusMode", 2 }, { "chorusMix", 0.7f }, { "delaySync", 9 }, { "delayMix", 0.2f } , { "masterGain", -3 } } },

        // ---------------------------------------------------------------- Pluck
        { "Glass Pluck", "Pluck", {
            { "osc1Shape", 1 }, { "osc2Semi", 12 }, { "oscMix", 0.3f }, { "pulseWidth", 0.25f },
            { "cutoff", 900 }, { "resonance", 0.35f }, { "filterEnvAmt", 0.55f }, { "velToCutoff", 0.6f },
            { "filterAttack", 0.001f }, { "filterDecay", 0.25f }, { "filterSustain", 0 }, { "filterRelease", 0.25f },
            { "ampAttack", 0.001f }, { "ampDecay", 0.45f }, { "ampSustain", 0 }, { "ampRelease", 0.35f },
            { "delaySync", 8 }, { "delayFeedback", 0.45f }, { "delayMix", 0.25f }, { "reverbMix", 0.2f } , { "masterGain", -5 } } },

        { "Arp Pluck", "Pluck", {
            { "osc2Shape", 1 }, { "osc2Fine", 6 }, { "cutoff", 600 }, { "resonance", 0.4f }, { "filterEnvAmt", 0.5f },
            { "filterDecay", 0.18f }, { "filterSustain", 0 },
            { "ampDecay", 0.3f }, { "ampSustain", 0 }, { "ampRelease", 0.2f },
            { "chorusMode", 1 }, { "chorusMix", 0.4f }, { "delaySync", 6 }, { "delayFeedback", 0.3f }, { "delayMix", 0.2f } , { "masterGain", -2 } } },

        // ---------------------------------------------------------------- FX
        { "Broken Tape", "FX", {
            { "osc2Shape", 1 }, { "cutoff", 1400 }, { "resonance", 0.3f },
            { "ampAttack", 0.2f }, { "ampSustain", 0.9f }, { "ampRelease", 1.2f },
            { "drift", 1 }, { "jitter", 0.8f }, { "spread", 1 }, { "sag", 0.8f }, { "warmup", 1 }, { "unit", 7 },
            { "lfo2Shape", 6 }, { "lfo2Rate", 0.7f },
            { "mod1Source", 2 }, { "mod1Dest", 1 }, { "mod1Amount", 0.02f },
            { "chorusMode", 1 }, { "delaySync", 11 }, { "delayFeedback", 0.6f }, { "delayMix", 0.35f }, { "delayTape", 1 },
            { "reverbMix", 0.3f } , { "masterGain", -11 } } },

        { "Noise Sweep", "FX", {
            { "osc1Shape", 3 }, { "oscMix", 0 }, { "noiseLevel", 0.9f }, { "filterMode", 2 },
            { "cutoff", 800 }, { "resonance", 0.75f }, { "filterEnvAmt", 0.6f },
            { "filterAttack", 2.5f }, { "filterDecay", 3 }, { "filterSustain", 0.1f }, { "filterRelease", 3 },
            { "ampAttack", 1 }, { "ampSustain", 1 }, { "ampRelease", 3 },
            { "lfo1Shape", 6 }, { "lfo1Rate", 0.5f },
            { "mod1Source", 1 }, { "mod1Dest", 10 }, { "mod1Amount", 0.25f },
            { "delayMix", 0.2f }, { "reverbSize", 0.9f }, { "reverbMix", 0.5f } , { "masterGain", -16 } } },
    };

    return presets;
}

} // namespace sonder
