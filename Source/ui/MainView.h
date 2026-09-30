#pragma once

#include "Controls.h"
#include "FilterPanel.h"
#include "FxRackView.h"
#include "LfoPanel.h"
#include "OscPanel.h"
#include "PlayPanels.h"
#include "PresetBar.h"
#include "ScopeView.h"
#include "ShaderStage.h"
#include "Visuals.h"

#include <juce_audio_utils/juce_audio_utils.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// Всё содержимое окна в фиксированных "дизайн-координатах"; редактор масштабирует его целиком.
// Четыре страницы: SYNTH (звук), MOD (мод-матрица), FX (рэк эффектов), PLAY (арпеджиатор, MPE, строй).
class MainView final : public juce::Component,
                       public juce::DragAndDropContainer,
                       private juce::Timer
{
public:
    static constexpr int kWidth = 1400;

    MainView (SonderAudioProcessor& processor, bool showKeyboard);
    ~MainView() override;

    int getDesignHeight() const noexcept;

    // Тема или цвет акцента поменялись: палитра уже обновлена, окно должно пересоздать MainView,
    // потому что часть компонентов запоминает цвета при создании
    std::function<void()> onThemeChanged;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    enum Page { synthPage, modPage, fxPage, playPage, numPages };

    struct Panel
    {
        juce::String title;
        Page page = synthPage;
        juce::Rectangle<int> bounds;
        std::vector<juce::Component*> cells; // nullptr - пустая ячейка
        int columns = 1;
        juce::Component* display = nullptr;  // экран над ручками
        int displayHeight = 0;
    };

    // Кнопка страницы. Пока тащат источник модуляции, наведение на неё открывает страницу:
    // так LFO со страницы SYNTH дотягивается до ручек эффектов на странице FX
    class PageButton final : public juce::TextButton,
                             public juce::DragAndDropTarget,
                             private juce::Timer
    {
    public:
        bool isInterestedInDragSource (const SourceDetails&) override;
        void itemDragEnter (const SourceDetails&) override { startTimer (350); }
        void itemDragExit (const SourceDetails&) override  { stopTimer(); }
        void itemDropped (const SourceDetails&) override   { stopTimer(); }

    private:
        void timerCallback() override;
    };

    // Кнопка меню вида: тема, цвет акцента, визуальные эффекты
    class ViewButton final : public juce::Button
    {
    public:
        ViewButton() : juce::Button ("View") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    ParameterControl* control (const juce::String& id, const char* label);
    Panel& addPanel (Page page, const juce::String& title, juce::Rectangle<int> bounds, int columns,
                     std::initializer_list<std::pair<juce::String, const char*>> items);
    void layoutPanel (Panel& panel);
    void showPage (Page page);
    void showViewMenu();
    void updateSourceChips();
    void timerCallback() override;

    SonderAudioProcessor& processor;
    const bool showKeyboard;
    Page currentPage = synthPage;

    // Раньше экранов: они читают отсюда цвет, спектры и накал
    Visuals visuals;

    // Шейдеры экранов: здесь только чтобы показать в меню, доступны ли они
    juce::SharedResourcePointer<ShaderStage> shaderStage;

    std::vector<std::unique_ptr<ParameterControl>> controls;
    std::map<juce::String, ParameterControl*> controlsById;
    std::vector<Panel> panels;

    std::array<PageButton, numPages> pageButtons;
    OscPanel oscPanel;
    FilterPanel filterPanel;
    ScopeView scopeView;
    FxRackView fxRackView;
    EnvelopeView filterEnvelopeView, ampEnvelopeView;
    ModSourceHandle filterEnvelopeHandle, ampEnvelopeHandle;
    LfoPanel lfoPanel;
    std::vector<std::unique_ptr<ModSlotView>> modSlots;

    // Источники модуляции на странице FX (там нет панелей LFO и огибающих)
    std::vector<std::unique_ptr<ModSourceChip>> fxSourceChips;
    int shownLfoChips = -1;

    // Страница PLAY
    ArpView arpView;
    ExpressionPanel expressionPanel;
    TuningPanel tuningPanel;
    juce::TextButton arpOnButton { "ON" }, arpHoldButton { "HOLD" }, mpeButton { "MPE" };
    std::unique_ptr<Apvts::ButtonAttachment> arpOnAttachment, arpHoldAttachment, mpeAttachment;

    // Дисторшн голоса до или после фильтра: кнопка в заголовке панели
    juce::TextButton distPositionButton;
    std::unique_ptr<juce::ParameterAttachment> distPositionAttachment;
    PresetBar presetBar;
    ViewButton viewButton;
    VoiceLeds voiceLeds;
    ParameterControl masterControl;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 600 };

    int coldStep = 0; // затемнение окна, пока синт "холодный"
};

} // namespace sonder::ui
