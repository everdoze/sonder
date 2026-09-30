#pragma once

#include "Controls.h"
#include "EffectView.h"

#include <juce_audio_processors/juce_audio_processors.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// Ручка или список для одного параметра слота рэка.
// Ручка - обычная ParameterControl: на неё так же перетаскиваются LFO и огибающие.
class FxParamControl final : public juce::Component
{
public:
    FxParamControl (SonderAudioProcessor& processor, int slot, int param, const FxParamInfo& info);

    void setTooltipText (const juce::String& text);
    void resized() override;

private:
    juce::Label label;
    std::unique_ptr<ParameterControl> knob;
    std::unique_ptr<juce::ComboBox> comboBox;
    std::unique_ptr<juce::ParameterAttachment> comboAttachment;
};

// Полоса одного эффекта: слева номер в цепочке, название, включение, перестановка и удаление,
// дальше экран эффекта и его ручки
class FxStrip final : public juce::Component
{
public:
    static constexpr int kHeight = 106;

    FxStrip (SonderAudioProcessor& processor, int slot, FxType type);

    int getSlot() const noexcept    { return slot; }
    FxType getType() const noexcept { return type; }

    // Место полосы в цепочке и длина цепочки: от них зависят номер и доступность стрелок
    void setPosition (int newPosition, int chainLength);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void updateBypassLook();

    SonderAudioProcessor& processor;
    const int slot;
    const FxType type;
    int position = 0;
    bool shownOn = true;

    DragGrip grip;
    juce::TextButton onButton { "ON" };
    juce::ArrowButton upButton, downButton;
    juce::TextButton removeButton { juce::String::fromUTF8 ("\xc3\x97") };
    std::unique_ptr<Apvts::ButtonAttachment> onAttachment;
    EffectView display;
    std::vector<std::unique_ptr<FxParamControl>> controls;
};

// Рэк эффектов: полосы сверху вниз в порядке прохождения сигнала.
// Эффекты добавляются кнопкой "+ ADD EFFECT" (один и тот же вид можно поставить несколько раз),
// переставляются перетаскиванием за грип или стрелками, убираются крестиком.
class FxRackView final : public juce::Component,
                         public juce::DragAndDropTarget,
                         private juce::Timer
{
public:
    explicit FxRackView (SonderAudioProcessor& processor);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    void timerCallback() override;
    void rebuild();
    void layoutStrips();
    void showAddMenu();
    int dropPositionAt (juce::Point<int> position) const;

    SonderAudioProcessor& processor;
    juce::TextButton addButton { "+ ADD EFFECT" };
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<FxStrip>> strips;
    int rackVersion = -1;
    int dropPosition = -1;
};

} // namespace sonder::ui
