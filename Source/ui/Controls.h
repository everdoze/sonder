#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace sonder::ui
{

using Apvts = juce::AudioProcessorValueTreeState;

// Ручка или выпадающий список, привязанный к параметру.
// standard: подпись сверху, ручка, значение снизу; inline: ручка слева, подпись и значение справа.
class ParameterControl final : public juce::Component
{
public:
    enum class Style { standard, inline_ };

    ParameterControl (Apvts& state, const juce::String& parameterID, const juce::String& labelText,
                      Style style = Style::standard);

    void setTooltipText (const juce::String& text);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    Style style;
    juce::Label label, valueLabel;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ComboBox> comboBox;
    std::unique_ptr<Apvts::SliderAttachment> sliderAttachment;
    std::unique_ptr<Apvts::ComboBoxAttachment> comboBoxAttachment;
};

// Строка мод-матрицы: источник -> цель, глубина
class ModSlotView final : public juce::Component
{
public:
    ModSlotView (Apvts& state, int slot);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void updateActiveState();

    int slot;
    juce::ComboBox source, destination;
    juce::Slider amount;
    std::unique_ptr<Apvts::ComboBoxAttachment> sourceAttachment, destinationAttachment;
    std::unique_ptr<Apvts::SliderAttachment> amountAttachment;
};

// Форма ADSR по текущим значениям ручек
class EnvelopeView final : public juce::Component, private juce::Timer
{
public:
    EnvelopeView (Apvts& state, const char* attackID, const char* decayID, const char* sustainID, const char* releaseID);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::array<juce::RangedAudioParameter*, 4> parameters {};
    std::array<float, 4> lastValues { -1.0f, -1.0f, -1.0f, -1.0f };
};

// Форма LFO и бегущая точка текущей фазы
class LfoView final : public juce::Component, private juce::Timer
{
public:
    LfoView (Apvts& state, const char* shapeID, const std::atomic<float>& phase);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    static float shapeValue (int shape, float phase);

    std::atomic<float>* shape = nullptr;
    const std::atomic<float>& phase;
};

// 16 индикаторов голосов, как светодиоды на голосовых платах
class VoiceLeds final : public juce::Component, private juce::Timer
{
public:
    explicit VoiceLeds (const std::atomic<uint32_t>& mask);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    const std::atomic<uint32_t>& mask;
    std::array<float, 16> brightness {};
};

} // namespace sonder::ui
