#include "PluginEditor.h"
#include "ui/Theme.h"

SonderAudioProcessorEditor::SonderAudioProcessorEditor (SonderAudioProcessor& p)
    : AudioProcessorEditor (&p),
      synth (p)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    setLookAndFeel (&lookAndFeel);
    rebuildView();

    const int designWidth = sonder::ui::MainView::kWidth;
    const int designHeight = view->getDesignHeight();

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
    view.reset();
    setLookAndFeel (nullptr);
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void SonderAudioProcessorEditor::rebuildView()
{
    view.reset();

    sonder::ui::applyTheme (sonder::ui::Settings::get());
    lookAndFeel.applyPalette();

    view = std::make_unique<sonder::ui::MainView> (synth, synth.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
    view->onThemeChanged = [this]
    {
        // Меню, из которого пришла команда, ещё на стеке: пересоздаём окно следующим сообщением
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<SonderAudioProcessorEditor> (this)]
        {
            if (safeThis != nullptr)
                safeThis->rebuildView();
        });
    };

    addAndMakeVisible (*view);

    if (getWidth() > 0)
        resized();

    repaint();
}

void SonderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (sonder::ui::Palette::background);
}

void SonderAudioProcessorEditor::resized()
{
    if (view == nullptr)
        return;

    const float scale = (float) getWidth() / (float) sonder::ui::MainView::kWidth;
    view->setBounds (0, 0, sonder::ui::MainView::kWidth, view->getDesignHeight());
    view->setTransform (juce::AffineTransform::scale (scale));
}
