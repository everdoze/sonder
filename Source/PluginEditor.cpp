#include "PluginEditor.h"

SonderAudioProcessorEditor::SonderAudioProcessorEditor (SonderAudioProcessor& p)
    : AudioProcessorEditor (&p),
      view (p, p.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (view);

    const int designWidth = sonder::ui::MainView::kWidth;
    const int designHeight = view.getDesignHeight();

    // Стартовый масштаб: не больше экрана
    double scale = 1.0;
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        scale = juce::jmin (1.0, (display->userBounds.getHeight() - 120.0) / designHeight,
                                 (display->userBounds.getWidth() - 40.0) / designWidth);

    setResizable (true, true);
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / designHeight);
    setSize (juce::roundToInt (designWidth * scale), juce::roundToInt (designHeight * scale));
}

SonderAudioProcessorEditor::~SonderAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void SonderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (sonder::ui::Palette::background);
}

void SonderAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) sonder::ui::MainView::kWidth;
    view.setBounds (0, 0, sonder::ui::MainView::kWidth, view.getDesignHeight());
    view.setTransform (juce::AffineTransform::scale (scale));
}
