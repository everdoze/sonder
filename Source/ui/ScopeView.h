#pragma once

#include "ScopeBuffer.h"
#include "Visuals.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace sonder::ui
{

// Осциллограф выхода, три режима (переключаются кликом по названию в углу экрана):
//  - WAVE: окно подстраивается под период ноты и синхронизируется по переходу через ноль вверх,
//    поэтому форма стоит на месте;
//  - ROLL: волна бежит справа налево на отрезке от 50 мс до 2 с, видны атака, огибающие и LFO;
//  - XY: вектороскоп (фигуры Лиссажу) - ширина и фаза стерео.
// Цвет следа зависит от яркости тембра, яркость свечения от громкости; след оставляет послесвечение,
// от громкой волны разлетаются искры. Если доступны шейдеры, экран проходит через них (см. ShaderStage).
class ScopeView final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    enum class Mode { wave, roll, xy };

    ScopeView (const ScopeBuffer& buffer, const Visuals& visuals);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;

    // Экран без подписей; forShader - картинка уйдёт в шейдеры, послесвечение и стекло нарисуют они
    void paintScreen (juce::Graphics&, bool forShader);
    void paintLabels (juce::Graphics&);
    void updateSparks (juce::Rectangle<float> area);

    Mode mode() const;
    void setMode (Mode newMode);
    juce::Rectangle<float> tabBounds (int index) const;

    // Строят след в координатах экрана; возвращают false, если рисовать нечего
    bool buildWave (juce::Rectangle<float> plot, juce::Path& trace, juce::Path& fill);
    bool buildRoll (juce::Rectangle<float> plot, juce::Path& trace, juce::Path& fill);
    bool buildVector (juce::Rectangle<float> plot, juce::Path& trace);
    void followGain (float peak, float target, float speed);

    void drawGrid (juce::Graphics&, juce::Rectangle<float> plot) const;

    const ScopeBuffer& scope;
    const Visuals& visuals;
    ScreenGlass glass;

    std::vector<float> samples, samplesRight;
    float displayGain = 1.0f;
    bool signalPresent = false;

    // Режим WAVE: первый период прошлого кадра - следующий кадр выравнивается по нему
    static constexpr int kShapePoints = 64;
    std::array<float, kShapePoints> previousShape {};
    float previousNominal = 0.0f;
    bool havePreviousShape = false;
    float cachedPeriod = 0.0f, periodNominal = 0.0f;
    int periodCountdown = 0;

    // Послесвечение: прошлые кадры гаснут в отдельной картинке (программной - её гасит процессор);
    // окно рисует её родную копию, иначе Direct2D каждый кадр создавал бы картинку заново
    juce::Image trail, trailShown;
    Mode trailMode = Mode::wave;

    ShaderScreen shader;

    // Искры: рождаются на волне, разлетаются и гаснут
    struct Spark
    {
        juce::Point<float> position, velocity;
        float age = 0.0f, lifetime = 1.0f;
    };

    std::vector<Spark> sparks;
    std::vector<juce::Point<float>> sparkSources; // точки следа, из которых может вылететь искра
    juce::Random random;
    double lastSparkTime = 0.0;
    float lastLevel = 0.0f, sparkDebt = 0.0f;
};

} // namespace sonder::ui
