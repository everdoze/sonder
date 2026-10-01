#pragma once

#include "Visuals.h"

#include <juce_gui_basics/juce_gui_basics.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// Экран осциллятора. Для wavetable - объёмная "стопка" кадров с подсвеченной текущей позицией,
// для обычных форм - сама волна. Клик открывает выбор wavetable и загрузку своих WAV.
// Стопка медленно покачивается и поворачивается вслед за мышью; когда позицию двигает модуляция,
// за текущим кадром тянется шлейф. Цвет волны идёт за яркостью тембра.
class OscillatorView final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    OscillatorView (SonderAudioProcessor& processor, const Visuals& visuals, int oscillator);

    // Какой из осцилляторов показывать (панель переключает его вкладками)
    void setOscillator (int newOscillator);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void showMenu();
    void setShapeToWavetable();
    int currentShape() const;

    // Экран без подписей; forShader - картинка уйдёт в шейдеры, стекло нарисуют они
    void paintScreen (juce::Graphics&, bool forShader);
    void paintLabels (juce::Graphics&);
    void paintWavetable (juce::Graphics&, juce::Rectangle<float> area);
    void paintBasicShape (juce::Graphics&, juce::Rectangle<float> area, int shape);

    SonderAudioProcessor& processor;
    const Visuals& visuals;
    ScreenGlass glass;
    int oscillator;
    std::unique_ptr<juce::FileChooser> chooser;
    bool lastOn = true;

    int lastShape = -1, lastVersion = -1;
    float lastPosition = -1.0f, lastLivePosition = -1.0f, lastPulseWidth = -1.0f;
    juce::uint32 lastColour = 0;
    ShaderScreen shader;
    bool lastShaders = false;

    // Поворот стопки вслед за мышью (-1..1 по обеим осям), сглаженный
    juce::Point<float> tiltTarget, tilt;

    // Недавние позиции в таблице: шлейф за текущим кадром
    static constexpr int kTrailLength = 6;
    std::array<float, kTrailLength> positionTrail {};

    // Само покачивание стопки медленное (пара пикселей в секунду): для него хватает каждого kSwayDivider-го кадра.
    // swayTime - время, по которому стопка покачивается; между такими кадрами оно стоит
    static constexpr int kSwayDivider = 4;
    int swayCounter = 0;
    double swayTime = 0.0;

    // Стопка кадров без текущей позиции и шлейфа кэшируется картинкой в разрешении экрана: она меняется
    // только с покачиванием, поворотом, таблицей и цветом, а позицию модуляция двигает каждый кадр
    struct StackKey
    {
        const void* table = nullptr;
        int numFrames = 0, width = 0, height = 0;
        float swayX = 0.0f, swayY = 0.0f, power = 0.0f;
        juce::uint32 colour = 0;

        bool operator== (const StackKey&) const = default;
    };

    juce::Image stackImage;
    StackKey stackKey;
};

} // namespace sonder::ui
