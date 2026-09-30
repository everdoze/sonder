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
    // Содержимое окна создаётся заново при смене темы: так все компоненты получают новые цвета
    void rebuildView();

    SonderAudioProcessor& synth;
    sonder::ui::SonderLookAndFeel lookAndFeel;
    std::unique_ptr<sonder::ui::MainView> view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SonderAudioProcessorEditor)
};
