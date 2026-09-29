#pragma once

#include "presets/PresetManager.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace sonder::ui
{

class PresetBar final : public juce::Component, private juce::ChangeListener
{
public:
    explicit PresetBar (PresetManager& manager);
    ~PresetBar() override;

    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { updateName(); }
    void updateName();
    void showPresetMenu();
    void showSaveDialog();

    PresetManager& presets;
    juce::TextButton previousButton { "<" }, nextButton { ">" }, nameButton, saveButton { "SAVE" };
};

} // namespace sonder::ui
