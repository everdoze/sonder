#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace sonder
{

inline constexpr int kNumModSlots = 32;
inline constexpr int kNumLfos = 8;

namespace ParamIDs
{
    inline constexpr auto osc1Shape     = "osc1Shape";
    inline constexpr auto osc2Shape     = "osc2Shape";
    inline constexpr auto osc1WtPos     = "osc1WtPos";
    inline constexpr auto osc2WtPos     = "osc2WtPos";
    inline constexpr auto osc2Semi      = "osc2Semi";
    inline constexpr auto osc2Fine      = "osc2Fine";
    inline constexpr auto oscMix        = "oscMix";
    inline constexpr auto pulseWidth    = "pulseWidth";
    inline constexpr auto subLevel      = "subLevel";
    inline constexpr auto noiseLevel    = "noiseLevel";
    inline constexpr auto fmAmount      = "fmAmount";
    inline constexpr auto ringLevel     = "ringLevel";
    inline constexpr auto foldAmount    = "foldAmount";

    inline constexpr auto filterMode    = "filterMode";
    inline constexpr auto cutoff        = "cutoff";
    inline constexpr auto resonance     = "resonance";
    inline constexpr auto drive         = "drive";
    inline constexpr auto vowel         = "vowel";
    inline constexpr auto filterEnvAmt  = "filterEnvAmt";
    inline constexpr auto keyTrack      = "keyTrack";
    inline constexpr auto velToCutoff   = "velToCutoff";

    inline constexpr auto distType      = "distType";
    inline constexpr auto distDrive     = "distDrive";
    inline constexpr auto distMix       = "distMix";
    inline constexpr auto distTone      = "distTone";

    inline constexpr auto filterAttack  = "filterAttack";
    inline constexpr auto filterDecay   = "filterDecay";
    inline constexpr auto filterSustain = "filterSustain";
    inline constexpr auto filterRelease = "filterRelease";

    inline constexpr auto ampAttack     = "ampAttack";
    inline constexpr auto ampDecay      = "ampDecay";
    inline constexpr auto ampSustain    = "ampSustain";
    inline constexpr auto ampRelease    = "ampRelease";
    inline constexpr auto ampVelocity   = "ampVelocity";

    inline constexpr auto drift         = "drift";
    inline constexpr auto jitter        = "jitter";
    inline constexpr auto spread        = "spread";
    inline constexpr auto sag           = "sag";
    inline constexpr auto warmup        = "warmup";
    inline constexpr auto unit          = "unit";

    inline constexpr auto voiceMode     = "voiceMode";
    inline constexpr auto unisonVoices  = "unisonVoices";
    inline constexpr auto unisonDetune  = "unisonDetune";
    inline constexpr auto unisonWidth   = "unisonWidth";
    inline constexpr auto glide         = "glide";
    inline constexpr auto bendRange     = "bendRange";
    inline constexpr auto vibrato       = "vibrato";

    inline constexpr auto chorusMode    = "chorusMode";
    inline constexpr auto chorusMix     = "chorusMix";
    inline constexpr auto delaySync     = "delaySync";
    inline constexpr auto delayTime     = "delayTime";
    inline constexpr auto delayFeedback = "delayFeedback";
    inline constexpr auto delayMix      = "delayMix";
    inline constexpr auto delayTape     = "delayTape";
    inline constexpr auto reverbSize    = "reverbSize";
    inline constexpr auto reverbMix     = "reverbMix";

    inline constexpr auto masterGain    = "masterGain";

    // LFO с нуля: "lfo1Shape", "lfo1Rate", "lfo1Sync", "lfo1Mode"...
    juce::String lfoShape (int lfo);
    juce::String lfoRate (int lfo);
    juce::String lfoSync (int lfo);
    juce::String lfoMode (int lfo);

    // Слоты мод-матрицы с нуля: "mod1Source", "mod1Dest", "mod1Amount"...
    juce::String modSource (int slot);
    juce::String modDest (int slot);
    juce::String modAmount (int slot);
}

// Списки вариантов для choice-параметров. Порядок совпадает с enum'ами в DSP-коде;
// новые варианты только дописываются в конец, чтобы не ломать сохранённые пресеты.
namespace Choices
{
    const juce::StringArray& oscShapes();
    const juce::StringArray& filterModes();
    const juce::StringArray& distTypes();
    const juce::StringArray& lfoShapes();
    const juce::StringArray& lfoModes();
    const juce::StringArray& syncDivisions();
    const juce::StringArray& modSources();
    const juce::StringArray& modDestinations();
    const juce::StringArray& voiceModes();
    const juce::StringArray& chorusModes();
}

// Длительность такта синхронизации в четвертях; 0 для "Free"
double syncDivisionInBeats (int index);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Указатели на значения параметров для чтения из аудиопотока без поиска по строкам
struct ParameterRefs
{
    using Ref = std::atomic<float>*;

    explicit ParameterRefs (juce::AudioProcessorValueTreeState& state);

    Ref osc1Shape, osc2Shape, osc1WtPos, osc2WtPos, osc2Semi, osc2Fine, oscMix, pulseWidth;
    Ref subLevel, noiseLevel, fmAmount, ringLevel, foldAmount;

    Ref filterMode, cutoff, resonance, drive, vowel, filterEnvAmt, keyTrack, velToCutoff;
    Ref distType, distDrive, distMix, distTone;

    Ref filterAttack, filterDecay, filterSustain, filterRelease;
    Ref ampAttack, ampDecay, ampSustain, ampRelease, ampVelocity;

    std::array<Ref, kNumLfos> lfoShape, lfoRate, lfoSync, lfoMode;
    std::array<Ref, kNumModSlots> modSource, modDest, modAmount;

    Ref drift, jitter, spread, sag, warmup, unit;
    Ref voiceMode, unisonVoices, unisonDetune, unisonWidth, glide, bendRange, vibrato;

    Ref chorusMode, chorusMix, delaySync, delayTime, delayFeedback, delayMix, delayTape, reverbSize, reverbMix;
    Ref masterGain;
};

} // namespace sonder
