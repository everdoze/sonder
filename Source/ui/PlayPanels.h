#pragma once

#include "Controls.h"
#include "ShaderStage.h"
#include "Visuals.h"

class SonderAudioProcessor;

namespace sonder::ui
{

// Экран арпеджиатора: ноты шаблона столбиками по высоте, текущий шаг подсвечен
class ArpView final : public juce::Component, private juce::Timer
{
public:
    explicit ArpView (SonderAudioProcessor& processor);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void paintScreen (juce::Graphics&, bool forShader);

    SonderAudioProcessor& processor;
    std::vector<int> pattern;
    int step = -1;
    bool on = false;

    ShaderScreen shader;
    ScreenGlass glass;
    int lastLook = -1;
};

// Полоса значения (как глубина в матрице); двойной клик - ввод числа с клавиатуры
class ValueBar final : public juce::Slider
{
public:
    ValueBar() : juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox)
    {
        setSliderSnapsToMousePosition (false);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void finishEditing (bool apply);

    std::unique_ptr<juce::TextEditor> editor;
};

// Экран строя: один период от тоники. Яркие риски сверху - ступени строя (их можно таскать мышью),
// тонкие снизу - клавиши равномерного строя; над ступенями - отклонение в центах.
class TuningView final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit TuningView (SonderAudioProcessor& processor);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void paintScreen (juce::Graphics&, bool forShader);
    juce::Rectangle<float> plotArea() const;
    std::vector<double> markerCents() const;
    double periodCents() const;
    float xForCents (double cents) const;
    int markerAt (float x) const;

    SonderAudioProcessor& processor;
    int version = -1;
    int hovered = -1, dragged = -1;
    double dragStartCents = 0.0;
    float dragStartX = 0.0f;
    bool needsRender = false;

    ShaderScreen shader;
    ScreenGlass glass;
    int lastLook = -1;
};

// Панель строя: экран, двенадцать ступеней в центах, готовые строи, тоника, частота ля, период,
// загрузка .scl / .tun и сохранение .scl
class TuningPanel final : public juce::Component, private juce::Timer
{
public:
    explicit TuningPanel (SonderAudioProcessor& processor);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void showPresets();
    void chooseFile();
    void saveFile();
    static juce::File tuningFolder();

    SonderAudioProcessor& processor;
    TuningView view;
    std::array<ValueBar, 12> cells;
    juce::TextButton presetsButton { "PRESETS" }, loadButton { "LOAD" }, saveButton { "SAVE .SCL" };
    juce::ComboBox rootBox;
    ValueBar referenceBar, periodBar;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> cellsArea, infoArea;
    int version = -1;
    bool updating = false;
};

// Панель выразительности: MPE и что куда приходит
class ExpressionPanel final : public juce::Component
{
public:
    explicit ExpressionPanel (SonderAudioProcessor& processor);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    ParameterControl bendRange;
};

} // namespace sonder::ui
