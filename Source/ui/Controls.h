#pragma once

#include "Visuals.h"
#include "dsp/LfoShapes.h"
#include "fx/FxRack.h"
#include "synth/SynthParams.h"

#include <juce_audio_processors/juce_audio_processors.h>

class SonderAudioProcessor;

namespace sonder::ui
{

using Apvts = juce::AudioProcessorValueTreeState;

// Связь источника с целью в мод-матрице: если такая уже есть - её слот, иначе первый свободный
// (глубина 25%, в обе стороны). -1 - свободных слотов нет.
int addModulationSlot (Apvts& state, ModSource source, ModDest dest);

// Очистить слот матрицы (все четыре его параметра - к значениям по умолчанию)
void clearModulationSlot (Apvts& state, int slot);

// Ручка или выпадающий список, привязанный к параметру.
// standard: подпись сверху, ручка, значение снизу; inline: ручка слева, подпись и значение справа.
// Ручки, которые можно модулировать, принимают перетаскивание источника (LFO, огибающей),
// показывают кольцо диапазона модуляции с живой точкой; Alt+drag меняет глубину, правый клик - меню
// (там же направление: в обе стороны, только вверх, только вниз).
// При смене пресета ручка не прыгает, а плавно доезжает до нового значения (beginMorph).
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

    // Для ручек рэка: дуга от середины и значение по двойному клику задаются снаружи
    void setBipolar (bool bipolar);
    void setDefaultValue (double value);
    void setLabelScale (float minimumHorizontalScale);

    // Вызывается прямо перед тем, как значения параметров поменяются: ручка запоминает,
    // где стояла, и рисует плавный переход к новому положению
    void beginMorph();

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
    void updateModulationRing();
    void updateMorph();
    void endMorph();
    std::vector<int> modulationSlots() const;
    int pickActiveSlot();
    void addModulation (int sourceIndex);
    void showModulationMenu();
    void setParameterValue (const juce::String& id, float value);
    std::pair<float, float> slotSpan (int slot) const;
    juce::String describeSlot (int slot) const;
    void showOverlay (int slot, int holdMilliseconds);

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
    bool draggingAmount = false, dragMoved = false;
    float dragStartAmount = 0.0f, dragStartY = 0.0f;
    juce::String amountOverlay, overlayCaption;
    juce::uint32 overlayExpiry = 0;

    bool morphing = false;
    float morphFrom = 0.0f;
    juce::uint32 morphStart = 0;
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

    // Источник из описания перетаскивания или -1, если тащат не источник модуляции
    static int sourceFromDrag (const juce::var& description);

private:
    std::function<ModSource()> sourceProvider;
};

// Подписанная "таблетка"-источник: её тоже можно утащить на ручку. Такие стоят на странице FX,
// где нет панелей LFO и огибающих.
class ModSourceChip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModSourceChip (ModSource source, const juce::String& text);

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseDrag (const juce::MouseEvent&) override;

private:
    ModSource source;
    juce::String text;
};

// Картинка-"пилюля" с подписью, которая летит за курсором при перетаскивании
juce::ScaledImage makeDragImage (const juce::String& text);

// Грип для перетаскивания чего угодно: что именно тащится, решает onDragStart
class DragGrip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    DragGrip();

    std::function<void (DragGrip&)> onDragStart;

    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    void mouseDrag (const juce::MouseEvent&) override;
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

// Кнопка направления модуляции: стрелки в обе стороны, вверх или вниз. Клик - следующее направление.
class PolarityButton final : public juce::Button
{
public:
    PolarityButton() : juce::Button ("Direction") {}

    void setPolarity (ModPolarity newPolarity) { polarity = newPolarity; repaint(); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    // Стрелки направления в прямоугольнике (для кнопки и подписей на ручках)
    static void drawArrows (juce::Graphics&, juce::Rectangle<float> area, ModPolarity polarity, juce::Colour colour);

private:
    ModPolarity polarity = ModPolarity::both;
};

// Строка мод-матрицы: источник -> цель, направление, глубина, кнопка очистки.
// Цели-ручки рэка подписаны по эффекту, который сейчас стоит в слоте.
class ModSlotView final : public juce::Component, private juce::Timer
{
public:
    ModSlotView (Apvts& state, const FxRack& rack, const LfoShapeBank& lfoShapes, int slot);

    bool isInUse() const;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    void updateActiveState();
    void fillDestinationList();
    void clear();

    juce::String targetsSignature() const;

    Apvts& state;
    const FxRack& rack;
    const LfoShapeBank& lfoShapes;
    int slot;
    juce::String signature;
    juce::ComboBox source, destination;
    PolarityButton polarityButton;
    juce::Slider amount;
    juce::TextButton clearButton { juce::String::fromUTF8 ("\xc3\x97") };
    std::unique_ptr<GroupedChoiceAttachment> sourceAttachment, destinationAttachment;
    std::unique_ptr<juce::ParameterAttachment> polarityAttachment;
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
    void paintScreen (juce::Graphics&, bool forShader);

    std::array<juce::RangedAudioParameter*, 4> parameters {};
    std::array<float, 4> lastValues { -1.0f, -1.0f, -1.0f, -1.0f };

    ShaderScreen shader;
    ScreenGlass glass;
    int lastLook = -1; // шейдеры и CRT: при смене экран перерисовывается
};

// 16 индикаторов голосов, как светодиоды на голосовых платах.
// Яркость идёт за огибающей голоса, цвет - за высотой ноты: низкие глубже и синее, высокие светлее.
class VoiceLeds final : public juce::Component, private juce::Timer
{
public:
    explicit VoiceLeds (SonderAudioProcessor& processor);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    SonderAudioProcessor& processor;
    std::array<float, 16> brightness {}, pitch {};
};

} // namespace sonder::ui
