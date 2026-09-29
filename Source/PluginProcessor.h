#pragma once

#include "Parameters.h"
#include "ScopeBuffer.h"
#include "dsp/LfoShapes.h"
#include "dsp/WavetableBank.h"
#include "fx/Chorus.h"
#include "fx/TapeDelay.h"
#include "presets/PresetManager.h"
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

    juce::AudioProcessorValueTreeState parameters;
    sonder::LfoShapeBank lfoShapes;
    sonder::WavetableBank wavetables;
    sonder::PresetManager presetManager;
    juce::MidiKeyboardState keyboardState;
    sonder::ScopeBuffer scope;

    // Для интерфейса: какие голоса звучат, фазы LFO, текущий срез фильтра
    std::atomic<uint32_t> activeVoiceMask { 0 };
    std::array<std::atomic<float>, sonder::kNumLfos> lfoDisplayPhase {};
    std::atomic<float> displayCutoff { 0.0f }, displayVowel { 0.0f };
    std::array<std::atomic<float>, (size_t) sonder::ModDest::count> displayModulation {};
    std::atomic<bool> displayVoiceActive { false };

private:
    static constexpr int kOversamplingOrder = 1; // 2^1 = 2x

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
    void renderChunk (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int startSample, int numSamples,
                      const sonder::SynthParams& synthParams, const Transport& transport);
    void updateAnalogState (int numSamples);
    void updateDisplayState (const sonder::SynthParams& synthParams);

    sonder::ParameterRefs params;
    sonder::VoiceManager voiceManager;
    std::array<GlobalLfo, sonder::kNumLfos> globalLfos {};

    sonder::Chorus chorus;
    sonder::TapeDelay delay;
    juce::Reverb reverb;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::MidiBuffer oversampledMidi;
    juce::SmoothedValue<float> masterGain;
    double currentSampleRate = 44100.0, oversampledRate = 88200.0;
    int maxBlockSize = 0;

    // Просадка питания: огибающая громкости, прогрев: время с "включения"
    float sagEnvelope = 0.0f;
    double warmupSeconds = 0.0;
    float lastWarmupAmount = -1.0f;
    sonder::ModulationBus analogBus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonderAudioProcessor)
};
