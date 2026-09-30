#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace sonder
{

inline constexpr int kNumFxSlots = 12;
inline constexpr int kNumFxParams = 16;

// Виды эффектов. Новые только дописываются в конец; в состоянии вид хранится строковым id.
enum class FxType { none, distortion, phaser, flanger, chorus, delay, compressor, reverb, equalizer,
                    filter, tremolo, widener, limiter, multiband, shifter, count };

// Описание одной ручки эффекта. В параметрах слота значение хранится нормированным (0..1),
// а в реальные единицы переводится по этому описанию.
struct FxParamInfo
{
    enum class Format { percent, bipolarPercent, hertz, seconds, milliseconds, decibels, signedDecibels, ratio, plain, choice,
                        octaves, multiplier, signedHertz };

    juce::String label;          // короткая подпись под ручкой
    float minimum = 0.0f, maximum = 1.0f;
    float centre = 0.0f;         // значение в середине хода ручки; 0 - линейная шкала
    float defaultValue = 0.0f;
    Format format = Format::percent;
    juce::StringArray choices;   // для Format::choice

    bool isChoice() const noexcept { return format == Format::choice; }

    juce::NormalisableRange<float> range() const;
    float toReal (float normalised) const;
    float toNormalised (float real) const;
    juce::String text (float real) const;
};

struct FxTypeInfo
{
    FxType type = FxType::none;
    juce::String id;    // стабильный идентификатор для сохранения
    juce::String name;  // название в интерфейсе
    std::vector<FxParamInfo> params;
};

const FxTypeInfo& getFxTypeInfo (FxType type);
FxType fxTypeFromId (const juce::String& id);

// Индексы ручек по видам эффектов (порядок совпадает с описаниями в FxTypes.cpp)
namespace fxp
{
    namespace distortion { enum { type, drive, tone, mix, level }; }
    namespace phaser     { enum { rate, depth, feedback, centre, stages, spread, mix }; }
    namespace flanger    { enum { rate, depth, feedback, delay, spread, mix }; }
    namespace chorus     { enum { mode, rate, depth, tone, width, mix }; }
    namespace delay      { enum { sync, time, feedback, mix, tape, lowCut, highCut, width, offset }; }
    namespace compressor { enum { threshold, ratio, attack, release, knee, makeup, mix, sidechainHp }; }
    namespace reverb     { enum { size, decay, preDelay, damping, lowCut, width, modulation, mix }; }
    namespace equalizer  { enum { lowCut, lowFreq, lowGain, mid1Freq, mid1Gain, mid1Q, mid2Freq, mid2Gain, mid2Q,
                                  highFreq, highGain, highCut }; }
    namespace filter     { enum { mode, cutoff, resonance, drive, sync, rate, depth, shape, spread, mix }; }
    namespace tremolo    { enum { mode, sync, rate, depth, shape, phase, smooth }; }
    namespace widener    { enum { width, spread, time, bassMono }; }
    namespace limiter    { enum { gain, ceiling, release }; }
    namespace multiband  { enum { depth, time, lowSplit, highSplit, upward, downward, lowGain, midGain, highGain, output }; }
    namespace shifter    { enum { shift, fine, feedback, spread, mix }; }
}

} // namespace sonder
