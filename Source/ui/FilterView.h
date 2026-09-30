#pragma once

#include "Visuals.h"

#include <juce_gui_basics/juce_gui_basics.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// АЧХ фильтров: по положению ручек, а пока звучит нота - с живой модуляцией среза и гласной.
// Главная кривая - то, что слышно (оба фильтра вместе, последовательно или параллельно);
// если включены оба, каждый ещё и отдельной тонкой линией, выбранный ярче.
// За кривой - спектр звука: тусклый контур до фильтров и заливка после них, видно, что они вырезают.
class FilterView final : public juce::Component, private juce::Timer
{
public:
    FilterView (SonderAudioProcessor& processor, Visuals& visuals);

    // Фильтр, который сейчас редактируется: его срез отмечен и подписан
    void setSelectedFilter (int filter);

    void paint (juce::Graphics&) override;

private:
    struct FilterState
    {
        bool on = false, live = false;
        int mode = 0;
        float cutoff = 1000.0f, resonance = 0.0f, vowel = 0.0f;
    };

    void timerCallback() override;
    FilterState readFilter (int filter) const;

    // Экран без подписи; forShader - картинка уйдёт в шейдеры, стекло нарисуют они
    void paintScreen (juce::Graphics&, bool forShader);
    void paintLabels (juce::Graphics&);

    void paintSpectrum (juce::Graphics&, juce::Rectangle<float> plot);

    SonderAudioProcessor& processor;
    Visuals& visuals;
    int selected = 0;
    double lastSignature = -1.0; // кадр перерисовывается, только когда на экране что-то меняется
    ScreenGlass glass;
    float spectrumOffset = 40.0f;
    ShaderScreen shader;
};

} // namespace sonder::ui
