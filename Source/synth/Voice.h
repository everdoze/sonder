#pragma once

#include "SynthParams.h"
#include "dsp/AnalogEnvelope.h"
#include "dsp/Distortion.h"
#include "dsp/FastRandom.h"
#include "dsp/FormantFilter.h"
#include "dsp/LadderFilter.h"
#include "dsp/NoiseGenerator.h"
#include "dsp/PolyBlepOscillator.h"
#include "dsp/SmoothNoise.h"

namespace sonder
{

// Один голос: до 4 слоёв унисона (у каждого до четырёх осцилляторов со своим дрейфом),
// стерео-пара фильтров (ladder или формантный), дисторшн до или после фильтра, две огибающие,
// собственные LFO, мод-матрица и глайд с настраиваемой кривой.
// Высота ноты приходит готовой (с учётом строя), у ноты может быть своя выразительность (MPE):
// бенд, давление и слайд.
class Voice
{
public:
    static constexpr int kMaxUnison = 4;

    void prepare (double sampleRate);

    // pitch - высота ноты в MIDI-единицах (строй уже учтён). glideFromPitch < 0: без глайда.
    // sampleOffset - позиция ноты в блоке (для фазы Free LFO)
    void start (int midiNote, float pitch, float noteVelocity, float glideFromPitch, bool retriggerEnvelopes,
                const SynthParams& params, const ModulationBus& bus, int sampleOffset);

    // Кража звучащего голоса: старая нота быстро гаснет, и только потом стартует новая.
    // Без этого высота и фильтр прыгали бы прямо на звучащем голосе.
    void stealTo (int midiNote, float pitch, float noteVelocity, float glideFromPitch) noexcept;

    // Смена ноты без перезапуска огибающих (легато, возврат к зажатой ноте в моно).
    // glide = false: высота меняется сразу.
    void glideTo (int midiNote, float pitch, bool glide) noexcept;
    void release() noexcept;
    void kill() noexcept;

    // Выразительность ноты: канал MIDI, бенд в полутонах, давление и слайд 0..1
    void setChannel (int newChannel) noexcept          { channel = newChannel; }
    int getChannel() const noexcept                     { return channel; }
    void setExpression (float bendSemitones, float pressure, float slide, bool jump) noexcept;
    void setNoteBend (float semitones) noexcept         { bendTarget = semitones; }
    void setPressure (float value) noexcept             { pressureTarget = value; }
    void setSlide (float value) noexcept                { slideTarget = value; }

    // Ведущий голос (последний взятый) записывает свои источники модуляции для общих целей
    void setLeader (bool isLeader) noexcept { leader = isLeader; }

    bool isActive() const noexcept    { return active; }
    bool isReleasing() const noexcept { return releasing; }
    bool isStealing() const noexcept  { return pendingNote >= 0; }
    int getNote() const noexcept      { return note; }
    float getCurrentPitch() const noexcept { return currentPitch; }

    // Для интерфейса
    float getLfoPhase (int lfo) const noexcept { return lfoStates[(size_t) lfo].phase; }
    float getDisplayCutoff (int filter) const noexcept { return filters[(size_t) filter].displayCutoff; }
    float getDisplayVowel (int filter) const noexcept  { return filters[(size_t) filter].displayVowel; }
    float getDisplayLevel() const noexcept     { return active ? displayLevel : 0.0f; }
    const std::array<float, kNumVoiceDests>& getDisplayModulation() const noexcept { return displayModulation; }

    void render (float* left, float* right, int startSample, int numSamples,
                 const SynthParams& params, const ModulationBus& bus, const Tolerances& tolerances) noexcept;

private:
    struct Layer
    {
        std::array<PolyBlepOscillator, kNumOscs> osc;
        std::array<SmoothNoise, kNumOscs> drift, jitter;
        std::array<float, kNumOscs> ratio { 1.0f, 1.0f, 1.0f, 1.0f };
        std::array<Wavetable::MipSelection, kNumOscs> mip;
        float lastO2 = 0.0f; // для FM, когда второй осциллятор синхронизирован с первым и считается после него
    };

    struct LfoState
    {
        float phase = 0.0f;
        uint32_t cycle = 0, seed = 0;
        bool finished = false;

        // Форма с модулированными точками (только если матрица двигает точки этого LFO)
        int numPoints = 0;
        std::array<LfoPoint, LfoPointSet::kMaxPoints> points {};
    };

    // Один фильтр голоса: стерео-пара ladder и формантных, сглаженные ручки, драйв
    struct FilterStage
    {
        LadderFilter left, right;
        FormantFilter formantLeft, formantRight;

        // Сглаженные ручки: без "ступенек" при быстром повороте
        float smoothedCutoff = 1000.0f, smoothedResonance = 0.0f, smoothedDrive = 0.3f;
        float cachedDrive = -1.0f, driveGain = 1.0f, makeup = 1.0f;
        float baseCutoff = 1000.0f, envOctaves = 0.0f; // пересчитываются раз в блок или на control rate

        float displayCutoff = 1000.0f, displayVowel = 0.0f;

        void reset() noexcept
        {
            left.reset();
            right.reset();
            formantLeft.reset();
            formantRight.reset();
        }
    };

    // Значения, которые меняются не чаще control rate: из ручек, а если их модулируют - с модуляцией
    struct SlowValues
    {
        int numLayers = 1;
        float layerNorm = 1.0f;
        std::array<float, kMaxUnison> layerDetune {}, layerGainLeft {}, layerGainRight {};
        float driftCents = 0.0f, cutoffDriftOctaves = 0.0f, jitterRatio = 0.0f;
        std::array<float, kNumOscs> staticCents {}, pulseWidthBase {};
        float gain = 1.0f, staticPan = 0.0f;
        float glideIncrement = 1.0f, glideK = 0.0f, glideNorm = 1.0f;
        bool glideLinear = true;
        float vibratoIncrement = 0.0f, vibratoDepth = 0.0f;
        float toneCoef = 1.0f;
    };

    void updateSlowValues (const SynthParams& p, const ModulationBus& bus, const Tolerances& tol,
                           const float* mod, const std::array<bool, kNumVoiceDests>& used) noexcept;
    void updateLfoPoints (int lfo, const ModulationBus& bus, const float* mod) noexcept;
    void launchPendingNote (const SynthParams& params, const ModulationBus& bus, int sampleOffset);

    std::array<Layer, kMaxUnison> layers;
    std::array<LfoState, kNumLfos> lfoStates;
    std::array<FilterStage, kNumFilters> filters;
    Distortion distortionLeft, distortionRight;
    AnalogEnvelope ampEnvelope, filterEnvelope;
    SmoothNoise cutoffDrift;
    NoiseGenerator noiseGenerator;
    FastRandom noise;
    SlowValues slow;

    float sampleRate = 44100.0f;
    float piOverSampleRate = 0.0f, maxCutoff = 20000.0f;

    bool active = false, releasing = false, leader = false;
    int note = 60;                // нота, за которую голос отвечает перед менеджером
    float soundingPitch = 60.0f;  // высота ноты, которая звучит сейчас (отличается, пока идёт фейд кражи)
    float velocity = 1.0f, randomValue = 0.0f;
    float currentPitch = 60.0f, targetPitch = 60.0f;
    float glideStart = 60.0f, glideProgress = 1.0f; // 1 - глайд закончен
    float vibratoPhase = 0.0f;
    std::array<float, kNumOscs> lastFrequency { 440.0f, 440.0f, 440.0f, 440.0f };

    // Выразительность ноты (MPE, полифонический афтертач): цели и сглаженные значения
    int channel = 1;
    float bendTarget = 0.0f, bendValue = 0.0f;
    float pressureTarget = 0.0f, pressureValue = 0.0f;
    float slideTarget = 0.0f, slideValue = 0.0f;

    // Отложенная нота при краже голоса
    int pendingNote = -1;
    float pendingPitch = 60.0f, pendingVelocity = 1.0f, pendingGlideFrom = -1.0f;
    bool pendingRelease = false;
    int stealFadeLength = 256, stealFadeLeft = 0;

    float parameterSmoothingCoef = 1.0f;
    bool snapParameters = true;

    float cutoffDriftValue = 0.0f;
    int controlCounter = 0;

    float displayLevel = 0.0f;
    std::array<float, kNumVoiceDests> displayModulation {};
};

} // namespace sonder
