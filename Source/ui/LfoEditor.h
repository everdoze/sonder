#pragma once

#include "Visuals.h"
#include "dsp/LfoShapes.h"
#include "synth/SynthParams.h"

#include <juce_gui_basics/juce_gui_basics.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// Редактор формы LFO в духе Serum:
//  - перетаскивание точек (Shift - привязка к сетке);
//  - двойной клик по пустому месту добавляет точку, по точке - удаляет;
//  - перетаскивание ручки в середине сегмента меняет изгиб, двойной клик по ней - выпрямляет;
//  - правый клик - меню со стандартными формами, по модулированной точке - меню её модуляций.
// Если выбрана стандартная форма, первое же касание превращает её в редактируемую (Custom).
//
// Модуляция точек: источник (другой LFO, огибающая...) бросается на точку - он двигает её высоту,
// с Alt - положение по времени. Диапазон модуляции виден на точке, кривая показывает живую форму звучащего голоса.
// Alt+drag по модулированной точке меняет глубину, как на ручках.
class LfoEditor final : public juce::Component,
                        public juce::DragAndDropTarget,
                        private juce::Timer
{
public:
    explicit LfoEditor (SonderAudioProcessor& processor);

    void setLfo (int index);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    enum class Target { none, point, curve };

    struct Hit
    {
        Target target = Target::none;
        int index = -1;
    };

    void timerCallback() override;

    // Экран без подписи; forShader - картинка уйдёт в шейдеры, стекло нарисуют они
    void paintScreen (juce::Graphics&, bool forShader);
    void render (bool immediate);

    // Перерисовать сразу (после действия мышью): с шейдерами кадр считается тут же, а не по таймеру
    void refresh();

    LfoShape currentShape() const;
    bool isCustom() const { return currentShape() == LfoShape::custom; }
    void makeCustom();
    void setShapeParameter (LfoShape shape);
    std::vector<LfoPoint> displayPoints() const;

    // Точки с живой модуляцией звучащего голоса (если модуляции нет - как есть)
    std::vector<LfoPoint> livePoints() const;
    float displayValue (float phase) const;

    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> fromScreen (juce::Point<float> position) const;
    Hit hitTest (juce::Point<float> position) const;
    int pointNear (juce::Point<float> position) const;

    // Модуляции точек этого LFO
    std::vector<int> pointSlots (int point) const;
    bool hasPointModulation() const;
    std::pair<float, float> pointSpan (int point, bool position) const;

    // Точки добавили, удалили или переставили: связи матрицы переезжают вслед за ними (-1 - связь удалить)
    void remapPointModulation (const std::function<int (int)>& newIndex);

    void showMenu();
    void showPointMenu (int point);

    SonderAudioProcessor& processor;
    int lfo = 0;

    Hit hover, drag;
    float dragStartCurve = 0.0f;
    float dragStartY = 0.0f;

    // Alt+drag по модулированной точке: глубина её модуляции
    int amountSlot = -1;
    float amountStart = 0.0f;

    int dropPoint = -1; // точка под перетаскиваемым источником

    ShaderScreen shader;
    ScreenGlass glass;
};

} // namespace sonder::ui
