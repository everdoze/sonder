#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// Экран осциллятора. Для wavetable - объёмная "стопка" кадров с подсвеченной текущей позицией,
// для обычных форм - сама волна. Клик открывает выбор wavetable и загрузку своих WAV.
class OscillatorView final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    OscillatorView (SonderAudioProcessor& processor, int oscillator);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void showMenu();
    void setShapeToWavetable();
    int currentShape() const;

    void paintWavetable (juce::Graphics&, juce::Rectangle<float> area);
    void paintBasicShape (juce::Graphics&, juce::Rectangle<float> area, int shape);

    SonderAudioProcessor& processor;
    const int oscillator;
    std::unique_ptr<juce::FileChooser> chooser;

    int lastShape = -1, lastVersion = -1;
    float lastPosition = -1.0f, lastLivePosition = -1.0f, lastPulseWidth = -1.0f;
};

} // namespace sonder::ui
