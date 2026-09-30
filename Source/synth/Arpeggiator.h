#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <optional>

namespace sonder
{

// Арпеджиатор: зажатые ноты превращаются в последовательность нот с шагом Rate.
// Стоит перед распределением по голосам и заменяет ноты в MIDI-буфере блока. Остальные сообщения проходят как есть.
// При играющем транспорте хоста шаги привязаны к его сетке, без транспорта идёт свой такт от первой ноты.
class Arpeggiator
{
public:
    enum class Mode { up, down, upDown, random, asPlayed };

    struct Settings
    {
        bool on = false, hold = false;
        Mode mode = Mode::up;
        int octaves = 1;
        double stepBeats = 0.25;
        float gate = 0.6f;   // доля шага, которую звучит нота
        float swing = 0.0f;  // сдвиг каждого второго шага, в долях шага (0..0.75)
    };

    static constexpr int kMaxNotes = 32;
    static constexpr int kMaxPattern = kMaxNotes * 4;

    void prepare (double sampleRate);
    void reset();

    void process (juce::MidiBuffer& midi, int numSamples, const Settings& settings,
                  double bpm, std::optional<double> hostPpq);

    // Для интерфейса: ноты шаблона по порядку и номер текущего шага (-1 - ничего не играет)
    int getPatternSize() const noexcept { return displaySize.load(); }
    int getPatternNote (int index) const noexcept { return displayPattern[(size_t) index].load(); }
    int getCurrentStep() const noexcept { return displayStep.load(); }

private:
    struct HeldNote
    {
        int note = 0;
        juce::uint8 velocity = 100;
    };

    void noteDown (int note, juce::uint8 velocity, const Settings& settings);
    void noteUp (int note, const Settings& settings);
    void rebuildPattern (const Settings& settings);
    void runSteps (juce::MidiBuffer& out, int from, int to, const Settings& settings);
    double stepBeat (long long step, const Settings& settings) const noexcept;
    void catchUpStep (juce::MidiBuffer& out, int at, const Settings& settings);
    void playStep (juce::MidiBuffer& out, int position, const Settings& settings);
    void stopNote (juce::MidiBuffer& out, int position);
    void publish();

    double sampleRate = 44100.0;
    double beatsPerSample = 0.0;
    double stepSamples = 1.0;

    // Зажатые ноты в порядке нажатия; с Hold - запомненный аккорд
    std::array<HeldNote, kMaxNotes> held {};
    int numHeld = 0;
    std::array<bool, 128> physicallyDown {};
    int numPhysicallyDown = 0;

    std::array<int, kMaxPattern> pattern {};
    std::array<juce::uint8, kMaxPattern> patternVelocity {};
    int patternSize = 0;

    double clockBeats = 0.0;      // положение такта арпеджиатора на начало блока
    bool clockRunning = false, synced = false;
    long long nextStep = 0;       // номер следующего шага
    int position = 0;             // счётчик сыгранных шагов (для выбора ноты)
    int lastIndex = -1;

    double lastStepAt = 0.0;      // сэмпл последнего шага от начала текущего блока
    int lastStepPosition = 0;     // счётчик шагов на момент последнего шага
    bool lastStepValid = false;

    int sounding = -1;            // звучащая нота арпеджиатора
    double offCountdown = 0.0;    // когда её отпустить: сэмпл от начала текущего блока
    bool wasOn = false;
    juce::Random random;

    std::array<std::atomic<int>, kMaxPattern> displayPattern {};
    std::atomic<int> displaySize { 0 }, displayStep { -1 };
};

} // namespace sonder
