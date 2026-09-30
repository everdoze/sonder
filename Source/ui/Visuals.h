#pragma once

#include "ShaderStage.h"

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// Общее "настроение" картинки, которое экраны читают при отрисовке: яркость тембра и громкость
// (от них зависят цвет и свечение волны), спектры до и после фильтра, накал экранов при прогреве
// и просадке питания. Считается на message thread по кольцевым буферам процессора.
class Visuals final : private juce::Timer
{
public:
    static constexpr int kNumBands = 96;
    static constexpr float kMinFrequency = 20.0f, kMaxFrequency = 20000.0f;
    static constexpr float kSilenceDb = -120.0f;

    explicit Visuals (SonderAudioProcessor& processor);

    // -1 тёмный тембр .. +1 яркий; 0 в тишине и при выключенной настройке
    float getTimbre() const noexcept   { return timbre; }
    float getLevel() const noexcept    { return level; }   // громкость выхода, 0..1
    float getPower() const noexcept    { return power; }   // накал экранов, 1 - полный
    float getCold() const noexcept     { return cold; }    // 1 - синт только что "включили"

    // Акцентные цвета, сдвинутые по яркости тембра
    juce::Colour trace() const;
    juce::Colour traceBright() const;

    // Спектры по полосам в дБ, от kMinFrequency до kMaxFrequency в логарифмической шкале
    using Spectrum = std::array<float, kNumBands>;
    const Spectrum& getPreFilterSpectrum() const noexcept  { return preSpectrum; }
    const Spectrum& getPostFilterSpectrum() const noexcept { return postSpectrum; }
    static float bandFrequency (float band) noexcept;

    // Пересчитывает спектры до и после фильтра. Вызывает экран фильтра прямо перед своим кадром:
    // если бы спектр обновлялся своим таймером, два несинхронных таймера давали бы рваное движение
    void updateFilterSpectra();

    // Время для медленной анимации, секунды
    static double now() noexcept { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

    // Настройки шейдеров для экрана по текущему состоянию: свечение растёт с громкостью,
    // на холодном синте больше зерна. persistence - послесвечение этого экрана (0 - нет).
    ScreenFx screenFx (float persistence = 0.0f) const;

    // Для экранов, которые не знают про звук (эффекты, огибающие, LFO): свечение постоянное.
    // Экраны, которые таскают мышью, искривляются слабее, чтобы точка оставалась под курсором.
    static ScreenFx staticScreenFx (bool interactive);

    // Нужно ли экрану рисовать через шейдеры: настройка включена и экран сейчас на виду
    static bool wantsShaders (const juce::Component& screen);

private:
    static constexpr int kFftOrder = 12, kFftSize = 1 << kFftOrder;

    void timerCallback() override;
    void transform (std::vector<float>& samples);
    void updateSpectrum (Spectrum& spectrum, float sampleRate, float seconds);

    SonderAudioProcessor& processor;
    juce::dsp::FFT fft { kFftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kFftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> work;

    Spectrum preSpectrum, postSpectrum;
    double lastSpectrumTime = 0.0;
    float timbre = 0.0f, level = 0.0f, power = 1.0f, cold = 0.0f;
};

// "Стекло" экрана поверх картинки: строки развёртки и затемнение к краям (если включено в настройках).
// Рисунок не меняется от кадра к кадру, поэтому хранится готовой картинкой.
class ScreenGlass
{
public:
    void draw (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerSize = 6.0f);

private:
    static void render (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerSize);

    juce::Image cache;
    juce::Rectangle<float> cachedBounds;
    float cachedScale = 0.0f;
};

} // namespace sonder::ui
