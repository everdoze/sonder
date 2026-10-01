#pragma once

#include "Controls.h"
#include "FilterView.h"
#include "Parameters.h"

class SonderAudioProcessor;

namespace sonder::ui
{

// Панель фильтров: два фильтра на вкладках, у каждого кнопка включения и свои ручки,
// кнопка соединения (друг за другом или рядом) и общий экран АЧХ.
// Точка на вкладке - фильтр включён.
class FilterPanel final : public juce::Component, private juce::Timer
{
public:
    FilterPanel (SonderAudioProcessor& processor, Visuals& visuals);

    // Перед сменой пресета: ручки выбранного фильтра плавно доедут до новых значений
    void beginMorph();

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

private:
    // Режим фильтра: список, сгруппированный по типам (в параметре режимы идут в порядке добавления)
    class ModeControl final : public juce::Component
    {
    public:
        ModeControl (SonderAudioProcessor& processor, int filter);
        void resized() override;

    private:
        juce::Label label;
        juce::ComboBox box;
        std::unique_ptr<GroupedChoiceAttachment> attachment;
    };

    void timerCallback() override;
    void selectFilter (int index);
    bool isOn (int filter) const;
    std::array<ParameterControl*, 7> knobs() const;

    SonderAudioProcessor& processor;
    std::array<juce::TextButton, kNumFilters> tabs;
    juce::TextButton onButton { "ON" }, routingButton;
    std::unique_ptr<Apvts::ButtonAttachment> onAttachment;
    std::unique_ptr<juce::ParameterAttachment> routingAttachment;
    FilterView view;
    std::unique_ptr<ModeControl> modeControl;
    std::unique_ptr<ParameterControl> cutoffControl, resonanceControl, driveControl, vowelControl,
                                      envControl, keyTrackControl, velocityControl;
    int selected = 0;
    void repaintTabLeds();
    uint32_t ledMask = ~0u; // какие огоньки на вкладках горят: перерисовываем вкладки, только когда это меняется
};

} // namespace sonder::ui
