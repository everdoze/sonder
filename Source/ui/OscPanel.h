#pragma once

#include "Controls.h"
#include "OscillatorView.h"
#include "Parameters.h"

class SonderAudioProcessor;

namespace sonder::ui
{

// Панель осцилляторов: четыре слота на вкладках, у каждого кнопка включения, экран и ручки.
// Точка на вкладке - осциллятор включён. Уровни всех четырёх продублированы в микшере.
class OscPanel final : public juce::Component, private juce::Timer
{
public:
    OscPanel (SonderAudioProcessor& processor, const Visuals& visuals);

    // Перед сменой пресета: ручки выбранного осциллятора плавно доедут до новых значений
    void beginMorph();

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

private:
    void timerCallback() override;
    void selectOsc (int index);
    bool isOn (int osc) const;
    std::array<ParameterControl*, 6> controls() const;

    SonderAudioProcessor& processor;
    std::array<juce::TextButton, kNumOscs> tabs;
    juce::TextButton onButton { "ON" }, syncButton { "SYNC" };
    std::unique_ptr<Apvts::ButtonAttachment> onAttachment, syncAttachment;
    OscillatorView view;
    std::unique_ptr<ParameterControl> shapeControl, positionControl, widthControl, semiControl, fineControl, levelControl;
    int selected = 0;
    void repaintTabLeds();
    uint32_t ledMask = ~0u; // какие огоньки на вкладках горят: перерисовываем вкладки, только когда это меняется
};

} // namespace sonder::ui
