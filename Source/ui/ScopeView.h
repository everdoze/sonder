#pragma once

#include "ScopeBuffer.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace sonder::ui
{

// Осциллограф выхода. Окно подстраивается под период последней ноты,
// синхронизация по переходу через ноль вверх, поэтому форма стоит на месте.
class ScopeView final : public juce::Component, private juce::Timer
{
public:
    explicit ScopeView (const ScopeBuffer& buffer);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }

    const ScopeBuffer& scope;
    std::vector<float> samples;
    float displayGain = 1.0f;
};

} // namespace sonder::ui
