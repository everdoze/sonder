#pragma once

#include "Controls.h"
#include "Parameters.h"
#include "LfoEditor.h"

class SonderAudioProcessor;

namespace sonder::ui
{

// Панель LFO: вкладки 1..8 (кнопкой "+" открываются новые), редактор формы и ручки выбранного LFO.
// Точка на вкладке - LFO задействован в мод-матрице.
class LfoPanel final : public juce::Component, private juce::Timer
{
public:
    explicit LfoPanel (SonderAudioProcessor& processor);

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void timerCallback() override;
    void selectLfo (int index);
    int visibleCount() const;
    bool isLfoUsed (int index) const;

    // Вкладка, которую можно утащить на ручку как источник модуляции
    class DraggableTab final : public juce::TextButton
    {
    public:
        int lfoIndex = 0;
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.getDistanceFromDragStart() > 6)
                ModSourceHandle::startModulationDrag (*this, sourceForLfo (lfoIndex));
            else
                juce::TextButton::mouseDrag (e);
        }
    };

    SonderAudioProcessor& processor;
    std::array<DraggableTab, kNumLfos> tabs;
    juce::TextButton addButton { "+" };
    ModSourceHandle dragHandle;
    LfoEditor editor;
    std::unique_ptr<ParameterControl> shapeControl, modeControl, rateControl, syncControl;
    int selected = 0;
    int shownCount = 0;
};

} // namespace sonder::ui
