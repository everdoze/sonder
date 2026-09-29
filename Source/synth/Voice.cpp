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
    constexpr float kMaxJitterCents = 4.0f;
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
}

void Voice::prepare (double newSampleRate)
{
    sampleRate = (float) newSampleRate;
    piOverSampleRate = (float) (juce::MathConstants<double>::pi / newSampleRate);
    maxCutoff = (float) (0.45 * newSampleRate);

    auto& random = juce::Random::getSystemRandom();
    const auto seed = [&random] { return (uint32_t) random.nextInt(); };

    // Дрейф обновляется на control rate, поэтому и генераторы готовим под эту частоту
    const double controlRate = newSampleRate / kControlInterval;

    for (auto& layer : layers)
    {
        layer.osc1.setSampleRate (newSampleRate);
        layer.osc2.setSampleRate (newSampleRate);
        layer.osc1.setPhase (random.nextFloat());
        layer.osc2.setPhase (random.nextFloat());

        // У двух осцилляторов немного разная скорость дрейфа: они медленно "плавают" друг относительно друга
        layer.drift1.prepare (controlRate, 0.25f, seed(), 2);
        layer.drift2.prepare (controlRate, 0.33f, seed(), 2);
        layer.jitter1.prepare (newSampleRate, 800.0f, seed(), 1);
        layer.jitter2.prepare (newSampleRate, 800.0f, seed(), 1);
    }

    cutoffDrift.prepare (controlRate, 0.15f, seed(), 2);
    noise.setSeed (seed());

    filterLeft.reset();
    filterRight.reset();
    formantLeft.reset();
    formantRight.reset();
    distortionLeft.prepare (newSampleRate);
    distortionRight.prepare (newSampleRate);
    ampEnvelope.setSampleRate (newSampleRate);
    filterEnvelope.setSampleRate (newSampleRate);
    kill();

    cutoffSmoothingCoef = (float) (1.0 - std::exp (-1.0 / (0.005 * newSampleRate)));
}

void Voice::start (int midiNote, float noteVelocity, float glideFromPitch, bool retriggerEnvelopes,
                   const SynthParams& params, const ModulationBus& bus, int sampleOffset)
{
    if (! active)
        snapCutoff = true;

    note = midiNote;
    velocity = noteVelocity;
    randomValue = noise.nextBipolar();
    targetPitch = (float) midiNote;
    currentPitch = glideFromPitch >= 0.0f ? glideFromPitch : targetPitch;

    if (retriggerEnvelopes || ! active)
    {
        // Огибающие стартуют с текущего уровня: при переназначении голоса нет щелчка
        ampEnvelope.noteOn();
        filterEnvelope.noteOn();

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

void Voice::glideTo (int midiNote) noexcept
{
    note = midiNote;
    targetPitch = (float) midiNote;
}

void Voice::release() noexcept
{
    ampEnvelope.noteOff();
    filterEnvelope.noteOff();
    releasing = true;
}

void Voice::kill() noexcept
{
    ampEnvelope.reset();
    filterEnvelope.reset();
    active = false;
    releasing = false;
}

void Voice::render (float* left, float* right, int startSample, int numSamples,
                    const SynthParams& p, const ModulationBus& bus, const Tolerances& tol) noexcept
{
    if (! active)
        return;

    const float spread = p.spread;
    const float envelopeTimeScale = 1.0f + 0.25f * spread * tol.envelopeTime;

    ampEnvelope.setParameters (p.ampAttack * envelopeTimeScale, p.ampDecay * envelopeTimeScale,
                               p.ampSustain, p.ampRelease * envelopeTimeScale);
    filterEnvelope.setParameters (p.filterAttack * envelopeTimeScale, p.filterDecay * envelopeTimeScale,
                                  p.filterSustain, p.filterRelease * envelopeTimeScale);

    // Унисон: слои раскладываются равномерно по расстройке и по стерео
    const int numLayers = std::clamp (p.unisonVoices, 1, kMaxUnison);
    const bool stereo = numLayers > 1;
    const float layerNorm = 1.0f / std::sqrt ((float) numLayers);

    std::array<float, kMaxUnison> layerDetune {}, layerGainLeft {}, layerGainRight {};
    for (int l = 0; l < numLayers; ++l)
    {
        const float position = numLayers == 1 ? 0.0f : -1.0f + 2.0f * (float) l / (float) (numLayers - 1);
        const float pan = position * p.unisonWidth;
        layerDetune[(size_t) l] = position * p.unisonDetune * kMaxUnisonDetuneCents;
        layerGainLeft[(size_t) l] = std::min (1.0f, 1.0f - pan);
        layerGainRight[(size_t) l] = std::min (1.0f, 1.0f + pan);
    }

    const float driftCents = p.drift * kMaxDriftCents * bus.driftScale;
    const float cutoffDriftOctaves = p.drift * kMaxCutoffDriftOctaves * bus.driftScale;
    const float jitterRatio = p.jitter * kMaxJitterCents * kCentsToRatio;
    const float osc1StaticCents = spread * kMaxSpreadCents * tol.osc1Cents;
    const float osc2StaticCents = p.osc2OffsetCents + spread * kMaxSpreadCents * tol.osc2Cents;
    const float pulseWidthBase = p.pulseWidth + 0.04f * spread * tol.pulseWidth;

    // Wavetable: если таблицы нет, осциллятор звучит пилой
    const Wavetable* table1 = p.osc1Shape == PolyBlepOscillator::Shape::wavetable ? bus.wavetables[0] : nullptr;
    const Wavetable* table2 = p.osc2Shape == PolyBlepOscillator::Shape::wavetable ? bus.wavetables[1] : nullptr;
    const auto shape1 = table1 == nullptr && p.osc1Shape == PolyBlepOscillator::Shape::wavetable ? PolyBlepOscillator::Shape::saw : p.osc1Shape;
    const auto shape2 = table2 == nullptr && p.osc2Shape == PolyBlepOscillator::Shape::wavetable ? PolyBlepOscillator::Shape::saw : p.osc2Shape;

    const float baseCutoff = p.cutoff * std::exp2 (p.keyTrack * (float) (note - 60) / 12.0f
                                                   + spread * kMaxSpreadCutoffOctaves * tol.cutoffOctaves
                                                   + p.velocityToCutoff * (velocity - 1.0f) * kVelocityCutoffOctaves);
    if (snapCutoff)
    {
        smoothedCutoff = baseCutoff;
        snapCutoff = false;
    }

    const float filterEnvOctaves = p.filterEnvAmount * kFilterEnvOctaves;
    const float gain = (1.0f - p.ampVelocity * (1.0f - velocity))
                     * juce::Decibels::decibelsToGain (spread * kMaxSpreadLevelDb * tol.level)
                     * bus.globalGain;
    const float staticPan = 0.5f * spread * tol.pan;

    const float glideCoef = p.glide < 0.001f ? 1.0f : 1.0f - std::exp (-1.0f / (0.25f * p.glide * sampleRate));
    const float vibratoIncrement = 5.3f * (1.0f + 0.06f * tol.vibratoRate) / sampleRate;
    const float vibratoDepth = p.vibrato * kVibratoSemitones;
    const float globalPitch = bus.globalPitchCents / 100.0f;

    // Дисторшн: тон - однополюсный ФНЧ от 800 Гц до ~19 кГц, на максимуме выключен
    const float toneCoef = p.distTone >= 0.999f ? 1.0f
                         : 1.0f - std::exp (-2.0f * piOverSampleRate * 800.0f * std::exp2 (p.distTone * 4.6f));
    auto distortionSettings = Distortion::makeSettings (p.distType, p.distDrive, p.distMix, toneCoef);
    const bool distortionOn = p.distType != Distortion::Type::off;

    // Активные слоты мод-матрицы и какие LFO реально используются
    std::array<ModSlot, kNumModSlots> slots {};
    int numSlots = 0;
    bool driveModulated = false, distortionModulated = false;
    std::array<bool, kNumLfos> lfoUsed {}, lfoRateModulated {};

    for (const auto& slot : p.modSlots)
    {
        if (slot.source == ModSource::off || slot.dest == ModDest::off || slot.amount == 0.0f)
            continue;

        slots[(size_t) numSlots++] = slot;
        driveModulated |= slot.dest == ModDest::drive;
        distortionModulated |= slot.dest == ModDest::distDrive || slot.dest == ModDest::distMix;

        if (const int lfo = lfoIndexForSource (slot.source); lfo >= 0)
            lfoUsed[(size_t) lfo] = true;

        if (slot.dest >= ModDest::lfo1Rate && slot.dest <= ModDest::lfo8Rate)
            lfoRateModulated[(size_t) ((int) slot.dest - (int) ModDest::lfo1Rate)] = true;
    }

    // Free LFO без модуляции скорости держим в фазе с общим генератором
    for (int l = 0; l < kNumLfos; ++l)
        if (lfoUsed[(size_t) l] && ! lfoRateModulated[(size_t) l] && p.lfos[(size_t) l].mode == LfoMode::free)
            globalLfoPhaseAt (bus, l, startSample, lfoStates[(size_t) l].phase, lfoStates[(size_t) l].cycle);

    float driveGain = driveToGain (p.drive);
    float makeup = 1.0f / std::sqrt (driveGain);

    constexpr auto numSources = (size_t) ModSource::count;
    constexpr auto numDests = (size_t) ModDest::count;
    std::array<float, numSources> source {};
    source[(size_t) ModSource::velocity] = velocity;
    source[(size_t) ModSource::key] = (float) (note - 60) / 48.0f;
    source[(size_t) ModSource::random] = randomValue;

    for (int i = startSample; i < startSample + numSamples; ++i)
    {
        if (--controlCounter <= 0)
        {
            controlCounter = kControlInterval;

            for (int l = 0; l < numLayers; ++l)
            {
                auto& layer = layers[(size_t) l];
                const float detune = layerDetune[(size_t) l];
                layer.ratio1 = std::exp2 ((osc1StaticCents + detune + layer.drift1.next() * driftCents) / 1200.0f);
                layer.ratio2 = std::exp2 ((osc2StaticCents + detune + layer.drift2.next() * driftCents) / 1200.0f);
                layer.level1 = Wavetable::levelForFrequency (lastFrequency1 * layer.ratio1 / sampleRate);
                layer.level2 = Wavetable::levelForFrequency (lastFrequency2 * layer.ratio2 / sampleRate);
            }

            cutoffDriftValue = cutoffDrift.next();
        }

        currentPitch += (targetPitch - currentPitch) * glideCoef;

        const float filterEnv = filterEnvelope.process();
        const float ampEnv = ampEnvelope.process();

        // Источники модуляции
        for (int l = 0; l < kNumLfos; ++l)
        {
            if (! lfoUsed[(size_t) l])
                continue;

            const auto& state = lfoStates[(size_t) l];
            source[(size_t) sourceForLfo (l)] = LfoMath::evaluate (p.lfos[(size_t) l].shape, state.phase, state.cycle,
                                                                   state.seed, bus.lfoTables[(size_t) l]);
        }

        source[(size_t) ModSource::filterEnv] = filterEnv;
        source[(size_t) ModSource::ampEnv] = ampEnv;
        source[(size_t) ModSource::modWheel] = bus.modWheel[i];
        source[(size_t) ModSource::aftertouch] = bus.aftertouch[i];

        std::array<float, numDests> mod {};
        for (int s = 0; s < numSlots; ++s)
            mod[(size_t) slots[(size_t) s].dest] += source[(size_t) slots[(size_t) s].source] * slots[(size_t) s].amount;

        // Для интерфейса запоминаем модуляцию последнего сэмпла блока
        if (i == startSample + numSamples - 1)
            displayModulation = mod;

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

        vibratoPhase += vibratoIncrement;
        if (vibratoPhase >= 1.0f)
            vibratoPhase -= 1.0f;

        const float pitch = currentPitch + bus.pitchBend[i] + globalPitch
                          + fastSinCycles (vibratoPhase) * vibratoDepth * bus.modWheel[i]
                          + mod[(size_t) ModDest::pitch] * kModPitchSemitones;

        const float frequency1 = midiToHz (pitch + mod[(size_t) ModDest::osc1Pitch] * kModPitchSemitones);
        const float frequency2 = midiToHz (pitch + mod[(size_t) ModDest::osc2Pitch] * kModPitchSemitones);
        lastFrequency1 = frequency1;
        lastFrequency2 = frequency2;

        const float pulseWidth = std::clamp (pulseWidthBase + mod[(size_t) ModDest::pulseWidth] * kModPulseWidth, 0.05f, 0.95f);
        const float mix = std::clamp (p.oscMix + mod[(size_t) ModDest::oscMix], 0.0f, 1.0f);
        const float fm = std::clamp (p.fmAmount + mod[(size_t) ModDest::fm], 0.0f, 1.0f) * kFmDepth;
        const float fold = std::clamp (p.foldAmount + mod[(size_t) ModDest::fold], 0.0f, 1.0f);
        const float sub = std::clamp (p.subLevel + mod[(size_t) ModDest::sub], 0.0f, 1.0f);
        const float noiseLevel = std::clamp (p.noiseLevel + mod[(size_t) ModDest::noise], 0.0f, 1.0f) + kNoiseFloor;
        const float wtPosition1 = std::clamp (p.osc1WtPos + mod[(size_t) ModDest::osc1WtPos], 0.0f, 1.0f);
        const float wtPosition2 = std::clamp (p.osc2WtPos + mod[(size_t) ModDest::osc2WtPos], 0.0f, 1.0f);

        float sumLeft = 0.0f, sumRight = 0.0f;

        for (int l = 0; l < numLayers; ++l)
        {
            auto& layer = layers[(size_t) l];

            // Джиттер мелкий, поэтому 2^(c/1200) ~ 1 + c * ln2 / 1200
            const float f2 = frequency2 * layer.ratio2 * (1.0f + layer.jitter2.next() * jitterRatio);
            const float o2 = table2 != nullptr ? layer.osc2.processWavetable (f2, *table2, wtPosition2, layer.level2)
                                               : layer.osc2.process (f2, shape2, pulseWidth);

            const float fmFactor = std::max (0.0f, 1.0f + fm * o2);
            const float f1 = frequency1 * layer.ratio1 * fmFactor * (1.0f + layer.jitter1.next() * jitterRatio);
            const float o1 = table1 != nullptr ? layer.osc1.processWavetable (f1, *table1, wtPosition1, layer.level1)
                                               : layer.osc1.process (f1, shape1, pulseWidth);

            float signal = o1 + (o2 - o1) * mix + p.ringLevel * o1 * o2 + sub * layer.osc1.getSub();
            signal = sineFold (signal, fold);

            sumLeft += signal * layerGainLeft[(size_t) l];
            sumRight += signal * layerGainRight[(size_t) l];
        }

        const float noiseSample = noise.nextBipolar() * noiseLevel;
        sumLeft = sumLeft * layerNorm + noiseSample;
        sumRight = sumRight * layerNorm + noiseSample;

        // Фильтр
        smoothedCutoff += (baseCutoff - smoothedCutoff) * cutoffSmoothingCoef;

        const float cutoffOctaves = filterEnv * filterEnvOctaves
                                  + mod[(size_t) ModDest::cutoff] * kModCutoffOctaves
                                  + cutoffDriftValue * cutoffDriftOctaves;
        const float cutoffHz = smoothedCutoff * std::exp2 (cutoffOctaves);
        const float resonance = std::clamp (p.resonance + mod[(size_t) ModDest::resonance], 0.0f, 1.0f);

        if (driveModulated)
        {
            driveGain = driveToGain (std::clamp (p.drive + mod[(size_t) ModDest::drive], 0.0f, 1.0f));
            makeup = 1.0f / std::sqrt (driveGain);
        }

        float filteredLeft = 0.0f, filteredRight = 0.0f;
        displayCutoff = cutoffHz;

        if (p.vowelFilter)
        {
            // Срез сдвигает форманты: выше - "меньше голова", ниже - "больше"
            const float vowel = std::clamp (p.vowel + mod[(size_t) ModDest::vowel], 0.0f, 1.0f);
            const float shiftOctaves = std::clamp (0.4f * std::log2 (cutoffHz / 1000.0f), -1.0f, 1.0f);
            const auto coefficients = FormantFilter::makeCoefficients (vowel, std::exp2 (shiftOctaves), resonance,
                                                                       piOverSampleRate, maxCutoff);
            displayVowel = vowel;

            filteredLeft = formantLeft.process (sumLeft, coefficients, driveGain);
            filteredRight = stereo ? formantRight.process (sumRight, coefficients, driveGain) : filteredLeft;
        }
        else
        {
            const auto coefficients = LadderFilter::makeCoefficients (cutoffHz, resonance, piOverSampleRate, maxCutoff);
            filteredLeft = filterLeft.process (sumLeft, coefficients, driveGain, p.ladderMode);
            filteredRight = stereo ? filterRight.process (sumRight, coefficients, driveGain, p.ladderMode) : filteredLeft;
        }

        filteredLeft *= makeup;
        filteredRight *= makeup;

        // Дисторшн после фильтра
        if (distortionOn)
        {
            if (distortionModulated)
                distortionSettings = Distortion::makeSettings (p.distType,
                                                               std::clamp (p.distDrive + mod[(size_t) ModDest::distDrive], 0.0f, 1.0f),
                                                               std::clamp (p.distMix + mod[(size_t) ModDest::distMix], 0.0f, 1.0f),
                                                               toneCoef);

            filteredLeft = distortionLeft.process (filteredLeft, distortionSettings);
            filteredRight = stereo ? distortionRight.process (filteredRight, distortionSettings) : filteredLeft;
        }

        const float amp = ampEnv * gain * std::max (0.0f, 1.0f + mod[(size_t) ModDest::amp]);
        const float pan = std::clamp (staticPan + mod[(size_t) ModDest::pan], -1.0f, 1.0f);

        left[i] += filteredLeft * amp * std::min (1.0f, 1.0f - pan);
        right[i] += filteredRight * amp * std::min (1.0f, 1.0f + pan);

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
