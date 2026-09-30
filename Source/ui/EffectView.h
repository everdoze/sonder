#pragma once

#include "Visuals.h"
#include "fx/FxTypes.h"

#include <juce_gui_basics/juce_gui_basics.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// Экран эффекта в слоте рэка. Показывает, что эффект делает со звуком:
//  - Distortion: кривая искажения и диапазон, в котором сейчас находится сигнал;
//  - Phaser, Flanger: АЧХ с провалами, которые едут вместе с LFO;
//  - Chorus: модуляция задержки левого и правого каналов;
//  - Delay: повторы на шкале времени, затухающие по Feedback;
//  - Compressor: передаточная кривая, точка входного уровня и индикатор подавления;
//  - Reverb: предзадержка и затухающий хвост;
//  - EQ: АЧХ; точки полос можно таскать мышью (частота и усиление), колесо меняет добротность;
//  - Filter: АЧХ на текущем (качаемом LFO) срезе;
//  - Tremolo: форма LFO и текущая точка на ней;
//  - Stereo: ширина стерео-картины и корреляция каналов;
//  - Limiter: уровни входа и выхода, подавление;
//  - Multiband: уровень каждой полосы и сколько её подняли или прижали;
//  - Freq Shift: сдвиг на оси частот и вращающийся фазор.
// Выключенный слот рисуется тускло.
class EffectView final : public juce::Component, private juce::Timer
{
public:
    EffectView (SonderAudioProcessor& processor, int slot);

    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;

    // Экран без рамки; forShader - картинка уйдёт в шейдеры, стекло нарисуют они
    void paintScreen (juce::Graphics&, bool forShader);
    void render (bool immediate);

    // Перерисовать сразу (после действия мышью): с шейдерами кадр считается тут же, а не по таймеру
    void refresh();

    FxType type() const;
    bool isOn() const;
    float value (int param) const;
    std::array<float, kNumFxParams> values() const;
    juce::Rectangle<float> plotArea() const;

    void paintDistortion (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintPhaser (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintFlanger (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintChorus (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintDelay (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintCompressor (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintReverb (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintEqualizer (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintFilter (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintTremolo (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintWidener (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintLimiter (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintMultiband (juce::Graphics&, juce::Rectangle<float> plot, bool active);
    void paintShifter (juce::Graphics&, juce::Rectangle<float> plot, bool active);

    // Точки полос эквалайзера
    struct EqBand
    {
        int frequencyParam, gainParam, qParam; // qParam = -1 у полок
    };

    static const std::array<EqBand, 4>& eqBands();
    juce::Point<float> eqPoint (int band, juce::Rectangle<float> plot) const;
    int eqBandAt (juce::Point<float> position) const;
    void setEqGesture (int band, bool begin);

    SonderAudioProcessor& processor;
    const int slot;
    ScreenGlass glass;
    ShaderScreen shader;
    int hoverBand = -1, dragBand = -1;
};

} // namespace sonder::ui
