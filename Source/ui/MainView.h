#pragma once

#include "Controls.h"
#include "PresetBar.h"
#include "ScopeView.h"

#include <juce_audio_utils/juce_audio_utils.h>

class SonderAudioProcessor;

namespace sonder::ui
{

// Всё содержимое окна в фиксированных "дизайн-координатах"; редактор масштабирует его целиком
class MainView final : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth = 1240;

    MainView (SonderAudioProcessor& processor, bool showKeyboard);

    int getDesignHeight() const noexcept;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Panel
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        std::vector<juce::Component*> cells; // nullptr - пустая ячейка
        int columns = 1;
        juce::Component* display = nullptr;  // график над ручками
        int displayHeight = 0;
    };

    void timerCallback() override;

    ParameterControl* control (const char* id, const char* label);
    Panel& addPanel (const juce::String& title, juce::Rectangle<int> bounds, int columns,
                     std::initializer_list<std::pair<const char*, const char*>> items);
    void layoutPanel (Panel& panel);

    SonderAudioProcessor& processor;
    const bool showKeyboard;

    std::vector<std::unique_ptr<ParameterControl>> controls;
    std::map<juce::String, ParameterControl*> controlsById;
    std::vector<Panel> panels;

    ScopeView scopeView;
    EnvelopeView filterEnvelopeView, ampEnvelopeView;
    LfoView lfo1View, lfo2View;
    std::vector<std::unique_ptr<ModSlotView>> modSlots;
    PresetBar presetBar;
    VoiceLeds voiceLeds;
    ParameterControl masterControl;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 600 };
};

} // namespace sonder::ui
