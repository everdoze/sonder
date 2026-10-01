#pragma once

#include "Parameters.h"
#include "ScopeBuffer.h"
#include "dsp/LfoShapes.h"
#include "dsp/WavetableBank.h"
#include "fx/FxChain.h"
#include "fx/FxRackController.h"
#include "presets/PresetManager.h"
#include "synth/Arpeggiator.h"
#include "synth/Tuning.h"
#include "synth/VoiceManager.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

class SonderAudioProcessor final : public juce::AudioProcessor
{
public:
    SonderAudioProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    // DAW видит заводские пресеты как программы
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Рэк объявлен раньше параметров: параметры слотов берут у него имена и единицы
    sonder::FxRack fxRack;
    juce::AudioProcessorValueTreeState parameters;
    sonder::LfoShapeBank lfoShapes;
    sonder::WavetableBank wavetables;
    sonder::FxRackController fxController;
    sonder::PresetManager presetManager;
    juce::MidiKeyboardState keyboardState;
    sonder::ScopeBuffer scope;
    sonder::Tuning tuning;

    // Шкалы ручек для модуляции "в долях хода" (цель -> шкала её ручки)
    const sonder::DestRanges& getDestRanges() const noexcept { return destRanges; }

    // Указатели на значения параметров: интерфейсу, который опрашивает матрицу модуляции на каждом кадре,
    // не нужно каждый раз искать параметр по строке
    const sonder::ParameterRefs& getParameterRefs() const noexcept { return params; }
    const sonder::Arpeggiator& getArpeggiator() const noexcept { return arpeggiator; }

    // Для интерфейса: какие голоса звучат, фазы LFO, текущий срез фильтра
    std::atomic<uint32_t> activeVoiceMask { 0 };
    std::array<std::atomic<float>, sonder::kNumLfos> lfoDisplayPhase {};
    std::array<std::atomic<float>, sonder::kNumFilters> displayCutoff {}, displayVowel {};
    std::array<std::atomic<float>, (size_t) sonder::ModDest::count> displayModulation {};
    std::atomic<bool> displayVoiceActive { false };

    // Уровень и высота каждого голоса (индикаторы голосов), "холод" прогрева и просадка питания (0..1)
    std::array<std::atomic<float>, sonder::VoiceManager::kMaxVoices> displayVoiceLevel {}, displayVoicePitch {};
    std::atomic<float> displayCold { 0.0f }, displaySag { 0.0f };

    // Сумма голосов до фильтра и после него (до эффектов): спектр на экране фильтра
    sonder::SampleRing preFilterTap, postFilterTap;

    // Для визуализации эффектов
    std::atomic<float> displayBpm { 120.0f };
    const sonder::FxChain& getFxChain() const noexcept { return fxChain; }

    // Качество: голоса считаются на частоте ×1 (Eco), ×2 (Normal) или ×4 (High).
    // Хранится в проекте, а не в пресете: это вопрос нагрузки на процессор, а не тембра
    // При рендере в файл (хост включил offline-режим) голоса всегда считаются в High.
    // Задержка, которую видит хост, от качества не зависит: меньшие качества добирают её до High сами,
    // поэтому переключение (и переход к рендеру) не сдвигает звук по времени
    enum class Quality { eco, normal, high };
    Quality getQuality() const noexcept { return static_cast<Quality> (quality.load()); }
    void setQuality (Quality newQuality);
    Quality getRenderingQuality() const noexcept { return isNonRealtime() ? Quality::high : getQuality(); }
    void setNonRealtime (bool offline) noexcept override;

private:

    struct Transport
    {
        double bpm = 120.0;
        std::optional<double> ppq;
        bool isPlaying = false;
    };

    struct GlobalLfo
    {
        double phase = 0.0;
        uint32_t cycle = 0;
    };

    Transport readTransport() const;
    void runArpeggiator (juce::MidiBuffer& midi, int numSamples, const Transport& transport);
    void computeGlobalSources (int record, const sonder::ModulationBus& bus, const sonder::SynthParams& synthParams);
    void processEffects (float* left, float* right, int numSamples, const sonder::SynthParams& synthParams,
                         const sonder::ModulationBus& bus, const Transport& transport);
    float globalModulated (sonder::ModDest dest, float base) const noexcept;
    void renderChunk (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int startSample, int numSamples,
                      const sonder::SynthParams& synthParams, const Transport& transport);
    void updateAnalogState (int numSamples);
    void updateDisplayState (const sonder::SynthParams& synthParams);

    sonder::ParameterRefs params;
    sonder::DestRanges destRanges;
    sonder::VoiceManager voiceManager;
    sonder::Arpeggiator arpeggiator;
    std::array<GlobalLfo, sonder::kNumLfos> globalLfos {};

    sonder::FxChain fxChain;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::MidiBuffer oversampledMidi;
    juce::SmoothedValue<float> masterGain;
    double currentSampleRate = 44100.0, oversampledRate = 88200.0;
    int maxBlockSize = 0;
    std::atomic<int> quality { (int) Quality::normal };
    int preparedQuality = -1;
    void applyQuality();

    // Добавочная задержка до задержки High (см. Quality)
    static constexpr int kMaxLatencyPad = 16;
    int latencyPad = 0, padPosition = 0;
    std::array<std::array<float, kMaxLatencyPad>, 2> padBuffer {};
    void applyLatencyPad (juce::AudioBuffer<float>& buffer) noexcept;

    // Сон при простое: нет нот, нет голосов, а выход уже kSleepAfterSeconds тише kSilenceLevel
    // (хвосты реверба и дилея успели затихнуть) - блок не считается, на выходе тишина.
    // Любое MIDI-событие будит сразу.
    static constexpr double kSleepAfterSeconds = 1.0;
    static constexpr float kSilenceLevel = 3.16e-5f; // -90 dB: ниже шипения хоруса, тише слышимого
    double silentSeconds = 0.0;
    bool sleeping (const juce::MidiBuffer& midi) const noexcept;
    void sleepThrough (int numSamples, const sonder::SynthParams& synthParams);

    // Просадка питания: огибающая громкости, прогрев: время с "включения"
    float sagEnvelope = 0.0f;
    float coldAmount = 0.0f, sagAmount = 0.0f;
    std::vector<float> tapBuffer;
    double warmupSeconds = 0.0;
    float lastWarmupAmount = -1.0f;
    sonder::ModulationBus analogBus;

    // Общие модуляции (рэк, Master, Sag, Warm-up): источники ведущего голоса по записям,
    // последние известные источники и итог последней записи
    std::vector<float> leaderSources;
    std::vector<uint8_t> leaderWritten;
    std::array<float, (size_t) sonder::ModSource::count> globalSources {};
    std::array<float, sonder::kNumGlobalDests> globalModulation {};
    std::array<float, sonder::kNumFxSlots * sonder::kNumFxParams> fxModulation {};
    float masterModGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonderAudioProcessor)
};
