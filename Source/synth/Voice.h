#pragma once

#include "SynthParams.h"
#include "dsp/AnalogEnvelope.h"
#include "dsp/FastRandom.h"
#include "dsp/LadderFilter.h"
#include "dsp/PolyBlepOscillator.h"
#include "dsp/SmoothNoise.h"

namespace sonder
{

// Один голос: до 4 слоёв унисона (у каждого свои осцилляторы и свой дрейф),
// стерео-пара ladder-фильтров, две огибающие, мод-матрица и глайд.
class Voice
{
public:
    static constexpr int kMaxUnison = 4;

    void prepare (double sampleRate);

    // glideFromPitch < 0: без глайда, высота сразу встаёт на ноту
    void start (int midiNote, float noteVelocity, float glideFromPitch, bool retriggerEnvelopes);
    void glideTo (int midiNote) noexcept;
    void release() noexcept;
    void kill() noexcept;

    bool isActive() const noexcept    { return active; }
    bool isReleasing() const noexcept { return releasing; }
    int getNote() const noexcept      { return note; }
    float getCurrentPitch() const noexcept { return currentPitch; }

    void render (float* left, float* right, int startSample, int numSamples,
                 const SynthParams& params, const ModulationBus& bus, const Tolerances& tolerances) noexcept;

private:
    struct Layer
    {
        PolyBlepOscillator osc1, osc2;
        SmoothNoise drift1, drift2, jitter1, jitter2;
        float ratio1 = 1.0f, ratio2 = 1.0f;
    };

    std::array<Layer, kMaxUnison> layers;
    LadderFilter filterLeft, filterRight;
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

    float smoothedCutoff = 1000.0f, cutoffSmoothingCoef = 1.0f;
    bool snapCutoff = true;
    float cutoffDriftValue = 0.0f;
    int controlCounter = 0;
};

} // namespace sonder
