#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace sonder
{

inline constexpr int kNumModSlots = 6;

namespace ParamIDs
{
    inline constexpr auto osc1Shape     = "osc1Shape";
    inline constexpr auto osc2Shape     = "osc2Shape";
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
    inline constexpr auto filterEnvAmt  = "filterEnvAmt";
    inline constexpr auto keyTrack      = "keyTrack";
    inline constexpr auto velToCutoff   = "velToCutoff";

    inline constexpr auto filterAttack  = "filterAttack";
    inline constexpr auto filterDecay   = "filterDecay";
    inline constexpr auto filterSustain = "filterSustain";
    inline constexpr auto filterRelease = "filterRelease";

    inline constexpr auto ampAttack     = "ampAttack";
    inline constexpr auto ampDecay      = "ampDecay";
    inline constexpr auto ampSustain    = "ampSustain";
    inline constexpr auto ampRelease    = "ampRelease";
    inline constexpr auto ampVelocity   = "ampVelocity";

    inline constexpr auto lfo1Shape     = "lfo1Shape";
    inline constexpr auto lfo1Rate      = "lfo1Rate";
    inline constexpr auto lfo1Sync      = "lfo1Sync";
    inline constexpr auto lfo2Shape     = "lfo2Shape";
    inline constexpr auto lfo2Rate      = "lfo2Rate";
    inline constexpr auto lfo2Sync      = "lfo2Sync";

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

    // Слоты мод-матрицы, slot с нуля: "mod1Source", "mod1Dest", "mod1Amount"...
    juce::String modSource (int slot);
    juce::String modDest (int slot);
    juce::String modAmount (int slot);
}

// Списки вариантов для choice-параметров. Порядок совпадает с enum'ами в DSP-коде.
namespace Choices
{
    const juce::StringArray& oscShapes();
    const juce::StringArray& filterModes();
    const juce::StringArray& lfoShapes();
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
    explicit ParameterRefs (juce::AudioProcessorValueTreeState& state);

    std::atomic<float>* osc1Shape;
    std::atomic<float>* osc2Shape;
    std::atomic<float>* osc2Semi;
    std::atomic<float>* osc2Fine;
    std::atomic<float>* oscMix;
    std::atomic<float>* pulseWidth;
    std::atomic<float>* subLevel;
    std::atomic<float>* noiseLevel;
    std::atomic<float>* fmAmount;
    std::atomic<float>* ringLevel;
    std::atomic<float>* foldAmount;

    std::atomic<float>* filterMode;
    std::atomic<float>* cutoff;
    std::atomic<float>* resonance;
    std::atomic<float>* drive;
    std::atomic<float>* filterEnvAmt;
    std::atomic<float>* keyTrack;
    std::atomic<float>* velToCutoff;

    std::atomic<float>* filterAttack;
    std::atomic<float>* filterDecay;
    std::atomic<float>* filterSustain;
    std::atomic<float>* filterRelease;

    std::atomic<float>* ampAttack;
    std::atomic<float>* ampDecay;
    std::atomic<float>* ampSustain;
    std::atomic<float>* ampRelease;
    std::atomic<float>* ampVelocity;

    std::atomic<float>* lfo1Shape;
    std::atomic<float>* lfo1Rate;
    std::atomic<float>* lfo1Sync;
    std::atomic<float>* lfo2Shape;
    std::atomic<float>* lfo2Rate;
    std::atomic<float>* lfo2Sync;

    std::array<std::atomic<float>*, kNumModSlots> modSource;
    std::array<std::atomic<float>*, kNumModSlots> modDest;
    std::array<std::atomic<float>*, kNumModSlots> modAmount;

    std::atomic<float>* drift;
    std::atomic<float>* jitter;
    std::atomic<float>* spread;
    std::atomic<float>* sag;
    std::atomic<float>* warmup;
    std::atomic<float>* unit;

    std::atomic<float>* voiceMode;
    std::atomic<float>* unisonVoices;
    std::atomic<float>* unisonDetune;
    std::atomic<float>* unisonWidth;
    std::atomic<float>* glide;
    std::atomic<float>* bendRange;
    std::atomic<float>* vibrato;

    std::atomic<float>* chorusMode;
    std::atomic<float>* chorusMix;
    std::atomic<float>* delaySync;
    std::atomic<float>* delayTime;
    std::atomic<float>* delayFeedback;
    std::atomic<float>* delayMix;
    std::atomic<float>* delayTape;
    std::atomic<float>* reverbSize;
    std::atomic<float>* reverbMix;

    std::atomic<float>* masterGain;
};

} // namespace sonder
