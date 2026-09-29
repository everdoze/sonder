#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Saturation.h"
#include "presets/FactoryPresets.h"

namespace
{
    constexpr double kWarmupTimeConstantSeconds = 20.0;
    constexpr float kMaxWarmupDetuneCents = 40.0f;
    constexpr float kMaxSagCents = 25.0f;
    constexpr float kMaxSagGainDrop = 0.3f;
}

SonderAudioProcessor::SonderAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "Parameters", sonder::createParameterLayout()),
      presetManager (parameters),
      params (parameters)
{
}

bool SonderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SonderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = juce::jmax (1, samplesPerBlock);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (2, kOversamplingOrder,
                                                                      juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                                                                      true, false);
    oversampling->initProcessing ((size_t) maxBlockSize);

    // Синтез идёт на удвоенной частоте: меньше алиасинга от нелинейностей, эффекты на обычной
    const auto factor = (int) oversampling->getOversamplingFactor();
    const double oversampledRate = sampleRate * factor;
    const int maxOversampledBlock = maxBlockSize * factor;

    voiceManager.prepare (oversampledRate, maxOversampledBlock);
    lfo1.prepare (oversampledRate, 0x11f01u);
    lfo2.prepare (oversampledRate, 0x22f02u);
    lfo1Buffer.assign ((size_t) maxOversampledBlock, 0.0f);
    lfo2Buffer.assign ((size_t) maxOversampledBlock, 0.0f);
    oversampledMidi.ensureSize (4096);

    chorus.prepare (sampleRate);
    delay.prepare (sampleRate);
    reverb.setSampleRate (sampleRate);
    reverb.reset();

    masterGain.reset (oversampledRate, 0.05);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.masterGain->load()));

    scope.sampleRate.store ((float) sampleRate);
    setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples()));
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

float SonderAudioProcessor::lfoRate (std::atomic<float>* rate, std::atomic<float>* sync, double bpm) const
{
    const double beats = sonder::syncDivisionInBeats ((int) sync->load());
    return beats > 0.0 ? (float) (bpm / 60.0 / beats) : rate->load();
}

void SonderAudioProcessor::updateAnalogState (int numSamples)
{
    // Прогрев: после "включения" (загрузки или поворота ручки) генераторы дрейфуют сильнее
    // и строй занижен, за ~минуту всё успокаивается
    const float warmupAmount = params.warmup->load();
    if (std::abs (warmupAmount - lastWarmupAmount) > 0.01f)
    {
        warmupSeconds = 0.0;
        lastWarmupAmount = warmupAmount;
    }

    const auto cold = (float) std::exp (-warmupSeconds / kWarmupTimeConstantSeconds);
    warmupSeconds += numSamples / currentSampleRate;

    // Просадка питания: чем громче и плотнее играем, тем ниже строй и тише голоса
    const float sagDepth = params.sag->load() * juce::jmin (1.0f, sagEnvelope * 2.0f);

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

    const auto synthParams = sonder::SynthParams::fromRefs (params);
    const auto transport = readTransport();

    // Синхронизированные LFO привязываем к позиции транспорта, чтобы фаза совпадала с тактом
    if (transport.isPlaying && transport.ppq.has_value())
    {
        const auto lockPhase = [&transport] (sonder::Lfo& lfo, std::atomic<float>* sync)
        {
            const double beats = sonder::syncDivisionInBeats ((int) sync->load());
            if (beats > 0.0)
                lfo.setPhase ((float) std::fmod (*transport.ppq / beats, 1.0));
        };

        lockPhase (lfo1, params.lfo1Sync);
        lockPhase (lfo2, params.lfo2Sync);
    }

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (params.masterGain->load()));

    // Хост может прислать блок больше заявленного в prepareToPlay, режем на куски
    for (int start = 0; start < numSamples; start += maxBlockSize)
    {
        const int length = juce::jmin (maxBlockSize, numSamples - start);
        updateAnalogState (length);
        renderChunk (buffer, midi, start, length, synthParams, transport);
    }

    activeVoiceMask.store (voiceManager.getActiveVoiceMask());
    lfo1Phase.store (lfo1.getPhase());
    lfo2Phase.store (lfo2.getPhase());
    scope.noteFrequency.store (voiceManager.getLastNoteFrequency());
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

    // LFO
    const float rate1 = lfoRate (params.lfo1Rate, params.lfo1Sync, transport.bpm);
    const float rate2 = lfoRate (params.lfo2Rate, params.lfo2Sync, transport.bpm);
    const auto shape1 = static_cast<sonder::Lfo::Shape> ((int) params.lfo1Shape->load());
    const auto shape2 = static_cast<sonder::Lfo::Shape> ((int) params.lfo2Shape->load());

    for (int i = 0; i < numOversampled; ++i)
    {
        lfo1Buffer[(size_t) i] = lfo1.process (rate1, shape1);
        lfo2Buffer[(size_t) i] = lfo2.process (rate2, shape2);
    }

    auto bus = analogBus;
    bus.lfo1 = lfo1Buffer.data();
    bus.lfo2 = lfo2Buffer.data();

    oversampledMidi.clear();
    for (const auto metadata : midi)
        if (metadata.samplePosition >= startSample && metadata.samplePosition < startSample + numSamples)
            oversampledMidi.addEvent (metadata.getMessage(), (metadata.samplePosition - startSample) * factor);

    voiceManager.render (left, right, numOversampled, oversampledMidi, synthParams, bus);

    // Огибающая громкости для просадки питания
    float sumOfSquares = 0.0f;
    for (int i = 0; i < numOversampled; ++i)
    {
        const float mono = 0.5f * (left[i] + right[i]);
        sumOfSquares += mono * mono;
    }

    const float rms = std::sqrt (sumOfSquares / (float) juce::jmax (1, numOversampled));
    const float chunkSeconds = (float) numSamples / (float) currentSampleRate;
    const float sagTime = rms > sagEnvelope ? 0.03f : 0.3f;
    sagEnvelope += (rms - sagEnvelope) * (1.0f - std::exp (-chunkSeconds / sagTime));

    // Выходной каскад синта: громкость и мягкое насыщение вместо цифрового клиппинга
    for (int i = 0; i < numOversampled; ++i)
    {
        const float gain = masterGain.getNextValue();
        left[i] = sonder::fastTanh (left[i] * gain);
        right[i] = sonder::fastTanh (right[i] * gain);
    }

    oversampling->processSamplesDown (block);

    // Эффекты на обычной частоте дискретизации
    float* outLeft = block.getChannelPointer (0);
    float* outRight = block.getChannelPointer (1);

    chorus.process (outLeft, outRight, numSamples,
                    static_cast<sonder::Chorus::Mode> ((int) params.chorusMode->load()), params.chorusMix->load());

    const double delayBeats = sonder::syncDivisionInBeats ((int) params.delaySync->load());
    const float delaySeconds = delayBeats > 0.0 ? (float) (delayBeats * 60.0 / transport.bpm) : params.delayTime->load();
    delay.process (outLeft, outRight, numSamples, delaySeconds,
                   params.delayFeedback->load(), params.delayMix->load(), params.delayTape->load());

    const float reverbMix = params.reverbMix->load();
    if (reverbMix > 0.001f)
    {
        juce::Reverb::Parameters reverbParams;
        reverbParams.roomSize = 0.3f + 0.69f * params.reverbSize->load();
        reverbParams.damping = 0.45f;
        reverbParams.wetLevel = 0.4f * reverbMix;
        reverbParams.dryLevel = 1.0f - 0.35f * reverbMix;
        reverbParams.width = 1.0f;
        reverb.setParameters (reverbParams);
        reverb.processStereo (outLeft, outRight, numSamples);
    }

    // Эффекты могут поднять пики выше 0 dBFS: страхуем мягким лимитером
    for (int i = 0; i < numSamples; ++i)
    {
        outLeft[i] = sonder::softLimit (outLeft[i]);
        outRight[i] = sonder::softLimit (outRight[i]);
    }

    scope.push (outLeft, outRight, numSamples);
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
        copyXmlToBinary (*xml, destData);
}

void SonderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (parameters.state.getType()))
        {
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
            presetManager.syncWithState();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SonderAudioProcessor();
}
