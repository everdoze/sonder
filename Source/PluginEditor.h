#pragma once

#include "PluginProcessor.h"
#include "ui/MainView.h"
#include "ui/SonderLookAndFeel.h"

// Окно плагина: масштабирует MainView целиком, сохраняя пропорции.
// Клавиатура на экране показывается только в standalone-версии.
class SonderAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit SonderAudioProcessorEditor (SonderAudioProcessor&);
    ~SonderAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    sonder::ui::SonderLookAndFeel lookAndFeel;
    sonder::ui::MainView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonderAudioProcessorEditor)
};
