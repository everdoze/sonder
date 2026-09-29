#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// АЧХ фильтра: по положению ручек, а пока звучит нота - с живой модуляцией среза и гласной
class FilterView final : public juce::Component, private juce::Timer
{
public:
    explicit FilterView (SonderAudioProcessor& processor);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }

    SonderAudioProcessor& processor;
};

} // namespace sonder::ui
