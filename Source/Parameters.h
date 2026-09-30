#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace sonder
{

inline constexpr int kNumModSlots = 32;
inline constexpr int kNumLfos = 8;
inline constexpr int kNumOscs = 4;
inline constexpr int kNumFilters = 2;

// Ручки фильтра: у первого фильтра прежние идентификаторы ("cutoff", "resonance"...), у второго "filter2..."
enum class FilterParam { on, mode, cutoff, resonance, drive, vowel, envAmount, keyTrack, velocity, count };

namespace ParamIDs
{
    // Осцилляторы с нуля: "osc1On", "osc1Shape", "osc1WtPos", "osc1Semi", "osc1Fine", "osc1Pw", "osc1Level"...
    juce::String oscOn (int osc);
    juce::String oscShape (int osc);
    juce::String oscWtPos (int osc);
    juce::String oscSemi (int osc);
    juce::String oscFine (int osc);
    juce::String oscPw (int osc);
    juce::String oscLevel (int osc);
    juce::String oscSync (int osc);  // осцилляторы 2-4: сброс фазы по первому (hard sync)

    juce::String filterParam (int filter, FilterParam param);
    inline constexpr auto filterRouting = "filterRouting";

    inline constexpr auto subLevel      = "subLevel";
    inline constexpr auto noiseLevel    = "noiseLevel";
    inline constexpr auto noiseColor    = "noiseColor";
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
    inline constexpr auto distPosition  = "distPosition";

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
    inline constexpr auto glideMode     = "glideMode";
    inline constexpr auto glideCurve    = "glideCurve";
    inline constexpr auto bendRange     = "bendRange";
    inline constexpr auto vibrato       = "vibrato";

    inline constexpr auto masterGain    = "masterGain";

    // Арпеджиатор
    inline constexpr auto arpOn         = "arpOn";
    inline constexpr auto arpMode       = "arpMode";
    inline constexpr auto arpOctaves    = "arpOctaves";
    inline constexpr auto arpRate       = "arpRate";
    inline constexpr auto arpGate       = "arpGate";
    inline constexpr auto arpSwing      = "arpSwing";
    inline constexpr auto arpHold       = "arpHold";

    // MPE: у каждой ноты свой канал с собственными бендом, давлением и слайдом
    inline constexpr auto mpeOn         = "mpeOn";
    inline constexpr auto mpeBendRange  = "mpeBendRange";

    // LFO с нуля: "lfo1Shape", "lfo1Rate", "lfo1Sync", "lfo1Mode"...
    juce::String lfoShape (int lfo);
    juce::String lfoRate (int lfo);
    juce::String lfoSync (int lfo);
    juce::String lfoMode (int lfo);

    // Слоты мод-матрицы с нуля: "mod1Source", "mod1Dest", "mod1Amount"...
    juce::String modSource (int slot);
    juce::String modDest (int slot);
    juce::String modAmount (int slot);
    juce::String modPolarity (int slot);

    // Рэк эффектов: ручки слота ("fx1p1" ... "fx12p16") и его включение ("fx1On")
    juce::String fxParam (int slot, int param);
    juce::String fxOn (int slot);
}

// Списки вариантов для choice-параметров. Порядок совпадает с enum'ами в DSP-коде;
// новые варианты только дописываются в конец, чтобы не ломать сохранённые пресеты.
namespace Choices
{
    const juce::StringArray& oscShapes();
    const juce::StringArray& noiseTypes();
    const juce::StringArray& filterModes();
    const juce::StringArray& distTypes();
    const juce::StringArray& distPositions();
    const juce::StringArray& glideModes();
    const juce::StringArray& filterRoutings();
    const juce::StringArray& lfoShapes();
    const juce::StringArray& lfoModes();
    const juce::StringArray& syncDivisions();
    const juce::StringArray& modSources();
    const juce::StringArray& modDestinations();
    const juce::StringArray& voiceModes();
    const juce::StringArray& modPolarities();
    const juce::StringArray& arpModes();
    const juce::StringArray& arpRates();
}

// Длительность шага арпеджиатора в четвертях (индекс в Choices::arpRates())
double arpRateInBeats (int index);

// Длительность такта синхронизации в четвертях; 0 для "Free"
double syncDivisionInBeats (int index);

class FxRack;

// Параметрам слотов рэка нужен сам рэк: их имена и единицы зависят от эффекта в слоте
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (const FxRack& rack);

// Указатели на значения параметров для чтения из аудиопотока без поиска по строкам
struct ParameterRefs
{
    using Ref = std::atomic<float>*;

    explicit ParameterRefs (juce::AudioProcessorValueTreeState& state);

    std::array<Ref, kNumOscs> oscOn, oscShape, oscWtPos, oscSemi, oscFine, oscPw, oscLevel, oscSync;
    Ref subLevel, noiseLevel, noiseColor, fmAmount, ringLevel, foldAmount;

    std::array<std::array<Ref, (size_t) FilterParam::count>, kNumFilters> filters {};
    Ref filterRouting;
    Ref distType, distDrive, distMix, distTone, distPosition;

    Ref filterAttack, filterDecay, filterSustain, filterRelease;
    Ref ampAttack, ampDecay, ampSustain, ampRelease, ampVelocity;

    std::array<Ref, kNumLfos> lfoShape, lfoRate, lfoSync, lfoMode;
    std::array<Ref, kNumModSlots> modSource, modDest, modAmount, modPolarity;

    Ref drift, jitter, spread, sag, warmup, unit;
    Ref voiceMode, unisonVoices, unisonDetune, unisonWidth, glide, glideMode, glideCurve, bendRange, vibrato;

    // Рэк эффектов: нормированные (0..1) значения ручек слотов и флаги включения
    std::array<std::array<Ref, 16>, 12> fxParams;
    std::array<Ref, 12> fxOn;
    Ref masterGain;

    Ref arpOn, arpMode, arpOctaves, arpRate, arpGate, arpSwing, arpHold;
    Ref mpeOn, mpeBendRange;
};

} // namespace sonder
