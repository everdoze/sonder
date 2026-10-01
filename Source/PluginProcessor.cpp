#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Saturation.h"
#include "presets/FactoryPresets.h"
#include "synth/ModRouting.h"

namespace
{
    constexpr double kWarmupTimeConstantSeconds = 20.0;
    constexpr float kMaxWarmupDetuneCents = 40.0f;
    constexpr float kMaxSagCents = 25.0f;
    constexpr float kMaxSagGainDrop = 0.3f;

    // Уровень, с которым сумма голосов приходит на аналоговый выходной каскад (tanh).
    // Ручка Master стоит в самом конце, после эффектов, и отсчитывается от этого же уровня:
    // при -9 дБ (значение по умолчанию) она ничего не меняет.
    constexpr float kOutputStageDb = -9.0f;
}

SonderAudioProcessor::SonderAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "Parameters", sonder::createParameterLayout (fxRack)),
      fxController (fxRack, parameters, *this),
      presetManager (parameters, lfoShapes, wavetables, fxController),
      params (parameters)
{
    // Шкала каждой цели - шкала её ручки; у ручек рэка значения и так нормированы
    for (int d = 1; d < (int) sonder::ModDest::count; ++d)
        if (auto* parameter = parameters.getParameter (sonder::parameterForDestination (static_cast<sonder::ModDest> (d))))
            destRanges[(size_t) d] = parameter->getNormalisableRange();

    voiceManager.setTuning (&tuning);
}

bool SonderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SonderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = juce::jmax (1, samplesPerBlock);

    // Порядок передискретизации: 0 - без неё, 1 - ×2, 2 - ×4
    const auto makeOversampling = [this] (Quality q)
    {
        auto result = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) q,
                                                                        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                        true, false);
        result->initProcessing ((size_t) maxBlockSize);
        return result;
    };

    const auto rendering = getRenderingQuality();
    preparedQuality = (int) rendering;
    oversampling = makeOversampling (rendering);

    // Хосту всегда сообщается задержка High, остальное добирается буфером на выходе
    const int maxLatency = juce::roundToInt (makeOversampling (Quality::high)->getLatencyInSamples());
    latencyPad = juce::jlimit (0, kMaxLatencyPad - 1, maxLatency - juce::roundToInt (oversampling->getLatencyInSamples()));
    padPosition = 0;
    for (auto& channel : padBuffer)
        channel.fill (0.0f);

    // Синтез идёт на повышенной частоте: меньше алиасинга от нелинейностей, эффекты на обычной
    const auto factor = (int) oversampling->getOversamplingFactor();
    oversampledRate = sampleRate * factor;
    const int maxOversampledBlock = maxBlockSize * factor;

    voiceManager.prepare (oversampledRate, maxOversampledBlock);
    arpeggiator.prepare (sampleRate);

    const int records = maxOversampledBlock / sonder::ModulationBus::kLeaderInterval + 2;
    leaderSources.assign ((size_t) records * (size_t) sonder::ModSource::count, 0.0f);
    leaderWritten.assign ((size_t) records, 0);
    tapBuffer.assign ((size_t) maxOversampledBlock, 0.0f);
    oversampledMidi.ensureSize (4096);

    fxChain.prepare (sampleRate, maxBlockSize);

    masterGain.reset (sampleRate, 0.05);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.masterGain->load() - kOutputStageDb));

    scope.sampleRate.store ((float) sampleRate);
    setLatencySamples (maxLatency);
}

void SonderAudioProcessor::setQuality (Quality newQuality)
{
    if ((int) newQuality == quality.load())
        return;

    quality.store ((int) newQuality);
    applyQuality();
}

void SonderAudioProcessor::setNonRealtime (bool offline) noexcept
{
    AudioProcessor::setNonRealtime (offline);
    applyQuality();
}

void SonderAudioProcessor::applyQuality()
{
    // Ещё не подготовлен или качество не поменялось - prepareToPlay всё сделает сам
    if (maxBlockSize == 0 || (int) getRenderingQuality() == preparedQuality)
        return;

    // Частота голосов меняется целиком: останавливаем обработку и готовим всё заново (звучащие ноты обрываются)
    suspendProcessing (true);
    prepareToPlay (currentSampleRate, maxBlockSize);
    suspendProcessing (false);
}

void SonderAudioProcessor::applyLatencyPad (juce::AudioBuffer<float>& buffer) noexcept
{
    if (latencyPad == 0)
        return;

    // Кольцевой буфер на kMaxLatencyPad отсчётов: читаем то, что записали latencyPad сэмплов назад
    const int numSamples = buffer.getNumSamples();
    float* channels[] { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    int position = padPosition;

    for (int i = 0; i < numSamples; ++i)
    {
        const int read = (position - latencyPad + kMaxLatencyPad) % kMaxLatencyPad;
        for (size_t c = 0; c < 2; ++c)
        {
            const float input = channels[c][i];
            channels[c][i] = padBuffer[c][(size_t) read];
            padBuffer[c][(size_t) position] = input;
        }
        position = (position + 1) % kMaxLatencyPad;
    }

    padPosition = position;
}

SonderAudioProcessor::Transport SonderAudioProcessor::readTransport() const
{
    Transport transport;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                transport.bpm = juce::jlimit (20.0, 999.0, *bpm);

            if (const auto ppq = position->getPpqPosition())
                transport.ppq = *ppq;

            transport.isPlaying = position->getIsPlaying();
        }
    }

    return transport;
}

float SonderAudioProcessor::globalModulated (sonder::ModDest dest, float base) const noexcept
{
    // Модуляция последней записи прошлого блока: для величин, которые считаются раз в блок
    const float modulation = globalModulation[(size_t) sonder::globalIndex (dest)];
    return modulation != 0.0f ? sonder::applyModulation (dest, base, modulation, destRanges[(size_t) dest]) : base;
}

void SonderAudioProcessor::updateAnalogState (int numSamples)
{
    // Прогрев: после "включения" (загрузки или поворота ручки) генераторы дрейфуют сильнее
    // и строй занижен, за ~минуту всё успокаивается. Модуляция ручки прогрев не перезапускает.
    const float warmupKnob = params.warmup->load();
    if (std::abs (warmupKnob - lastWarmupAmount) > 0.01f)
    {
        warmupSeconds = 0.0;
        lastWarmupAmount = warmupKnob;
    }

    const float warmupAmount = globalModulated (sonder::ModDest::warmup, warmupKnob);

    const auto cold = (float) std::exp (-warmupSeconds / kWarmupTimeConstantSeconds);
    warmupSeconds += numSamples / currentSampleRate;

    // Просадка питания: чем громче и плотнее играем, тем ниже строй и тише голоса
    const float sagDepth = globalModulated (sonder::ModDest::sag, params.sag->load()) * juce::jmin (1.0f, sagEnvelope * 2.0f);

    coldAmount = warmupAmount * cold;
    sagAmount = sagDepth;

    analogBus.driftScale = 1.0f + 3.0f * warmupAmount * cold;
    analogBus.globalPitchCents = -kMaxWarmupDetuneCents * warmupAmount * cold - kMaxSagCents * sagDepth;
    analogBus.globalGain = 1.0f - kMaxSagGainDrop * sagDepth;
}

void SonderAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    const auto transport = readTransport();
    runArpeggiator (midi, numSamples, transport);
    const auto synthParams = sonder::SynthParams::fromRefs (params, transport.bpm);

    // Синхронизированные LFO привязываем к позиции транспорта, чтобы фаза совпадала с тактом
    if (transport.isPlaying && transport.ppq.has_value())
    {
        for (int l = 0; l < sonder::kNumLfos; ++l)
        {
            const double beats = sonder::syncDivisionInBeats ((int) params.lfoSync[(size_t) l]->load());
            if (beats > 0.0)
            {
                const double position = *transport.ppq / beats;
                globalLfos[(size_t) l].phase = position - std::floor (position);
                globalLfos[(size_t) l].cycle = (uint32_t) (int64_t) std::floor (position);
            }
        }
    }

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (params.masterGain->load() - kOutputStageDb));

    // Хост может прислать блок больше заявленного в prepareToPlay, режем на куски
    for (int start = 0; start < numSamples; start += maxBlockSize)
    {
        const int length = juce::jmin (maxBlockSize, numSamples - start);
        updateAnalogState (length);
        renderChunk (buffer, midi, start, length, synthParams, transport);
    }

    applyLatencyPad (buffer);
    updateDisplayState (synthParams);
}

void SonderAudioProcessor::runArpeggiator (juce::MidiBuffer& midi, int numSamples, const Transport& transport)
{
    sonder::Arpeggiator::Settings settings;
    settings.on = params.arpOn->load() > 0.5f;
    settings.hold = params.arpHold->load() > 0.5f;
    settings.mode = static_cast<sonder::Arpeggiator::Mode> ((int) params.arpMode->load());
    settings.octaves = (int) params.arpOctaves->load();
    settings.stepBeats = sonder::arpRateInBeats ((int) params.arpRate->load());
    settings.gate = params.arpGate->load();
    settings.swing = params.arpSwing->load();

    // Пока транспорт стоит, у арпеджиатора свой такт от первой ноты
    const auto ppq = transport.isPlaying ? transport.ppq : std::nullopt;
    arpeggiator.process (midi, numSamples, settings, transport.bpm, ppq);
}

void SonderAudioProcessor::updateDisplayState (const sonder::SynthParams& synthParams)
{
    activeVoiceMask.store (voiceManager.getActiveVoiceMask());
    scope.noteFrequency.store (voiceManager.getLastNoteFrequency());

    const auto* voice = voiceManager.getDisplayVoice();

    for (int l = 0; l < sonder::kNumLfos; ++l)
    {
        const bool free = synthParams.lfos[(size_t) l].mode == sonder::LfoMode::free;
        const float phase = free || voice == nullptr ? (float) globalLfos[(size_t) l].phase : voice->getLfoPhase (l);
        lfoDisplayPhase[(size_t) l].store (phase);
    }

    // Живая модуляция для колец на ручках: голосовые цели - у последнего голоса, общие - у синта
    displayVoiceActive.store (voice != nullptr);
    for (size_t d = 0; d < displayModulation.size(); ++d)
    {
        const auto dest = static_cast<sonder::ModDest> (d);
        displayModulation[d].store (sonder::isGlobalDest (dest) ? globalModulation[(size_t) sonder::globalIndex (dest)]
                                    : voice != nullptr ? voice->getDisplayModulation()[d] : 0.0f);
    }

    // 0 - нет звучащего голоса, интерфейс рисует фильтр по положению ручек
    for (int f = 0; f < sonder::kNumFilters; ++f)
    {
        displayCutoff[(size_t) f].store (voice != nullptr ? voice->getDisplayCutoff (f) : 0.0f);
        displayVowel[(size_t) f].store (voice != nullptr ? voice->getDisplayVowel (f) : synthParams.filters[(size_t) f].vowel);
    }

    for (int v = 0; v < sonder::VoiceManager::kMaxVoices; ++v)
    {
        const auto& each = voiceManager.getVoice (v);
        displayVoiceLevel[(size_t) v].store (each.getDisplayLevel());
        displayVoicePitch[(size_t) v].store (each.getCurrentPitch());
    }

    displayCold.store (coldAmount);
    displaySag.store (sagAmount);
}

void SonderAudioProcessor::renderChunk (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi,
                                        int startSample, int numSamples,
                                        const sonder::SynthParams& synthParams, const Transport& transport)
{
    auto block = juce::dsp::AudioBlock<float> (buffer).getSubBlock ((size_t) startSample, (size_t) numSamples);

    // processSamplesUp отдаёт внутренний буфер передискретизации: рендерим синт прямо в него
    auto oversampledBlock = oversampling->processSamplesUp (block);
    oversampledBlock.clear();

    const int factor = (int) oversampling->getOversamplingFactor();
    const int numOversampled = (int) oversampledBlock.getNumSamples();
    float* left = oversampledBlock.getChannelPointer (0);
    float* right = oversampledBlock.getChannelPointer (1);

    // Общие данные для голосов: таблицы, фазы Free-LFO
    auto bus = analogBus;
    for (int osc = 0; osc < sonder::kNumOscs; ++osc)
        bus.wavetables[(size_t) osc] = wavetables.get (osc);

    for (int l = 0; l < sonder::kNumLfos; ++l)
    {
        auto& lfo = globalLfos[(size_t) l];
        bus.lfoTables[(size_t) l] = lfoShapes.getTable (l);
        bus.lfoPoints[(size_t) l] = lfoShapes.getPointSet (l);
        bus.lfoIncrement[(size_t) l] = synthParams.lfos[(size_t) l].rateHz / (float) oversampledRate;
        bus.lfoPhase[(size_t) l] = (float) lfo.phase;
        bus.lfoCycle[(size_t) l] = lfo.cycle;

        const double advanced = lfo.phase + (double) bus.lfoIncrement[(size_t) l] * numOversampled;
        const double whole = std::floor (advanced);
        lfo.phase = advanced - whole;
        lfo.cycle += (uint32_t) whole;
    }

    oversampledMidi.clear();
    for (const auto metadata : midi)
        if (metadata.samplePosition >= startSample && metadata.samplePosition < startSample + numSamples)
            oversampledMidi.addEvent (metadata.getMessage(), (metadata.samplePosition - startSample) * factor);

    std::fill_n (tapBuffer.begin(), numOversampled, 0.0f);
    bus.displayTap = tapBuffer.data();
    bus.ranges = &destRanges;

    std::fill (leaderWritten.begin(), leaderWritten.end(), (uint8_t) 0);
    bus.leaderSources = leaderSources.data();
    bus.leaderWritten = leaderWritten.data();

    voiceManager.render (left, right, numOversampled, oversampledMidi, synthParams, bus);

    // Огибающая громкости для просадки питания
    float sumOfSquares = 0.0f;
    for (int i = 0; i < numOversampled; ++i)
    {
        const float mono = 0.5f * (left[i] + right[i]);
        sumOfSquares += mono * mono;
    }

    // Сигнал до фильтра и после него для спектра на экране фильтра, прореженный до обычной частоты
    {
        constexpr int batch = 128;
        float pre[batch], post[batch];
        const float norm = 1.0f / (float) factor;

        for (int start = 0; start < numSamples; start += batch)
        {
            const int count = juce::jmin (batch, numSamples - start);

            for (int i = 0; i < count; ++i)
            {
                float preSum = 0.0f, postSum = 0.0f;
                for (int k = 0; k < factor; ++k)
                {
                    const int index = (start + i) * factor + k;
                    preSum += tapBuffer[(size_t) index];
                    postSum += 0.5f * (left[index] + right[index]);
                }

                pre[i] = preSum * norm;
                post[i] = postSum * norm;
            }

            preFilterTap.push (pre, count);
            postFilterTap.push (post, count);
        }
    }

    const float rms = std::sqrt (sumOfSquares / (float) juce::jmax (1, numOversampled));
    const float chunkSeconds = (float) numSamples / (float) currentSampleRate;
    const float sagTime = rms > sagEnvelope ? 0.03f : 0.3f;
    sagEnvelope += (rms - sagEnvelope) * (1.0f - std::exp (-chunkSeconds / sagTime));

    // Выходной каскад синта: мягкое насыщение вместо цифрового клиппинга
    const float stageGain = juce::Decibels::decibelsToGain (kOutputStageDb);
    for (int i = 0; i < numOversampled; ++i)
    {
        left[i] = sonder::fastTanh (left[i] * stageGain);
        right[i] = sonder::fastTanh (right[i] * stageGain);
    }

    oversampling->processSamplesDown (block);

    // Рэк эффектов работает на обычной частоте дискретизации
    float* outLeft = block.getChannelPointer (0);
    float* outRight = block.getChannelPointer (1);

    processEffects (outLeft, outRight, numSamples, synthParams, bus, transport);
    displayBpm.store ((float) transport.bpm);
    scope.push (outLeft, outRight, numSamples);
}

void SonderAudioProcessor::computeGlobalSources (int record, const sonder::ModulationBus& bus,
                                                 const sonder::SynthParams& synthParams)
{
    using S = sonder::ModSource;
    constexpr auto numSources = (size_t) S::count;

    // Звучит ведущий голос - берём его источники как есть
    if (leaderWritten[(size_t) record] != 0)
    {
        std::copy_n (leaderSources.begin() + (std::ptrdiff_t) ((size_t) record * numSources), numSources, globalSources.begin());
        return;
    }

    // Иначе: LFO в режиме Free идут дальше по общей фазе, контроллеры - как есть,
    // огибающие молчат, остальное держит последнее значение
    const int offset = record * sonder::ModulationBus::kLeaderInterval;

    for (int l = 0; l < sonder::kNumLfos; ++l)
    {
        if (synthParams.lfos[(size_t) l].mode != sonder::LfoMode::free)
            continue;

        const float position = bus.lfoPhase[(size_t) l] + bus.lfoIncrement[(size_t) l] * (float) offset;
        const float whole = std::floor (position);
        globalSources[(size_t) sonder::sourceForLfo (l)]
            = sonder::LfoMath::evaluate (synthParams.lfos[(size_t) l].shape, position - whole,
                                         bus.lfoCycle[(size_t) l] + (uint32_t) whole, 0x1f0u + (uint32_t) l, bus.lfoTables[(size_t) l]);
    }

    if (voiceManager.getDisplayVoice() == nullptr)
    {
        globalSources[(size_t) S::filterEnv] = 0.0f;
        globalSources[(size_t) S::ampEnv] = 0.0f;
    }

    globalSources[(size_t) S::modWheel] = voiceManager.getModWheel();
    globalSources[(size_t) S::aftertouch] = voiceManager.getAftertouch();
    globalSources[(size_t) S::slide] = voiceManager.getSlide();
}

void SonderAudioProcessor::processEffects (float* left, float* right, int numSamples, const sonder::SynthParams& synthParams,
                                           const sonder::ModulationBus& bus, const Transport& transport)
{
    using namespace sonder;

    // Слоты матрицы с общими целями
    std::array<ModSlot, kNumModSlots> slots {};
    int numSlots = 0;
    bool fxModulated = false, masterModulated = false;

    for (const auto& slot : synthParams.modSlots)
    {
        if (! synthParams.isActive (slot) || ! isGlobalDest (slot.dest))
            continue;

        slots[(size_t) numSlots++] = slot;
        fxModulated |= slot.dest >= ModDest::fxFirst;
        masterModulated |= slot.dest == ModDest::master;
    }

    // Master - последним, после эффектов. Пики выше 0 dBFS страхует мягкий лимитер
    const auto applyMaster = [this] (float* l, float* r, int count, float fromGain, float toGain)
    {
        for (int i = 0; i < count; ++i)
        {
            const float gain = masterGain.getNextValue() * (fromGain + (toGain - fromGain) * (float) (i + 1) / (float) count);
            l[i] = softLimit (l[i] * gain);
            r[i] = softLimit (r[i] * gain);
        }
    };

    if (numSlots == 0)
    {
        globalModulation.fill (0.0f);
        fxChain.process (left, right, numSamples, fxRack, params.fxParams, params.fxOn, transport.bpm);
        applyMaster (left, right, numSamples, masterModGain, 1.0f);
        masterModGain = 1.0f;
        return;
    }

    // Модуляция считается по записям ведущего голоса; эффекты обрабатываются кусками той же длины
    const int factor = (int) oversampling->getOversamplingFactor();
    const int chunk = ModulationBus::kLeaderInterval / factor;
    const float masterBase = params.masterGain->load();

    for (int start = 0, record = 0; start < numSamples; start += chunk, ++record)
    {
        const int length = juce::jmin (chunk, numSamples - start);
        computeGlobalSources (record, bus, synthParams);

        for (const auto& slot : slots)
            if (slot.source != ModSource::off)
                globalModulation[(size_t) globalIndex (slot.dest)] = 0.0f;

        for (int s = 0; s < numSlots; ++s)
        {
            const auto& slot = slots[(size_t) s];
            globalModulation[(size_t) globalIndex (slot.dest)]
                += globalSources[(size_t) slot.source] * slot.scale + slot.offset;
        }

        if (fxModulated)
        {
            fxModulation.fill (0.0f);
            for (int s = 0; s < numSlots; ++s)
                if (slots[(size_t) s].dest >= ModDest::fxFirst)
                {
                    const auto index = (size_t) ((int) slots[(size_t) s].dest - (int) ModDest::fxFirst);
                    fxModulation[index] = globalModulation[(size_t) globalIndex (slots[(size_t) s].dest)];
                }
        }

        fxChain.process (left + start, right + start, length, fxRack, params.fxParams, params.fxOn, transport.bpm,
                         fxModulated ? fxModulation.data() : nullptr);

        // Master модулируется в децибелах своей шкалы; внутри куска громкость меняется плавно
        float targetGain = 1.0f;
        if (masterModulated)
            targetGain = juce::Decibels::decibelsToGain (globalModulated (ModDest::master, masterBase) - masterBase);

        applyMaster (left + start, right + start, length, masterModGain, targetGain);
        masterModGain = targetGain;
    }

    // Цели, которые больше никто не модулирует, возвращаются к нулю
    std::array<bool, kNumGlobalDests> active {};
    for (int s = 0; s < numSlots; ++s)
        active[(size_t) globalIndex (slots[(size_t) s].dest)] = true;

    for (size_t g = 0; g < active.size(); ++g)
        if (! active[g])
            globalModulation[g] = 0.0f;
}

int SonderAudioProcessor::getNumPrograms()
{
    return (int) sonder::getFactoryPresets().size();
}

int SonderAudioProcessor::getCurrentProgram()
{
    const int index = presetManager.getCurrentIndex();
    return index < getNumPrograms() ? index : 0;
}

void SonderAudioProcessor::setCurrentProgram (int index)
{
    presetManager.load (index);
}

const juce::String SonderAudioProcessor::getProgramName (int index)
{
    const auto& presets = sonder::getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? juce::String (presets[(size_t) index].name) : juce::String();
}

juce::AudioProcessorEditor* SonderAudioProcessor::createEditor()
{
    return new SonderAudioProcessorEditor (*this);
}

void SonderAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
    {
        // Формы LFO и выбор wavetable не параметры - сохраняем рядом
        xml->addChildElement (lfoShapes.toXml().release());
        xml->addChildElement (wavetables.toXml().release());
        xml->addChildElement (fxController.toXml().release());
        xml->addChildElement (tuning.toXml().release());
        xml->setAttribute ("quality", quality.load());
        copyXmlToBinary (*xml, destData);
    }
}

void SonderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    lfoShapes.fromXml (xml->getChildByName ("LfoShapes"));
    wavetables.fromXml (xml->getChildByName ("Wavetables"));

    // Что стоит в слотах рэка: без этого значения ручек слотов ничего не значат
    std::unique_ptr<juce::XmlElement> rackXml;
    if (const auto* rackElement = xml->getChildByName ("FxRack"))
        rackXml = std::make_unique<juce::XmlElement> (*rackElement);

    tuning.fromXml (xml->getChildByName ("Tuning"));
    setQuality (static_cast<Quality> (juce::jlimit (0, 2, xml->getIntAttribute ("quality", (int) Quality::normal))));
    xml->removeAttribute ("quality");
    xml->deleteAllChildElementsWithTagName ("Tuning");
    xml->deleteAllChildElementsWithTagName ("LfoShapes");
    xml->deleteAllChildElementsWithTagName ("Wavetables");
    xml->deleteAllChildElementsWithTagName ("FxRack");

    parameters.replaceState (juce::ValueTree::fromXml (*xml));

    if (rackXml != nullptr)
        fxController.fromXml (*rackXml);
    else
        fxController.clear();

    presetManager.syncWithState();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SonderAudioProcessor();
}
