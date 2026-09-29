#pragma once

#include "dsp/LfoShapes.h"

#include <juce_gui_basics/juce_gui_basics.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// Редактор формы LFO в духе Serum:
//  - перетаскивание точек (Shift - привязка к сетке);
//  - двойной клик по пустому месту добавляет точку, по точке - удаляет;
//  - перетаскивание ручки в середине сегмента меняет изгиб, двойной клик по ней - выпрямляет;
//  - правый клик - меню со стандартными формами.
// Если выбрана стандартная форма, первое же касание превращает её в редактируемую (Custom).
class LfoEditor final : public juce::Component, private juce::Timer
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

private:
    enum class Target { none, point, curve };

    struct Hit
    {
        Target target = Target::none;
        int index = -1;
    };

    void timerCallback() override { repaint(); }

    LfoShape currentShape() const;
    bool isCustom() const { return currentShape() == LfoShape::custom; }
    void makeCustom();
    void setShapeParameter (LfoShape shape);
    std::vector<LfoPoint> displayPoints() const;
    float displayValue (float phase) const;

    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> fromScreen (juce::Point<float> position) const;
    Hit hitTest (juce::Point<float> position) const;

    void showMenu();

    SonderAudioProcessor& processor;
    int lfo = 0;

    Hit hover, drag;
    float dragStartCurve = 0.0f;
    float dragStartY = 0.0f;
};

} // namespace sonder::ui
