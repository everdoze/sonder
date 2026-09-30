#include "Voice.h"
#include "ModRouting.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace sonder
{

namespace
{
    constexpr int kControlInterval = 16;

    // Максимумы при ручках на 100 %. Дрейф и джиттер заданы как СКО отклонения.
    constexpr float kMaxDriftCents = 10.0f;
    constexpr float kMaxCutoffDriftOctaves = 0.2f;
    constexpr float kMaxJitterCents = 1.5f;
    constexpr float kMaxSpreadCents = 8.0f;
    constexpr float kMaxSpreadCutoffOctaves = 0.35f;
    constexpr float kMaxSpreadLevelDb = 1.0f;
    constexpr float kMaxUnisonDetuneCents = 25.0f;

    constexpr float kFilterEnvOctaves = 6.0f;
    constexpr float kVelocityCutoffOctaves = 3.0f;
    constexpr float kVibratoSemitones = 0.6f;
    constexpr float kFmDepth = 3.0f;
    constexpr float kNoiseFloor = 1.0e-4f;          // собственный шум схемы, заодно "раскачивает" резонанс
    constexpr float kCentsToRatio = 0.00057762265f; // ln(2) / 1200
    constexpr float kStealFadeSeconds = 0.004f;

    float driveToGain (float drive) noexcept
    {
        // 0.5..2.8: на малом драйве tanh почти линеен, на большом насыщение "съедает" резонанс, как у Moog
        return 0.5f * std::exp2 (2.5f * drive);
    }

    float midiToHz (float pitch) noexcept { return 440.0f * std::exp2 ((pitch - 69.0f) / 12.0f); }

    // Фаза общего (Free) LFO в сэмпле offset текущего блока
    void globalLfoPhaseAt (const ModulationBus& bus, int lfo, int offset, float& phase, uint32_t& cycle) noexcept
    {
        const float position = bus.lfoPhase[(size_t) lfo] + bus.lfoIncrement[(size_t) lfo] * (float) offset;
        const float whole = std::floor (position);
        phase = position - whole;
        cycle = bus.lfoCycle[(size_t) lfo] + (uint32_t) whole;
    }

    // Модуляция "в долях хода ручки": сдвиг нормированного положения
    float modulateNormalised (const ModulationBus& bus, ModDest dest, float base, float modulation) noexcept
    {
        if (modulation == 0.0f)
            return base;

        if (bus.ranges == nullptr)
            return base + modulation;

        const auto& range = (*bus.ranges)[(size_t) dest];
        return range.convertFrom0to1 (std::clamp (range.convertTo0to1 (base) + modulation, 0.0f, 1.0f));
    }
}

void Voice::prepare (double newSampleRate)
{
    sampleRate = (float) newSampleRate;
    piOverSampleRate = (float) (juce::MathConstants<double>::pi / newSampleRate);
    maxCutoff = (float) (0.45 * newSampleRate);
    stealFadeLength = juce::jmax (8, juce::roundToInt (kStealFadeSeconds * sampleRate));

    // Таблицы классических форм создаются здесь, а не при первом звуке в аудиопотоке
    Wavetables::classicSaw();
    Wavetables::classicTriangle();
    Wavetables::classicSine();

    auto& random = juce::Random::getSystemRandom();
    const auto seed = [&random] { return (uint32_t) random.nextInt(); };

    // Дрейф обновляется на control rate, поэтому и генераторы готовим под эту частоту
    const double controlRate = newSampleRate / kControlInterval;

    // У осцилляторов немного разная скорость дрейфа: они медленно "плавают" друг относительно друга
    static constexpr float driftRates[kNumOscs] { 0.25f, 0.33f, 0.29f, 0.21f };

    for (auto& layer : layers)
    {
        for (size_t o = 0; o < (size_t) kNumOscs; ++o)
        {
            layer.osc[o].setSampleRate (newSampleRate);
            layer.osc[o].setPhase (random.nextFloat());
            layer.drift[o].prepare (controlRate, driftRates[o], seed(), 2);
            // Дрожание медленнее, чем раньше (было 800 Гц): даёт живость, но не шумовые боковые полосы у гармоник
            layer.jitter[o].prepare (newSampleRate, 150.0f, seed(), 1);
        }
    }

    cutoffDrift.prepare (controlRate, 0.15f, seed(), 2);
    noiseGenerator.prepare (newSampleRate, seed());
    noise.setSeed (seed());

    for (auto& filter : filters)
        filter.reset();
    distortionLeft.prepare (newSampleRate);
    distortionRight.prepare (newSampleRate);
    ampEnvelope.setSampleRate (newSampleRate);
    filterEnvelope.setSampleRate (newSampleRate);
    kill();

    parameterSmoothingCoef = (float) (1.0 - std::exp (-1.0 / (0.005 * newSampleRate)));
}

void Voice::start (int midiNote, float pitch, float noteVelocity, float glideFromPitch, bool retriggerEnvelopes,
                   const SynthParams& params, const ModulationBus& bus, int sampleOffset)
{
    if (! active)
        snapParameters = true;

    pendingNote = -1;
    stealFadeLeft = 0;

    note = midiNote;
    soundingPitch = pitch;
    velocity = noteVelocity;
    randomValue = noise.nextBipolar();
    targetPitch = pitch;
    currentPitch = glideFromPitch >= 0.0f ? glideFromPitch : targetPitch;
    glideStart = currentPitch;
    glideProgress = glideFromPitch >= 0.0f ? 0.0f : 1.0f;

    if (retriggerEnvelopes || ! active)
    {
        // Огибающие стартуют с текущего уровня: повторная нота на том же голосе не щёлкает
        ampEnvelope.noteOn();
        filterEnvelope.noteOn();

        // Модуляция прошлой ноты новой не достаётся
        displayModulation.fill (0.0f);

        for (int l = 0; l < kNumLfos; ++l)
        {
            auto& state = lfoStates[(size_t) l];
            state.finished = false;

            if (params.lfos[(size_t) l].mode == LfoMode::free)
            {
                // Все голоса видят одну и ту же фазу и одни и те же "случайные" значения
                globalLfoPhaseAt (bus, l, sampleOffset, state.phase, state.cycle);
                state.seed = 0x1f0u + (uint32_t) l;
            }
            else
            {
                state.phase = 0.0f;
                state.cycle = 0;
                state.seed = noise.nextUInt();
            }
        }
    }

    active = true;
    releasing = false;
    controlCounter = 0;
}

void Voice::setExpression (float bendSemitones, float pressure, float slide, bool jump) noexcept
{
    bendTarget = bendSemitones;
    pressureTarget = pressure;
    slideTarget = slide;

    if (jump)
    {
        bendValue = bendTarget;
        pressureValue = pressureTarget;
        slideValue = slideTarget;
    }
}

void Voice::stealTo (int midiNote, float pitch, float noteVelocity, float glideFromPitch) noexcept
{
    pendingNote = midiNote;
    pendingPitch = pitch;
    pendingVelocity = noteVelocity;
    pendingGlideFrom = glideFromPitch;
    pendingRelease = false;

    // Для менеджера голос уже принадлежит новой ноте
    note = midiNote;
    releasing = false;

    if (stealFadeLeft == 0)
        stealFadeLeft = stealFadeLength;
}

void Voice::launchPendingNote (const SynthParams& params, const ModulationBus& bus, int sampleOffset)
{
    const int midiNote = pendingNote;
    const bool releaseAfterStart = pendingRelease;

    // Старая нота затухла: новая стартует с тишины, как на свободном голосе
    ampEnvelope.reset();
    filterEnvelope.reset();
    active = false;

    start (midiNote, pendingPitch, pendingVelocity, pendingGlideFrom, true, params, bus, sampleOffset);

    // Клавишу успели отпустить, пока шёл фейд
    if (releaseAfterStart)
        release();
}

void Voice::glideTo (int midiNote, float pitch, bool glide) noexcept
{
    note = midiNote;

    if (pendingNote >= 0)
    {
        pendingNote = midiNote;
        pendingPitch = pitch;
        return;
    }

    soundingPitch = pitch;
    targetPitch = pitch;

    // Глайд идёт от того места, где высота находится сейчас (в том числе посреди прошлого глайда)
    glideStart = currentPitch;
    glideProgress = glide ? 0.0f : 1.0f;
}

void Voice::release() noexcept
{
    releasing = true;

    if (pendingNote >= 0)
    {
        pendingRelease = true;
        return;
    }

    ampEnvelope.noteOff();
    filterEnvelope.noteOff();
}

void Voice::kill() noexcept
{
    ampEnvelope.reset();
    filterEnvelope.reset();
    active = false;
    releasing = false;
    pendingNote = -1;
    stealFadeLeft = 0;
}

void Voice::updateSlowValues (const SynthParams& p, const ModulationBus& bus, const Tolerances& tol,
                              const float* mod, const std::array<bool, kNumVoiceDests>& used) noexcept
{
    using D = ModDest;

    // Значение ручки с модуляцией (если цель модулируют), иначе как есть
    const auto value = [&] (ModDest dest, float base)
    {
        return mod != nullptr && used[(size_t) dest] ? modulateNormalised (bus, dest, base, mod[(size_t) dest]) : base;
    };

    const float spread = value (D::spread, p.spread);
    const float envelopeTimeScale = 1.0f + 0.25f * spread * tol.envelopeTime;

    ampEnvelope.setParameters (value (D::ampAttack, p.ampAttack) * envelopeTimeScale,
                               value (D::ampDecay, p.ampDecay) * envelopeTimeScale,
                               value (D::ampSustain, p.ampSustain),
                               value (D::ampRelease, p.ampRelease) * envelopeTimeScale);
    filterEnvelope.setParameters (value (D::filterEnvAttack, p.filterAttack) * envelopeTimeScale,
                                  value (D::filterEnvDecay, p.filterDecay) * envelopeTimeScale,
                                  value (D::filterEnvSustain, p.filterSustain),
                                  value (D::filterEnvRelease, p.filterRelease) * envelopeTimeScale);

    // Унисон: слои раскладываются равномерно по расстройке и по стерео
    slow.numLayers = std::clamp (p.unisonVoices, 1, kMaxUnison);
    slow.layerNorm = 1.0f / std::sqrt ((float) slow.numLayers);
    const float detune = value (D::unisonDetune, p.unisonDetune);
    const float width = value (D::unisonWidth, p.unisonWidth);

    for (int l = 0; l < slow.numLayers; ++l)
    {
        const float position = slow.numLayers == 1 ? 0.0f : -1.0f + 2.0f * (float) l / (float) (slow.numLayers - 1);
        const float pan = position * width;
        slow.layerDetune[(size_t) l] = position * detune * kMaxUnisonDetuneCents;
        slow.layerGainLeft[(size_t) l] = std::min (1.0f, 1.0f - pan);
        slow.layerGainRight[(size_t) l] = std::min (1.0f, 1.0f + pan);
    }

    const float drift = value (D::drift, p.drift);
    slow.driftCents = drift * kMaxDriftCents * bus.driftScale;
    slow.cutoffDriftOctaves = drift * kMaxCutoffDriftOctaves * bus.driftScale;
    slow.jitterRatio = value (D::jitter, p.jitter) * kMaxJitterCents * kCentsToRatio;

    for (size_t o = 0; o < (size_t) kNumOscs; ++o)
    {
        const auto& osc = p.oscs[o];
        const float fine = value (fineDestForOsc ((int) o), osc.fineCents);
        slow.staticCents[o] = osc.offsetCents + (fine - osc.fineCents) + spread * kMaxSpreadCents * tol.oscCents[o];
        slow.pulseWidthBase[o] = osc.pulseWidth + 0.04f * spread * tol.pulseWidth;
    }

    for (size_t f = 0; f < filters.size(); ++f)
    {
        const auto& fp = p.filters[f];
        const auto dests = destsForFilter ((int) f);
        auto& stage = filters[f];

        stage.baseCutoff = fp.cutoff * std::exp2 (value (dests.keyTrack, fp.keyTrack) * (soundingPitch - 60.0f) / 12.0f
                                                  + spread * kMaxSpreadCutoffOctaves * tol.cutoffOctaves
                                                  + value (dests.velocity, fp.velocityToCutoff) * (velocity - 1.0f) * kVelocityCutoffOctaves);
        stage.envOctaves = value (dests.envAmount, fp.envAmount) * kFilterEnvOctaves;
    }

    slow.gain = (1.0f - value (D::ampVelocity, p.ampVelocity) * (1.0f - velocity))
              * juce::Decibels::decibelsToGain (spread * kMaxSpreadLevelDb * tol.level)
              * bus.globalGain;
    slow.staticPan = 0.5f * spread * tol.pan;

    // Глайд: за время Glide высота проходит путь от старой ноты к новой по кривой.
    // Кривая > 0 - быстрый старт и плавный подход (как RC-цепь), 0 - равномерно, < 0 - медленный старт.
    const float glide = value (D::glide, p.glide);
    slow.glideIncrement = glide < 0.001f ? 1.0f : 1.0f / (glide * sampleRate);
    slow.glideK = value (D::glideCurve, p.glideCurve) * 5.7f;
    slow.glideLinear = std::abs (slow.glideK) < 0.05f;
    slow.glideNorm = slow.glideLinear ? 1.0f : 1.0f / (1.0f - std::exp (-slow.glideK));

    slow.vibratoIncrement = 5.3f * (1.0f + 0.06f * tol.vibratoRate) / sampleRate;
    slow.vibratoDepth = value (D::vibrato, p.vibrato) * kVibratoSemitones;

    // Дисторшн: тон - однополюсный ФНЧ от 800 Гц до ~19 кГц, на максимуме выключен
    const float tone = value (D::distTone, p.distTone);
    slow.toneCoef = tone >= 0.999f ? 1.0f : 1.0f - std::exp (-2.0f * piOverSampleRate * 800.0f * std::exp2 (tone * 4.6f));
}

void Voice::updateLfoPoints (int lfo, const ModulationBus& bus, const float* mod) noexcept
{
    // Точки формы с модуляцией: высота - в долях всей шкалы (-1..1), положение - в долях цикла.
    // Крайние точки по времени прибиты к краям цикла, остальные не перескакивают через соседей.
    auto& state = lfoStates[(size_t) lfo];
    const auto& base = *bus.lfoPoints[(size_t) lfo];
    state.numPoints = base.count;
    std::copy (base.points.begin(), base.points.begin() + base.count, state.points.begin());

    const int modulated = std::min (base.count, kMaxModulatedPoints);

    for (int i = 0; i < modulated; ++i)
    {
        auto& point = state.points[(size_t) i];
        point.y = std::clamp (point.y + 2.0f * mod[(size_t) lfoPointDest (lfo, i, false)], -1.0f, 1.0f);

        if (i > 0 && i < base.count - 1)
        {
            const float lower = state.points[(size_t) i - 1].x;
            const float upper = base.points[(size_t) i + 1].x;
            point.x = std::clamp (point.x + mod[(size_t) lfoPointDest (lfo, i, true)], lower, upper);
        }
    }
}

void Voice::render (float* left, float* right, int startSample, int numSamples,
                    const SynthParams& p, const ModulationBus& bus, const Tolerances& tol) noexcept
{
    if (! active || numSamples <= 0)
        return;

    constexpr auto numOscs = (size_t) kNumOscs;
    constexpr auto numSources = (size_t) ModSource::count;
    constexpr auto numDests = (size_t) kNumVoiceDests;

    // Активные слоты мод-матрицы, какие цели и LFO реально используются.
    // Общие цели (рэк, Master) голос не считает, но LFO для них нужны ведущему голосу.
    std::array<ModSlot, kNumModSlots> slots {};
    int numSlots = 0;
    std::array<bool, numDests> used {};
    std::array<int, numDests> usedList {};
    int numUsed = 0;
    bool distortionModulated = false, subModulated = false, slowModulated = false, ringModulated = false, noiseModulated = false;
    std::array<bool, numOscs> pitchModulated {};
    std::array<bool, kNumLfos> lfoUsed {}, lfoRateModulated {}, lfoPointsModulated {};
    bool anyPointsModulated = false;

    for (const auto& slot : p.modSlots)
    {
        if (! p.isActive (slot))
            continue;

        if (const int lfo = lfoIndexForSource (slot.source); lfo >= 0)
            lfoUsed[(size_t) lfo] = true;

        if (isGlobalDest (slot.dest))
            continue;

        slots[(size_t) numSlots++] = slot;

        const auto d = (size_t) slot.dest;
        if (! used[d])
        {
            used[d] = true;
            usedList[(size_t) numUsed++] = (int) d;
        }

        distortionModulated |= slot.dest == ModDest::distDrive || slot.dest == ModDest::distMix;
        subModulated |= slot.dest == ModDest::sub;
        noiseModulated |= slot.dest == ModDest::noise;
        ringModulated |= slot.dest == ModDest::ring;
        slowModulated |= isNormalisedDest (slot.dest) && slot.dest != ModDest::ring;

        for (size_t o = 0; o < numOscs; ++o)
            pitchModulated[o] |= slot.dest == pitchDestForOsc ((int) o);

        if (slot.dest >= ModDest::lfo1Rate && slot.dest <= ModDest::lfo8Rate)
            lfoRateModulated[(size_t) ((int) slot.dest - (int) ModDest::lfo1Rate)] = true;

        // Точки формы модулируются только у пользовательской формы
        int pointLfo = 0, point = 0;
        bool position = false;
        if (lfoPointFromDest (slot.dest, pointLfo, point, position)
            && p.lfos[(size_t) pointLfo].shape == LfoShape::custom && bus.lfoPoints[(size_t) pointLfo] != nullptr)
        {
            lfoPointsModulated[(size_t) pointLfo] = true;
            anyPointsModulated = true;
        }
    }

    // Модуляция точек с конца прошлого блока, дальше обновляется на control rate
    if (anyPointsModulated)
        for (int l = 0; l < kNumLfos; ++l)
            if (lfoPointsModulated[(size_t) l])
                updateLfoPoints (l, bus, displayModulation.data());

    // Медленные значения: модуляция берётся с конца прошлого блока, дальше обновляется на control rate
    updateSlowValues (p, bus, tol, slowModulated ? displayModulation.data() : nullptr, used);

    for (size_t f = 0; f < filters.size(); ++f)
    {
        if (snapParameters)
        {
            // Голос был тихим: ручки сразу встают на место, без подъезда от старых значений
            auto& stage = filters[f];
            stage.smoothedCutoff = stage.baseCutoff;
            stage.smoothedResonance = p.filters[f].resonance;
            stage.smoothedDrive = p.filters[f].drive;
        }
    }

    snapParameters = false;

    const float globalPitch = bus.globalPitchCents / 100.0f;
    auto distortionSettings = Distortion::makeSettings (p.distType, p.distDrive, p.distMix, slow.toneCoef);
    const bool distortionOn = p.distType != Distortion::Type::off;

    // Free LFO без модуляции скорости держим в фазе с общим генератором
    for (int l = 0; l < kNumLfos; ++l)
        if (lfoUsed[(size_t) l] && ! lfoRateModulated[(size_t) l] && p.lfos[(size_t) l].mode == LfoMode::free)
            globalLfoPhaseAt (bus, l, startSample, lfoStates[(size_t) l].phase, lfoStates[(size_t) l].cycle);

    // Осцилляторы. Первый считается и выключенным, пока звучит суб или по нему синхронизируются другие:
    // суб привязан к его фазе, синк - к концу его цикла. Wavetable: если таблицы нет, осциллятор звучит пилой.
    std::array<bool, numOscs> oscRuns {}, oscHeard {}, oscSynced {};
    std::array<const Wavetable*, numOscs> tables {};
    std::array<PolyBlepOscillator::Shape, numOscs> shapes {};
    bool anySynced = false;

    for (size_t o = 0; o < numOscs; ++o)
    {
        const auto& osc = p.oscs[o];
        const bool wavetable = osc.shape == PolyBlepOscillator::Shape::wavetable;

        oscHeard[o] = osc.on;
        oscRuns[o] = osc.on;
        oscSynced[o] = o > 0 && osc.on && osc.sync;
        anySynced |= oscSynced[o];
        tables[o] = wavetable ? bus.wavetables[o] : nullptr;
        shapes[o] = wavetable && tables[o] == nullptr ? PolyBlepOscillator::Shape::saw : osc.shape;
    }

    oscRuns[0] = oscRuns[0] || p.subLevel > 0.0f || subModulated || anySynced;
    const bool noiseOn = p.noiseLevel > 0.0f || noiseModulated;

    std::array<float, numSources> source {};
    source[(size_t) ModSource::velocity] = velocity;
    source[(size_t) ModSource::key] = (soundingPitch - 60.0f) / 48.0f;
    source[(size_t) ModSource::random] = randomValue;

    std::array<float, numDests> mod {};
    const int endSample = startSample + numSamples;

    for (int i = startSample; i < endSample; ++i)
    {
        // Фейд при краже голоса
        float stealGain = 1.0f;
        if (stealFadeLeft > 0)
        {
            stealGain = (float) stealFadeLeft / (float) stealFadeLength;
            --stealFadeLeft;
        }

        if (glideProgress < 1.0f)
        {
            glideProgress = std::min (1.0f, glideProgress + slow.glideIncrement);
            const float shaped = slow.glideLinear ? glideProgress
                                                  : (1.0f - std::exp (-slow.glideK * glideProgress)) * slow.glideNorm;
            currentPitch = glideStart + (targetPitch - glideStart) * shaped;
        }
        else
        {
            currentPitch = targetPitch;
        }

        const float filterEnv = filterEnvelope.process();
        const float ampEnv = ampEnvelope.process();

        // Выразительность ноты сглаживается, как колесо модуляции
        bendValue += (bendTarget - bendValue) * parameterSmoothingCoef;
        pressureValue += (pressureTarget - pressureValue) * parameterSmoothingCoef;
        slideValue += (slideTarget - slideValue) * parameterSmoothingCoef;

        // Источники модуляции
        for (int l = 0; l < kNumLfos; ++l)
        {
            if (! lfoUsed[(size_t) l])
                continue;

            const auto& state = lfoStates[(size_t) l];
            source[(size_t) sourceForLfo (l)] = lfoPointsModulated[(size_t) l]
                                                  ? LfoMath::evaluatePoints (state.points.data(), state.numPoints, state.phase)
                                                  : LfoMath::evaluate (p.lfos[(size_t) l].shape, state.phase, state.cycle,
                                                                       state.seed, bus.lfoTables[(size_t) l]);
        }

        source[(size_t) ModSource::filterEnv] = filterEnv;
        source[(size_t) ModSource::ampEnv] = ampEnv;
        source[(size_t) ModSource::modWheel] = bus.modWheel[i];
        source[(size_t) ModSource::aftertouch] = std::max (bus.aftertouch[i], pressureValue);
        source[(size_t) ModSource::slide] = slideValue;

        for (int u = 0; u < numUsed; ++u)
            mod[(size_t) usedList[(size_t) u]] = 0.0f;

        for (int s = 0; s < numSlots; ++s)
        {
            const auto& slot = slots[(size_t) s];
            mod[(size_t) slot.dest] += source[(size_t) slot.source] * slot.scale + slot.offset;
        }

        // Ведущий голос отдаёт свои источники общим целям (рэк, Master)
        if (leader && bus.leaderSources != nullptr && i % ModulationBus::kLeaderInterval == 0)
        {
            const int record = i / ModulationBus::kLeaderInterval;
            std::copy (source.begin(), source.end(), bus.leaderSources + (size_t) record * numSources);
            bus.leaderWritten[record] = 1;
        }

        // Для интерфейса и начала следующего блока запоминаем модуляцию последнего сэмпла
        if (i == endSample - 1)
            displayModulation = mod;

        if (--controlCounter <= 0)
        {
            controlCounter = kControlInterval;

            if (slowModulated)
            {
                updateSlowValues (p, bus, tol, mod.data(), used);
                distortionSettings = Distortion::makeSettings (p.distType, p.distDrive, p.distMix, slow.toneCoef);
            }

            if (anyPointsModulated)
                for (int l = 0; l < kNumLfos; ++l)
                    if (lfoPointsModulated[(size_t) l])
                        updateLfoPoints (l, bus, mod.data());

            for (int l = 0; l < slow.numLayers; ++l)
            {
                auto& layer = layers[(size_t) l];
                const float detune = slow.layerDetune[(size_t) l];

                for (size_t o = 0; o < numOscs; ++o)
                {
                    if (! oscRuns[o])
                        continue;

                    layer.ratio[o] = std::exp2 ((slow.staticCents[o] + detune + layer.drift[o].next() * slow.driftCents) / 1200.0f);

                    // Уровень таблицы нужен и wavetable, и классическим формам
                    layer.mip[o] = Wavetable::selectMip (std::abs (lastFrequency[o]) * layer.ratio[o] / sampleRate);
                }
            }

            cutoffDriftValue = cutoffDrift.next();
        }

        // Продвигаем LFO (скорость может модулироваться матрицей)
        for (int l = 0; l < kNumLfos; ++l)
        {
            auto& state = lfoStates[(size_t) l];
            if (! lfoUsed[(size_t) l] || state.finished)
                continue;

            float increment = bus.lfoIncrement[(size_t) l];
            if (lfoRateModulated[(size_t) l])
                increment *= std::exp2 (mod[(size_t) rateDestForLfo (l)] * kModLfoRateOctaves);

            state.phase += increment;
            if (state.phase >= 1.0f)
            {
                const float whole = std::floor (state.phase);
                state.phase -= whole;
                state.cycle += (uint32_t) whole;

                // Env-режим: один проход и остановка на последнем значении
                if (p.lfos[(size_t) l].mode == LfoMode::envelope)
                {
                    state.phase = 0.9999f;
                    state.finished = true;
                }
            }
        }

        vibratoPhase += slow.vibratoIncrement;
        if (vibratoPhase >= 1.0f)
            vibratoPhase -= 1.0f;

        const float pitch = currentPitch + bendValue + bus.pitchBend[i] + globalPitch
                          + fastSinCycles (vibratoPhase) * slow.vibratoDepth * bus.modWheel[i]
                          + mod[(size_t) ModDest::pitch] * kModPitchSemitones;

        // Частота общая для всех осцилляторов, пока матрица не двигает высоту кого-то отдельно
        const float baseFrequency = midiToHz (pitch);
        const float pulseWidthMod = mod[(size_t) ModDest::pulseWidth] * kModPulseWidth;
        std::array<float, numOscs> frequency {}, level {}, pulseWidth {}, wtPosition {};

        for (size_t o = 0; o < numOscs; ++o)
        {
            if (! oscRuns[o])
                continue;

            const int osc = (int) o;
            frequency[o] = pitchModulated[o] ? midiToHz (pitch + mod[(size_t) pitchDestForOsc (osc)] * kModPitchSemitones)
                                             : baseFrequency;
            lastFrequency[o] = frequency[o];
            level[o] = oscHeard[o] ? std::clamp (p.oscs[o].level + mod[(size_t) levelDestForOsc (osc)], 0.0f, 1.0f) : 0.0f;
            pulseWidth[o] = std::clamp (slow.pulseWidthBase[o] + pulseWidthMod, 0.05f, 0.95f);
            wtPosition[o] = std::clamp (p.oscs[o].wtPos + mod[(size_t) wtPosDestForOsc (osc)], 0.0f, 1.0f);
        }

        const float fm = std::clamp (p.fmAmount + mod[(size_t) ModDest::fm], 0.0f, 1.0f) * kFmDepth;
        const float fold = std::clamp (p.foldAmount + mod[(size_t) ModDest::fold], 0.0f, 1.0f);
        const float sub = std::clamp (p.subLevel + mod[(size_t) ModDest::sub], 0.0f, 1.0f);
        const float noiseLevel = std::clamp (p.noiseLevel + mod[(size_t) ModDest::noise], 0.0f, 1.0f);
        const float ring = ringModulated ? modulateNormalised (bus, ModDest::ring, p.ringLevel, mod[(size_t) ModDest::ring])
                                         : p.ringLevel;

        float sumLeft = 0.0f, sumRight = 0.0f;

        for (int l = 0; l < slow.numLayers; ++l)
        {
            auto& layer = layers[(size_t) l];

            const auto oscFrequency = [&] (size_t o, float frequencyScale) noexcept
            {
                // Джиттер мелкий, поэтому 2^(c/1200) ~ 1 + c * ln2 / 1200
                return frequency[o] * layer.ratio[o] * frequencyScale * (1.0f + layer.jitter[o].next() * slow.jitterRatio);
            };

            const auto run = [&] (size_t o, float frequencyScale) noexcept
            {
                const float f = oscFrequency (o, frequencyScale);
                return tables[o] != nullptr ? layer.osc[o].processWavetable (f, *tables[o], wtPosition[o], layer.mip[o])
                                            : layer.osc[o].processClassic (f, shapes[o], pulseWidth[o], layer.mip[o]);
            };

            const auto runSynced = [&] (size_t o, float wrap) noexcept
            {
                return layer.osc[o].processSynced (oscFrequency (o, 1.0f), shapes[o], pulseWidth[o], wrap,
                                                   tables[o], wtPosition[o], layer.mip[o]);
            };

            // Второй осциллятор обычно считается первым: он модулирует частоту первого.
            // Through-zero FM: при глубокой модуляции частота уходит в минус, и фаза идёт назад.
            // Синхронизированный второй считается после первого, а в FM идёт его значение с прошлого сэмпла.
            float o2 = 0.0f;
            if (oscRuns[1] && ! oscSynced[1])
                o2 = run (1, 1.0f);

            const float fmSource = oscSynced[1] ? layer.lastO2 : o2;
            float o1 = oscRuns[0] ? run (0, 1.0f + fm * fmSource) : 0.0f;
            const float subOut = layer.osc[0].getSub();
            const float wrap = oscRuns[0] ? layer.osc[0].getWrapFraction() : -1.0f;

            if (oscSynced[1])
            {
                o2 = runSynced (1, wrap);
                layer.lastO2 = o2;
            }

            if (! oscHeard[0])
                o1 = 0.0f; // выключен, работает только ради суба и синка

            float signal = o1 * level[0] + o2 * level[1] + ring * o1 * o2 + sub * subOut;

            for (size_t o = 2; o < numOscs; ++o)
                if (oscRuns[o])
                    signal += (oscSynced[o] ? runSynced (o, wrap) : run (o, 1.0f)) * level[o];

            signal = sineFold (signal, fold);

            sumLeft += signal * slow.layerGainLeft[(size_t) l];
            sumRight += signal * slow.layerGainRight[(size_t) l];
        }

        float noiseSample = noise.nextBipolar() * kNoiseFloor;
        if (noiseOn)
            noiseSample += noiseGenerator.processMorph (p.noiseColor + mod[(size_t) ModDest::noiseColor] * kModNoiseColor,
                                                        baseFrequency) * noiseLevel;

        const bool stereo = slow.numLayers > 1;
        sumLeft = sumLeft * slow.layerNorm + noiseSample;
        sumRight = sumRight * slow.layerNorm + noiseSample;

        const auto modulatedDistortion = [&]
        {
            return Distortion::makeSettings (p.distType,
                                             std::clamp (p.distDrive + mod[(size_t) ModDest::distDrive], 0.0f, 1.0f),
                                             std::clamp (p.distMix + mod[(size_t) ModDest::distMix], 0.0f, 1.0f),
                                             slow.toneCoef);
        };

        // Дисторшн до фильтра: искажается сырая смесь осцилляторов, фильтр потом сглаживает результат
        if (distortionOn && p.distBeforeFilter)
        {
            if (distortionModulated)
                distortionSettings = modulatedDistortion();

            sumLeft = distortionLeft.process (sumLeft, distortionSettings);
            sumRight = stereo ? distortionRight.process (sumRight, distortionSettings) : sumLeft;
        }

        // Фильтр: ручки сглаживаются, модуляция добавляется поверх
        const auto runFilter = [&] (size_t f, float inLeft, float inRight, float& outLeft, float& outRight) noexcept
        {
            const auto& fp = p.filters[f];
            auto& stage = filters[f];
            const auto dests = destsForFilter ((int) f);

            stage.smoothedCutoff += (stage.baseCutoff - stage.smoothedCutoff) * parameterSmoothingCoef;
            stage.smoothedResonance += (fp.resonance - stage.smoothedResonance) * parameterSmoothingCoef;

            if (std::abs (fp.drive - stage.smoothedDrive) < 1.0e-5f)
                stage.smoothedDrive = fp.drive;
            else
                stage.smoothedDrive += (fp.drive - stage.smoothedDrive) * parameterSmoothingCoef;

            const float cutoffOctaves = filterEnv * stage.envOctaves
                                      + mod[(size_t) dests.cutoff] * kModCutoffOctaves
                                      + cutoffDriftValue * slow.cutoffDriftOctaves;
            const float cutoffHz = stage.smoothedCutoff * std::exp2 (cutoffOctaves);
            const float resonance = std::clamp (stage.smoothedResonance + mod[(size_t) dests.resonance], 0.0f, 1.0f);

            // Усиление драйва пересчитываем, только когда значение реально меняется
            const float drive = std::clamp (stage.smoothedDrive + mod[(size_t) dests.drive], 0.0f, 1.0f);
            if (drive != stage.cachedDrive)
            {
                stage.cachedDrive = drive;
                stage.driveGain = driveToGain (drive);
                stage.makeup = 1.0f / std::sqrt (stage.driveGain);
            }

            stage.displayCutoff = cutoffHz;

            if (fp.vowelMode)
            {
                // Срез сдвигает форманты: выше - "меньше голова", ниже - "больше"
                const float vowel = std::clamp (fp.vowel + mod[(size_t) dests.vowel], 0.0f, 1.0f);
                const float shiftOctaves = std::clamp (0.4f * std::log2 (cutoffHz / 1000.0f), -1.0f, 1.0f);
                const auto coefficients = FormantFilter::makeCoefficients (vowel, std::exp2 (shiftOctaves), resonance,
                                                                           piOverSampleRate, maxCutoff);
                stage.displayVowel = vowel;

                outLeft = stage.formantLeft.process (inLeft, coefficients, stage.driveGain);
                outRight = stereo ? stage.formantRight.process (inRight, coefficients, stage.driveGain) : outLeft;
            }
            else
            {
                const auto coefficients = LadderFilter::makeCoefficients (cutoffHz, resonance, piOverSampleRate, maxCutoff);
                outLeft = stage.left.process (inLeft, coefficients, stage.driveGain, fp.mode);
                outRight = stereo ? stage.right.process (inRight, coefficients, stage.driveGain, fp.mode) : outLeft;
            }

            outLeft *= stage.makeup;
            outRight *= stage.makeup;
        };

        // Два фильтра: друг за другом или рядом (тогда выходы усредняются). Выключенный фильтр пропускает звук.
        float filteredLeft = sumLeft, filteredRight = sumRight;
        const bool firstOn = p.filters[0].on, secondOn = p.filters[1].on;

        if (p.filtersParallel && firstOn && secondOn)
        {
            float aLeft = 0.0f, aRight = 0.0f, bLeft = 0.0f, bRight = 0.0f;
            runFilter (0, sumLeft, sumRight, aLeft, aRight);
            runFilter (1, sumLeft, sumRight, bLeft, bRight);
            filteredLeft = 0.5f * (aLeft + bLeft);
            filteredRight = 0.5f * (aRight + bRight);
        }
        else
        {
            if (firstOn)
                runFilter (0, filteredLeft, filteredRight, filteredLeft, filteredRight);

            if (secondOn)
                runFilter (1, filteredLeft, filteredRight, filteredLeft, filteredRight);
        }

        // Дисторшн после фильтра
        if (distortionOn && ! p.distBeforeFilter)
        {
            if (distortionModulated)
                distortionSettings = modulatedDistortion();

            filteredLeft = distortionLeft.process (filteredLeft, distortionSettings);
            filteredRight = stereo ? distortionRight.process (filteredRight, distortionSettings) : filteredLeft;
        }

        const float amp = ampEnv * slow.gain * stealGain * std::max (0.0f, 1.0f + mod[(size_t) ModDest::amp]);
        const float pan = std::clamp (slow.staticPan + mod[(size_t) ModDest::pan], -1.0f, 1.0f);
        displayLevel = amp;

        if (bus.displayTap != nullptr)
            bus.displayTap[i] += 0.5f * (sumLeft + sumRight) * amp;

        left[i] += filteredLeft * amp * std::min (1.0f, 1.0f - pan);
        right[i] += filteredRight * amp * std::min (1.0f, 1.0f + pan);

        // Фейд кражи закончился (или старая нота сама затихла): запускаем отложенную ноту.
        // Остаток блока рендерим заново, потому что поблочные значения зависят от ноты и velocity.
        if (pendingNote >= 0 && (stealFadeLeft == 0 || ! ampEnvelope.isActive()))
        {
            launchPendingNote (p, bus, i + 1);
            render (left, right, i + 1, endSample - (i + 1), p, bus, tol);
            return;
        }

        if (! ampEnvelope.isActive())
        {
            filterEnvelope.reset();
            active = false;
            releasing = false;
            break;
        }
    }
}

} // namespace sonder
