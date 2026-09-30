#pragma once

#include "Tuning.h"
#include "Voice.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace sonder
{

// Распределение нот по голосам (Poly с ротацией голосов, как у аналоговых полифоников, Mono, Legato),
// педаль сустейна, колесо модуляции, питч-бенд и афтертач.
// Высота ноты берётся из строя (Tuning). Полифонический афтертач и MPE (канал на ноту: свой бенд,
// давление и слайд - CC74) доходят до своего голоса; в MPE первый канал - общий для всех нот.
class VoiceManager
{
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kNumUnits = 16;

    VoiceManager();

    void prepare (double sampleRate, int maxBlockSize);

    // MIDI-события уже с позициями в сэмплах этого (передискретизированного) блока
    void render (float* left, float* right, int numSamples, const juce::MidiBuffer& midi,
                 const SynthParams& params, ModulationBus bus);

    void allNotesOff (bool immediately);

    void setTuning (const Tuning* newTuning) noexcept { tuning = newTuning; }

    // Последние значения контроллеров (для общих модуляций, когда ни один голос не звучит)
    float getModWheel() const noexcept  { return modWheelValue; }
    float getAftertouch() const noexcept { return aftertouchValue; }
    float getSlide() const noexcept     { return slideValue; }

    uint32_t getActiveVoiceMask() const noexcept;
    float getLastNoteFrequency() const noexcept { return lastNoteFrequency; }

    // Последний запущенный звучащий голос: по нему интерфейс рисует LFO и фильтр
    const Voice* getDisplayVoice() const noexcept;
    const Voice& getVoice (int index) const noexcept { return voices[(size_t) index]; }

private:
    void handleMessage (const juce::MidiMessage& message, const SynthParams& params, const ModulationBus& bus, int position);
    void noteOn (int note, float velocity, int channel, const SynthParams& params, const ModulationBus& bus, int position);
    float pitchFor (int note) const noexcept { return tuning != nullptr ? tuning->pitchFor (note) : (float) note; }
    bool isMemberChannel (int channel, const SynthParams& params) const noexcept { return params.mpe && channel >= 2; }
    void startExpression (Voice& voice, int channel, const SynthParams& params);
    void noteOff (int note, const SynthParams& params);
    bool shouldGlide (const SynthParams& params, bool overlapping) const noexcept;
    void releaseSustainedNotes();
    int chooseVoice (int polyphony) const;
    void renderSegment (float* left, float* right, int start, int end, const SynthParams& params, ModulationBus& bus);

    void pushHeldNote (int note) noexcept;
    void removeHeldNote (int note) noexcept;

    std::array<Voice, kMaxVoices> voices;
    std::array<uint64_t, kMaxVoices> voiceOrder {};
    std::array<std::array<Tolerances, kMaxVoices>, kNumUnits> tolerances {};

    std::vector<float> modWheelBuffer, aftertouchBuffer, pitchBendBuffer;
    float modWheelTarget = 0.0f, aftertouchTarget = 0.0f, pitchBendTarget = 0.0f;
    float modWheelValue = 0.0f, aftertouchValue = 0.0f, pitchBendValue = 0.0f;
    float slideValue = 0.0f;
    float controlSmoothingCoef = 1.0f;

    // MPE: последние бенд (-1..1), давление и слайд каждого канала - нота берёт их при старте
    std::array<float, 17> channelBend {}, channelPressure {}, channelSlide {};
    const Tuning* tuning = nullptr;

    std::array<int, 128> heldNotes {};
    int numHeldNotes = 0;
    std::array<bool, 128> keyDown {}, sustained {};
    bool sustainPedal = false;

    VoiceMode currentMode = VoiceMode::poly;
    int nextVoice = 0;
    int lastStartedVoice = -1;
    uint64_t orderCounter = 0;
    float lastPitch = -1.0f;
    float lastNoteFrequency = 0.0f;
};

} // namespace sonder
