#pragma once

#include "Parameters.h"
#include "dsp/Distortion.h"
#include "dsp/LadderFilter.h"
#include "dsp/LfoShapes.h"
#include "dsp/NoiseGenerator.h"
#include "dsp/PolyBlepOscillator.h"
#include "fx/FxTypes.h"

#include <array>

namespace sonder
{

// Сколько первых точек пользовательской формы каждого LFO можно модулировать
inline constexpr int kMaxModulatedPoints = 16;

// Порядок совпадает с Choices::modSources() / modDestinations()
enum class ModSource { off, lfo1, lfo2, filterEnv, ampEnv, velocity, modWheel, aftertouch, key, random,
                       lfo3, lfo4, lfo5, lfo6, lfo7, lfo8, slide, count };

// Цели матрицы. До filter2Vowel у каждой цели свой масштаб (октавы среза, полутоны высоты...),
// начиная с ampAttack модуляция считается в долях хода ручки: 100% - вся шкала.
// Цели от master до конца ручек рэка общие для всего синта (выход, аналоговые "болезни", ручки рэка):
// их считает процессор, а не голос. Последними идут точки форм LFO (высота и положение каждой точки) -
// они снова голосовые.
enum class ModDest { off, pitch, osc1Pitch, osc2Pitch, pulseWidth, osc1Level, fm, fold, sub, noise,
                     cutoff, resonance, drive, amp, pan, vowel, distDrive, distMix, osc1WtPos, osc2WtPos,
                     lfo1Rate, lfo2Rate, lfo3Rate, lfo4Rate, lfo5Rate, lfo6Rate, lfo7Rate, lfo8Rate,
                     noiseColor, osc3Pitch, osc4Pitch, osc3WtPos, osc4WtPos, osc2Level, osc3Level, osc4Level,
                     filter2Cutoff, filter2Resonance, filter2Drive, filter2Vowel,
                     ampAttack, ampDecay, ampSustain, ampRelease, ampVelocity,
                     filterEnvAttack, filterEnvDecay, filterEnvSustain, filterEnvRelease,
                     filter1EnvAmount, filter1KeyTrack, filter1Velocity, filter2EnvAmount, filter2KeyTrack, filter2Velocity,
                     distTone, ring, osc1Fine, osc2Fine, osc3Fine, osc4Fine,
                     unisonDetune, unisonWidth, glide, glideCurve, vibrato, drift, jitter, spread,
                     master, sag, warmup,
                     fxFirst,
                     lfoPointFirst = fxFirst + kNumFxSlots * kNumFxParams,
                     count = lfoPointFirst + kNumLfos * kMaxModulatedPoints * 2 };

inline constexpr ModDest kFirstNormalisedDest = ModDest::ampAttack;
inline constexpr ModDest kFirstGlobalDest = ModDest::master;
inline constexpr int kNumGlobalDests = (int) ModDest::lfoPointFirst - (int) kFirstGlobalDest;

// Голос хранит модуляцию всех целей (общие у него просто остаются нулями)
inline constexpr int kNumVoiceDests = (int) ModDest::count;

inline bool isGlobalDest (ModDest dest) noexcept     { return dest >= kFirstGlobalDest && dest < ModDest::lfoPointFirst; }
inline bool isNormalisedDest (ModDest dest) noexcept { return dest >= kFirstNormalisedDest && dest < ModDest::lfoPointFirst; }
inline int globalIndex (ModDest dest) noexcept       { return (int) dest - (int) kFirstGlobalDest; }

// Точка point формы LFO lfo: высота (position = false) или положение по времени (position = true)
inline ModDest lfoPointDest (int lfo, int point, bool position) noexcept
{
    return static_cast<ModDest> ((int) ModDest::lfoPointFirst + (lfo * kMaxModulatedPoints + point) * 2 + (position ? 1 : 0));
}

inline bool lfoPointFromDest (ModDest dest, int& lfo, int& point, bool& position) noexcept
{
    const int index = (int) dest - (int) ModDest::lfoPointFirst;
    if (index < 0 || index >= kNumLfos * kMaxModulatedPoints * 2)
        return false;

    position = (index & 1) != 0;
    point = (index / 2) % kMaxModulatedPoints;
    lfo = index / 2 / kMaxModulatedPoints;
    return true;
}

// Ручка param слота рэка slot
inline ModDest fxDest (int slot, int param) noexcept
{
    return static_cast<ModDest> ((int) ModDest::fxFirst + slot * kNumFxParams + param);
}

// Для ручки рэка: слот и номер ручки; false - это не ручка рэка
inline bool fxSlotAndParam (ModDest dest, int& slot, int& param) noexcept
{
    const int index = (int) dest - (int) ModDest::fxFirst;
    if (index < 0 || index >= kNumFxSlots * kNumFxParams)
        return false;

    slot = index / kNumFxParams;
    param = index % kNumFxParams;
    return true;
}

// Направление модуляции слота матрицы.
// Both: двуполярный источник (LFO) качает ручку в обе стороны, однополярный (огибающая) - вверх.
// Up / Down: только вверх или только вниз от положения ручки.
enum class ModPolarity { both, up, down };

enum class VoiceMode { poly, mono, legato };

// Когда срабатывает глайд: Auto - в режиме Legato только на связных нотах, в остальных всегда;
// Always - на каждой ноте; Legato - только когда новая нота взята, пока держится другая клавиша
enum class GlideMode { automatic, always, legato };

// Цели матрицы для осциллятора с номером osc (они добавлялись в разное время и лежат в enum не подряд)
inline ModDest pitchDestForOsc (int osc) noexcept
{
    static constexpr ModDest dests[kNumOscs] { ModDest::osc1Pitch, ModDest::osc2Pitch, ModDest::osc3Pitch, ModDest::osc4Pitch };
    return dests[osc];
}

inline ModDest wtPosDestForOsc (int osc) noexcept
{
    static constexpr ModDest dests[kNumOscs] { ModDest::osc1WtPos, ModDest::osc2WtPos, ModDest::osc3WtPos, ModDest::osc4WtPos };
    return dests[osc];
}

inline ModDest levelDestForOsc (int osc) noexcept
{
    static constexpr ModDest dests[kNumOscs] { ModDest::osc1Level, ModDest::osc2Level, ModDest::osc3Level, ModDest::osc4Level };
    return dests[osc];
}

inline ModDest fineDestForOsc (int osc) noexcept
{
    return static_cast<ModDest> ((int) ModDest::osc1Fine + osc);
}

// Цели матрицы для фильтра с номером filter
struct FilterDests
{
    ModDest cutoff, resonance, drive, vowel, envAmount, keyTrack, velocity;
};

inline FilterDests destsForFilter (int filter) noexcept
{
    return filter == 0 ? FilterDests { ModDest::cutoff, ModDest::resonance, ModDest::drive, ModDest::vowel,
                                       ModDest::filter1EnvAmount, ModDest::filter1KeyTrack, ModDest::filter1Velocity }
                       : FilterDests { ModDest::filter2Cutoff, ModDest::filter2Resonance, ModDest::filter2Drive, ModDest::filter2Vowel,
                                       ModDest::filter2EnvAmount, ModDest::filter2KeyTrack, ModDest::filter2Velocity };
}

// Режимы фильтра в порядке Choices::filterModes() (новые дописаны в конец, поэтому Vowel посередине)
inline constexpr int kVowelFilterMode = 4;

inline LadderFilter::Mode ladderModeForIndex (int index) noexcept
{
    using M = LadderFilter::Mode;
    static constexpr M modes[] { M::lowpass24, M::lowpass12, M::bandpass12, M::highpass24, M::lowpass24,
                                 M::highpass12, M::bandpass24, M::notch };
    return modes[std::clamp (index, 0, (int) std::size (modes) - 1)];
}

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

// LFO, нота и случайное значение качаются в обе стороны; огибающие, velocity и контроллеры - только вверх
inline bool isBipolarSource (ModSource source) noexcept
{
    return lfoIndexForSource (source) >= 0 || source == ModSource::key || source == ModSource::random;
}

// Значение источника с учётом направления: source * scale + offset
struct PolarityMapping
{
    float scale = 1.0f, offset = 0.0f;
};

inline PolarityMapping polarityMapping (ModSource source, ModPolarity polarity) noexcept
{
    if (isBipolarSource (source))
    {
        // -1..1 превращается в 0..1 (вверх) или 0..-1 (вниз)
        switch (polarity)
        {
            case ModPolarity::up:   return { 0.5f, 0.5f };
            case ModPolarity::down: return { -0.5f, -0.5f };
            case ModPolarity::both: break;
        }

        return {};
    }

    return polarity == ModPolarity::down ? PolarityMapping { -1.0f, 0.0f } : PolarityMapping {};
}

// Диапазон, который слот может добавить к ручке при глубине amount: [low, high]
inline std::pair<float, float> modulationSpan (ModSource source, ModPolarity polarity, float amount) noexcept
{
    const auto mapping = polarityMapping (source, polarity);
    const float lowest = isBipolarSource (source) ? -1.0f : 0.0f;
    const float a = (lowest * mapping.scale + mapping.offset) * amount;
    const float b = (1.0f * mapping.scale + mapping.offset) * amount;
    return { std::min (a, b), std::max (a, b) };
}

struct ModSlot
{
    ModSource source = ModSource::off;
    ModDest dest = ModDest::off;
    float amount = 0.0f;
    ModPolarity polarity = ModPolarity::both;

    // Вклад слота: source * scale + offset (глубина и направление уже учтены)
    float scale = 0.0f, offset = 0.0f;
};

struct LfoParams
{
    LfoShape shape = LfoShape::sine;
    LfoMode mode = LfoMode::free;
    float rateHz = 1.0f;
};

struct OscParams
{
    bool on = false, sync = false;
    PolyBlepOscillator::Shape shape = PolyBlepOscillator::Shape::saw;
    float wtPos = 0.0f, offsetCents = 0.0f, fineCents = 0.0f, pulseWidth = 0.5f, level = 0.5f;
};

// Снимок параметров на один аудиоблок: читается один раз, а не каждым голосом по отдельности
struct SynthParams
{
    std::array<OscParams, kNumOscs> oscs {};
    float subLevel = 0.0f, noiseLevel = 0.0f;
    float noiseColor = 0.0f; // положение между видами шума, см. NoiseGenerator::Type
    float fmAmount = 0.0f, ringLevel = 0.0f, foldAmount = 0.0f;

    struct FilterParams
    {
        bool on = true, vowelMode = false;
        LadderFilter::Mode mode = LadderFilter::Mode::lowpass24;
        float cutoff = 2000.0f, resonance = 0.0f, drive = 0.3f, vowel = 0.0f;
        float envAmount = 0.0f, keyTrack = 0.5f, velocityToCutoff = 0.0f;
    };

    std::array<FilterParams, kNumFilters> filters {};
    bool filtersParallel = false;

    Distortion::Type distType = Distortion::Type::off;
    float distDrive = 0.0f, distMix = 1.0f, distTone = 1.0f;
    bool distBeforeFilter = false;

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
    GlideMode glideMode = GlideMode::automatic;
    float glideCurve = 0.7f; // -1 медленный старт .. 0 равномерно .. +1 быстрый старт

    bool mpe = false;
    float mpeBendRange = 48.0f;

    static SynthParams fromRefs (const ParameterRefs& p, double bpm)
    {
        SynthParams s;
        for (size_t i = 0; i < s.oscs.size(); ++i)
        {
            auto& osc = s.oscs[i];
            osc.on = p.oscOn[i]->load() > 0.5f;
            osc.sync = p.oscSync[i] != nullptr && p.oscSync[i]->load() > 0.5f;
            osc.shape = static_cast<PolyBlepOscillator::Shape> ((int) p.oscShape[i]->load());
            osc.wtPos = p.oscWtPos[i]->load();
            osc.fineCents = p.oscFine[i]->load();
            osc.offsetCents = p.oscSemi[i]->load() * 100.0f + osc.fineCents;
            osc.pulseWidth = p.oscPw[i]->load();
            osc.level = p.oscLevel[i]->load();
        }

        s.subLevel = p.subLevel->load();
        s.noiseLevel = p.noiseLevel->load();
        s.noiseColor = p.noiseColor->load();
        s.fmAmount = p.fmAmount->load();
        s.ringLevel = p.ringLevel->load();
        s.foldAmount = p.foldAmount->load();

        for (size_t f = 0; f < s.filters.size(); ++f)
        {
            const auto& refs = p.filters[f];
            const auto get = [&refs] (FilterParam param) { return refs[(size_t) param]->load(); };
            auto& filter = s.filters[f];

            const int mode = (int) get (FilterParam::mode);
            filter.on = get (FilterParam::on) > 0.5f;
            filter.vowelMode = mode == kVowelFilterMode;
            filter.mode = ladderModeForIndex (mode);
            filter.cutoff = get (FilterParam::cutoff);
            filter.resonance = get (FilterParam::resonance);
            filter.drive = get (FilterParam::drive);
            filter.vowel = get (FilterParam::vowel);
            filter.envAmount = get (FilterParam::envAmount);
            filter.keyTrack = get (FilterParam::keyTrack);
            filter.velocityToCutoff = get (FilterParam::velocity);
        }

        s.filtersParallel = p.filterRouting->load() > 0.5f;

        s.distType = static_cast<Distortion::Type> ((int) p.distType->load());
        s.distDrive = p.distDrive->load();
        s.distMix = p.distMix->load();
        s.distTone = p.distTone->load();
        s.distBeforeFilter = p.distPosition->load() > 0.5f;

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
            auto& slot = s.modSlots[i];
            slot.source = static_cast<ModSource> (juce::jlimit (0, (int) ModSource::count - 1, (int) p.modSource[i]->load()));
            slot.dest = static_cast<ModDest> (juce::jlimit (0, (int) ModDest::count - 1, (int) p.modDest[i]->load()));
            slot.amount = p.modAmount[i]->load();
            slot.polarity = static_cast<ModPolarity> (juce::jlimit (0, 2, (int) p.modPolarity[i]->load()));

            const auto mapping = polarityMapping (slot.source, slot.polarity);
            slot.scale = mapping.scale * slot.amount;
            slot.offset = mapping.offset * slot.amount;
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
        s.glideMode = static_cast<GlideMode> ((int) p.glideMode->load());
        s.glideCurve = p.glideCurve->load();
        s.bendRange = p.bendRange->load();
        s.vibrato = p.vibrato->load();

        s.mpe = p.mpeOn->load() > 0.5f;
        s.mpeBendRange = p.mpeBendRange->load();
        return s;
    }

    bool isActive (const ModSlot& slot) const noexcept
    {
        return slot.source != ModSource::off && slot.dest != ModDest::off && slot.amount != 0.0f;
    }
};

// Шкалы ручек для целей "в долях хода": модуляция добавляется к нормированному положению ручки
using DestRanges = std::array<juce::NormalisableRange<float>, (size_t) ModDest::count>;

// Глобальные модуляции, общие для всех голосов на один блок
struct ModulationBus
{
    // Как часто "ведущий" голос (последний взятый) записывает свои источники для общих целей
    static constexpr int kLeaderInterval = 64;

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

    std::array<const Wavetable*, kNumOscs> wavetables {};
    std::array<const LfoPointSet*, kNumLfos> lfoPoints {}; // точки форм: для модуляции точек
    const DestRanges* ranges = nullptr;

    // Источники ведущего голоса: kLeaderInterval сэмплов на запись, ModSource::count значений в записи
    float* leaderSources = nullptr;
    uint8_t* leaderWritten = nullptr;

    // Для интерфейса: сюда голоса складывают сигнал до фильтра (спектр "что фильтр вырезает")
    float* displayTap = nullptr;
};

// Разброс "железа" между голосовыми платами. Значения в [-1, 1], итог масштабирует ручка Spread.
struct Tolerances
{
    std::array<float, kNumOscs> oscCents {};
    float cutoffOctaves = 0.0f, envelopeTime = 0.0f;
    float pulseWidth = 0.0f, level = 0.0f, pan = 0.0f, vibratoRate = 0.0f;
};

} // namespace sonder
