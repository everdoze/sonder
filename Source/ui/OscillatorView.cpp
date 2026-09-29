#include "OscillatorView.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "dsp/Saturation.h"
#include "synth/ModRouting.h"

namespace sonder::ui
{

namespace
{
    constexpr int kWavetableShape = 4;

    const char* shapeParameter (int oscillator)    { return oscillator == 0 ? ParamIDs::osc1Shape : ParamIDs::osc2Shape; }
    const char* positionParameter (int oscillator) { return oscillator == 0 ? ParamIDs::osc1WtPos : ParamIDs::osc2WtPos; }
}

OscillatorView::OscillatorView (SonderAudioProcessor& p, int osc)
    : processor (p), oscillator (osc)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Click to choose a wavetable or load your own WAV");
    startTimerHz (30);
}

int OscillatorView::currentShape() const
{
    return (int) processor.parameters.getRawParameterValue (shapeParameter (oscillator))->load();
}

void OscillatorView::timerCallback()
{
    const int shape = currentShape();
    const int version = processor.wavetables.getVersion();
    const float position = processor.parameters.getRawParameterValue (positionParameter (oscillator))->load();
    const float pulseWidth = processor.parameters.getRawParameterValue (ParamIDs::pulseWidth)->load();

    // Живая позиция: база плюс модуляция звучащего голоса (LFO, огибающие)
    const auto dest = oscillator == 0 ? ModDest::osc1WtPos : ModDest::osc2WtPos;
    const float livePosition = processor.displayVoiceActive.load()
                                 ? juce::jlimit (0.0f, 1.0f, position + processor.displayModulation[(size_t) dest].load())
                                 : position;

    if (shape != lastShape || version != lastVersion || position != lastPosition
        || livePosition != lastLivePosition || pulseWidth != lastPulseWidth)
    {
        lastShape = shape;
        lastVersion = version;
        lastPosition = position;
        lastLivePosition = livePosition;
        lastPulseWidth = pulseWidth;
        repaint();
    }
}

void OscillatorView::setShapeToWavetable()
{
    if (auto* parameter = processor.parameters.getParameter (shapeParameter (oscillator)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) kWavetableShape));
        parameter->endChangeGesture();
    }
}

void OscillatorView::mouseDown (const juce::MouseEvent&)
{
    showMenu();
}

void OscillatorView::showMenu()
{
    juce::PopupMenu menu;
    const auto selectedId = processor.wavetables.getSelectedId (oscillator);
    const auto& builtIns = Wavetables::builtInNames();
    const auto userFiles = WavetableBank::getUserFiles();

    menu.addSectionHeader ("WAVETABLES");
    for (int i = 0; i < builtIns.size(); ++i)
        menu.addItem (1 + i, builtIns[i], true, selectedId == "builtin:" + builtIns[i]);

    if (! userFiles.isEmpty())
    {
        menu.addSectionHeader ("USER");
        for (int i = 0; i < userFiles.size(); ++i)
        {
            const auto name = userFiles[i].getFileNameWithoutExtension();
            menu.addItem (1000 + i, name, true, selectedId == "user:" + name);
        }
    }

    constexpr int loadId = 5000, folderId = 5001;
    menu.addSeparator();
    menu.addItem (loadId, "Load wavetable...");
    menu.addItem (folderId, "Open wavetables folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safeThis = juce::Component::SafePointer<OscillatorView> (this), builtIns, userFiles] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;

        auto& view = *safeThis;
        auto& bank = view.processor.wavetables;

        if (result == loadId)
        {
            view.chooser = std::make_unique<juce::FileChooser> ("Load wavetable", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
            view.chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                       [safeView = juce::Component::SafePointer<OscillatorView> (&view)] (const juce::FileChooser& fc)
            {
                if (safeView == nullptr || fc.getResult() == juce::File())
                    return;

                if (safeView->processor.wavetables.importFile (safeView->oscillator, fc.getResult()))
                    safeView->setShapeToWavetable();
                else
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Wavetable",
                                                            "Could not read this file as a wavetable.");
            });
            return;
        }

        if (result == folderId)
        {
            const auto folder = WavetableBank::getUserDirectory();
            folder.createDirectory();
            folder.startAsProcess();
            return;
        }

        const bool ok = result >= 1000 ? bank.select (view.oscillator, "user:" + userFiles[result - 1000].getFileNameWithoutExtension())
                                       : bank.select (view.oscillator, "builtin:" + builtIns[result - 1]);
        if (ok)
            view.setShapeToWavetable();
    });
}

void OscillatorView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Palette::deep);
    g.fillRoundedRectangle (bounds, 6.0f);

    const int shape = currentShape();
    const auto area = bounds.reduced (10.0f, 10.0f);

    juce::String caption;
    if (shape == kWavetableShape)
    {
        paintWavetable (g, area);
        caption = processor.wavetables.getDisplayName (oscillator);
    }
    else
    {
        paintBasicShape (g, area, shape);
        caption = Choices::oscShapes()[shape];
    }

    g.setColour (Palette::textDim);
    g.setFont (makeFont (10.5f, true, 0.1f));
    g.drawText (caption.toUpperCase(), bounds.reduced (9.0f, 6.0f), juce::Justification::topLeft);

    // Шеврон: экран кликабельный
    const float x = bounds.getRight() - 14.0f, y = bounds.getY() + 12.0f;
    juce::Path arrow;
    arrow.startNewSubPath (x - 3.5f, y - 1.5f);
    arrow.lineTo (x, y + 2.0f);
    arrow.lineTo (x + 3.5f, y - 1.5f);
    g.setColour (Palette::accent);
    g.strokePath (arrow, { 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

void OscillatorView::paintWavetable (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto* table = processor.wavetables.get (oscillator);
    if (table == nullptr)
        return;

    // Кадры уходят "вглубь" по диагонали вверх-вправо
    const int numFrames = table->getNumFrames();
    const int shown = juce::jmin (numFrames, 20);
    const float depthX = area.getWidth() * 0.22f;
    const float depthY = area.getHeight() * 0.38f;
    const float frameWidth = area.getWidth() - depthX;
    const float amplitude = (area.getHeight() - depthY) * 0.42f;
    const float position = lastLivePosition < 0.0f ? 0.0f : lastLivePosition;
    const int currentFrame = juce::roundToInt (position * (float) (numFrames - 1));
    const int baseFrame = juce::roundToInt (juce::jmax (0.0f, lastPosition) * (float) (numFrames - 1));

    const auto framePath = [&] (int frame, float depth)
    {
        const float* data = table->getFrame (frame);
        const float originX = area.getX() + depth * depthX;
        const float centreY = area.getBottom() - amplitude - depth * depthY;

        juce::Path path;
        constexpr int points = 128;
        for (int i = 0; i <= points; ++i)
        {
            const int index = juce::jmin (Wavetable::kFrameSize - 1, i * Wavetable::kFrameSize / points);
            const juce::Point<float> p (originX + frameWidth * (float) i / points, centreY - data[index] * amplitude);
            if (i == 0)
                path.startNewSubPath (p);
            else
                path.lineTo (p);
        }
        return path;
    };

    // Сзади наперёд: дальние кадры тусклее
    for (int k = shown - 1; k >= 0; --k)
    {
        const float depth = shown > 1 ? (float) k / (float) (shown - 1) : 0.0f;
        const int frame = juce::roundToInt (depth * (float) (numFrames - 1));
        g.setColour (Palette::accent.withAlpha (0.08f + 0.22f * (1.0f - depth)));
        g.strokePath (framePath (frame, depth), juce::PathStrokeType (1.0f));
    }

    // Положение ручки (если модуляция его сдвинула - тонкой линией)
    if (baseFrame != currentFrame)
    {
        const float baseDepth = numFrames > 1 ? (float) baseFrame / (float) (numFrames - 1) : 0.0f;
        g.setColour (Palette::accent.withAlpha (0.6f));
        g.strokePath (framePath (baseFrame, baseDepth), juce::PathStrokeType (1.0f));
    }

    // Текущая (живая) позиция
    const float currentDepth = numFrames > 1 ? (float) currentFrame / (float) (numFrames - 1) : 0.0f;
    const auto current = framePath (currentFrame, currentDepth);
    g.setColour (Palette::accent.withAlpha (0.25f));
    g.strokePath (current, { 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    g.setColour (Palette::accentBright);
    g.strokePath (current, { 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    g.setColour (Palette::textFaint);
    g.setFont (makeFont (10.0f, true));
    g.drawText (juce::String (currentFrame + 1) + " / " + juce::String (numFrames),
                area.withTrimmedRight (6.0f), juce::Justification::bottomRight);
}

void OscillatorView::paintBasicShape (juce::Graphics& g, juce::Rectangle<float> area, int shape)
{
    const float pulseWidth = lastPulseWidth < 0.0f ? 0.5f : lastPulseWidth;
    const auto plot = area.withTrimmedTop (10.0f);

    g.setColour (Palette::outline);
    g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());

    // Два периода формы
    juce::Path path;
    constexpr int points = 200;
    for (int i = 0; i <= points; ++i)
    {
        const float t = 2.0f * (float) i / points;
        const float phase = t - std::floor (t);
        float value = 0.0f;

        switch (shape)
        {
            case 0: value = 2.0f * phase - 1.0f; break;
            case 1: value = phase < pulseWidth ? 1.0f : -1.0f; break;
            case 2: value = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
            case 3: value = fastSinCycles (phase); break;
            default: break;
        }

        const juce::Point<float> p (plot.getX() + plot.getWidth() * (float) i / points,
                                    plot.getCentreY() - value * plot.getHeight() * 0.42f);
        if (i == 0)
            path.startNewSubPath (p);
        else
            path.lineTo (p);
    }

    g.setColour (Palette::accent.withAlpha (0.2f));
    g.strokePath (path, { 5.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded });
    g.setColour (Palette::accentBright);
    g.strokePath (path, { 1.8f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded });
}

} // namespace sonder::ui
