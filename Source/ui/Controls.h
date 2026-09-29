#pragma once

#include "synth/SynthParams.h"

#include <juce_audio_processors/juce_audio_processors.h>

class SonderAudioProcessor;

namespace sonder::ui
{

using Apvts = juce::AudioProcessorValueTreeState;

// Ручка или выпадающий список, привязанный к параметру.
// standard: подпись сверху, ручка, значение снизу; inline: ручка слева, подпись и значение справа.
// Ручки, которые можно модулировать, принимают перетаскивание источника (LFO, огибающей),
// показывают кольцо диапазона модуляции с живой точкой; Alt+drag меняет глубину, правый клик - меню.
class ParameterControl final : public juce::Component,
                               public juce::DragAndDropTarget,
                               private juce::Timer
{
public:
    enum class Style { standard, inline_ };

    ParameterControl (SonderAudioProcessor& processor, const juce::String& parameterID, const juce::String& labelText,
                      Style style = Style::standard);
    ~ParameterControl() override;

    void setTooltipText (const juce::String& text);

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    class Knob;
    friend class Knob;

    bool handleMouseDown (const juce::MouseEvent&);
    bool handleMouseDrag (const juce::MouseEvent&);
    bool handleMouseUp (const juce::MouseEvent&);

    void timerCallback() override;
    std::vector<int> modulationSlots() const;
    int pickActiveSlot();
    void addModulation (int sourceIndex);
    void showModulationMenu();
    void setParameterValue (const juce::String& id, float value);
    juce::String describeSlot (int slot) const;

    SonderAudioProcessor& processor;
    const juce::String parameterID;
    juce::RangedAudioParameter* parameter = nullptr;
    ModDest destination = ModDest::off;
    Style style;

    juce::Label label, valueLabel;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ComboBox> comboBox;
    std::unique_ptr<Apvts::SliderAttachment> sliderAttachment;
    std::unique_ptr<Apvts::ComboBoxAttachment> comboBoxAttachment;

    int activeSlot = -1;
    bool draggingAmount = false;
    float dragStartAmount = 0.0f, dragStartY = 0.0f;
    juce::String amountOverlay;
};

// Значок-"ручка" источника модуляции: тащится на любую модулируемую ручку
class ModSourceHandle final : public juce::Component, public juce::SettableTooltipClient
{
public:
    // Источник определяется в момент перетаскивания (например, выбранный LFO)
    explicit ModSourceHandle (std::function<ModSource()> sourceProvider);

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseDrag (const juce::MouseEvent&) override;

    static void startModulationDrag (juce::Component& source, ModSource modSource);

private:
    std::function<ModSource()> sourceProvider;
};

// Привязка ComboBox к choice-параметру через item ID = индекс + 1:
// в отличие от стандартной, пункты можно переставлять и группировать заголовками
class GroupedChoiceAttachment
{
public:
    GroupedChoiceAttachment (juce::RangedAudioParameter& parameter, juce::ComboBox& box, std::function<void()> onChange = {});

private:
    juce::ComboBox& comboBox;
    std::function<void()> callback;
    juce::ParameterAttachment attachment;
};

// Строка мод-матрицы: источник -> цель, глубина, кнопка очистки
class ModSlotView final : public juce::Component
{
public:
    ModSlotView (Apvts& state, int slot);

    bool isInUse() const;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void updateActiveState();
    void clear();

    Apvts& state;
    int slot;
    juce::ComboBox source, destination;
    juce::Slider amount;
    juce::TextButton clearButton { juce::String::fromUTF8 ("\xc3\x97") };
    std::unique_ptr<GroupedChoiceAttachment> sourceAttachment, destinationAttachment;
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
