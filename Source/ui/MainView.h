#pragma once

#include "Controls.h"
#include "FilterView.h"
#include "LfoPanel.h"
#include "OscillatorView.h"
#include "PresetBar.h"
#include "ScopeView.h"

#include <juce_audio_utils/juce_audio_utils.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// Всё содержимое окна в фиксированных "дизайн-координатах"; редактор масштабирует его целиком.
// Три страницы: SYNTH (звук), MOD (мод-матрица), FX (эффекты).
class MainView final : public juce::Component, public juce::DragAndDropContainer
{
public:
    static constexpr int kWidth = 1400;

    MainView (SonderAudioProcessor& processor, bool showKeyboard);

    int getDesignHeight() const noexcept;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum Page { synthPage, modPage, fxPage, numPages };

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

    ParameterControl* control (const char* id, const char* label);
    Panel& addPanel (Page page, const juce::String& title, juce::Rectangle<int> bounds, int columns,
                     std::initializer_list<std::pair<const char*, const char*>> items);
    void layoutPanel (Panel& panel);
    void showPage (Page page);

    SonderAudioProcessor& processor;
    const bool showKeyboard;
    Page currentPage = synthPage;

    std::vector<std::unique_ptr<ParameterControl>> controls;
    std::map<juce::String, ParameterControl*> controlsById;
    std::vector<Panel> panels;

    std::array<juce::TextButton, numPages> pageButtons;
    OscillatorView osc1View, osc2View;
    FilterView filterView;
    ScopeView scopeView;
    EnvelopeView filterEnvelopeView, ampEnvelopeView;
    ModSourceHandle filterEnvelopeHandle, ampEnvelopeHandle;
    LfoPanel lfoPanel;
    std::vector<std::unique_ptr<ModSlotView>> modSlots;
    PresetBar presetBar;
    VoiceLeds voiceLeds;
    ParameterControl masterControl;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 600 };
};

} // namespace sonder::ui
