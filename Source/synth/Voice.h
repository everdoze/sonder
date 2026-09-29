#pragma once

#include "SynthParams.h"
#include "dsp/AnalogEnvelope.h"
#include "dsp/Distortion.h"
#include "dsp/FastRandom.h"
#include "dsp/FormantFilter.h"
#include "dsp/LadderFilter.h"
#include "dsp/PolyBlepOscillator.h"
#include "dsp/SmoothNoise.h"

namespace sonder
{

// Один голос: до 4 слоёв унисона (у каждого свои осцилляторы и свой дрейф),
// стерео-пара фильтров (ladder или формантный), дисторшн, две огибающие,
// собственные LFO, мод-матрица и глайд.
class Voice
{
public:
    static constexpr int kMaxUnison = 4;

    void prepare (double sampleRate);

    // glideFromPitch < 0: без глайда. sampleOffset - позиция ноты в блоке (для фазы Free LFO)
    void start (int midiNote, float noteVelocity, float glideFromPitch, bool retriggerEnvelopes,
                const SynthParams& params, const ModulationBus& bus, int sampleOffset);
    void glideTo (int midiNote) noexcept;
    void release() noexcept;
    void kill() noexcept;

    bool isActive() const noexcept    { return active; }
    bool isReleasing() const noexcept { return releasing; }
    int getNote() const noexcept      { return note; }
    float getCurrentPitch() const noexcept { return currentPitch; }

    // Для интерфейса
    float getLfoPhase (int lfo) const noexcept { return lfoStates[(size_t) lfo].phase; }
    float getDisplayCutoff() const noexcept    { return displayCutoff; }
    float getDisplayVowel() const noexcept     { return displayVowel; }
    const std::array<float, (size_t) ModDest::count>& getDisplayModulation() const noexcept { return displayModulation; }

    void render (float* left, float* right, int startSample, int numSamples,
                 const SynthParams& params, const ModulationBus& bus, const Tolerances& tolerances) noexcept;

private:
    struct Layer
    {
        PolyBlepOscillator osc1, osc2;
        SmoothNoise drift1, drift2, jitter1, jitter2;
        float ratio1 = 1.0f, ratio2 = 1.0f;
        int level1 = 0, level2 = 0; // mip-уровни wavetable
    };

    struct LfoState
    {
        float phase = 0.0f;
        uint32_t cycle = 0, seed = 0;
        bool finished = false;
    };

    std::array<Layer, kMaxUnison> layers;
    std::array<LfoState, kNumLfos> lfoStates;
    LadderFilter filterLeft, filterRight;
    FormantFilter formantLeft, formantRight;
    Distortion distortionLeft, distortionRight;
    AnalogEnvelope ampEnvelope, filterEnvelope;
    SmoothNoise cutoffDrift;
    FastRandom noise;

    float sampleRate = 44100.0f;
    float piOverSampleRate = 0.0f, maxCutoff = 20000.0f;

    bool active = false, releasing = false;
    int note = 60;
    float velocity = 1.0f, randomValue = 0.0f;
    float currentPitch = 60.0f, targetPitch = 60.0f;
    float vibratoPhase = 0.0f;
    float lastFrequency1 = 440.0f, lastFrequency2 = 440.0f;

    float smoothedCutoff = 1000.0f, cutoffSmoothingCoef = 1.0f;
    bool snapCutoff = true;
    float cutoffDriftValue = 0.0f;
    int controlCounter = 0;

    float displayCutoff = 1000.0f, displayVowel = 0.0f;
    std::array<float, (size_t) ModDest::count> displayModulation {};
};

} // namespace sonder
