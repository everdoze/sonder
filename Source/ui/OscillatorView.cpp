#include "OscillatorView.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "Theme.h"
#include "dsp/Saturation.h"
#include "synth/ModRouting.h"

namespace sonder::ui
{

namespace
{
    constexpr int kWavetableShape = 4;

    juce::String shapeParameter (int oscillator)    { return ParamIDs::oscShape (oscillator); }
    juce::String positionParameter (int oscillator) { return ParamIDs::oscWtPos (oscillator); }
}

OscillatorView::OscillatorView (SonderAudioProcessor& p, const Visuals& v, int osc)
    : processor (p), visuals (v), oscillator (osc)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Click to choose a wavetable or load your own WAV");
    startTimerHz (30);
}

void OscillatorView::setOscillator (int newOscillator)
{
    if (oscillator == newOscillator)
        return;

    oscillator = newOscillator;
    lastShape = lastVersion = -1;
    lastPosition = lastLivePosition = lastPulseWidth = -1.0f;
    positionTrail.fill (0.0f);
    timerCallback();
    repaint();
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
    const float pulseWidth = processor.parameters.getRawParameterValue (ParamIDs::oscPw (oscillator))->load();
    const bool on = processor.parameters.getRawParameterValue (ParamIDs::oscOn (oscillator))->load() > 0.5f;

    // Живая позиция: база плюс модуляция звучащего голоса (LFO, огибающие)
    const auto dest = wtPosDestForOsc (oscillator);
    const float livePosition = processor.displayVoiceActive.load() && on
                                 ? juce::jlimit (0.0f, 1.0f, position + processor.displayModulation[(size_t) dest].load())
                                 : position;

    // Шлейф: позиции прошлых кадров
    for (int i = kTrailLength - 1; i > 0; --i)
        positionTrail[(size_t) i] = positionTrail[(size_t) i - 1];
    positionTrail[0] = livePosition;

    tilt += (tiltTarget - tilt) * 0.2f;

    // Цвет (тембр и накал экрана) меняется плавно: перерисовываем, когда он заметно сдвинулся
    const auto colour = visuals.traceBright().withMultipliedAlpha (visuals.getPower()).getARGB() & 0xfcfcfcfcu;

    // Стопка кадров в движении перерисовывается каждый кадр
    const bool moving = shape == kWavetableShape && Settings::get().motion && isVisible() && on;
    const bool shaders = shader.isAvailable() && Visuals::wantsShaders (*this);

    if (moving || shaders != lastShaders || on != lastOn || shape != lastShape || version != lastVersion || position != lastPosition
        || livePosition != lastLivePosition || pulseWidth != lastPulseWidth || colour != lastColour)
    {
        lastShape = shape;
        lastVersion = version;
        lastPosition = position;
        lastLivePosition = livePosition;
        lastPulseWidth = pulseWidth;
        lastColour = colour;
        lastOn = on;
        lastShaders = shaders;

        if (shaders)
            shader.render (*this, visuals.screenFx(), [this] (juce::Graphics& g) { paintScreen (g, true); });
        else
            shader.invalidate();

        repaint();
    }
    else if (shader.flush())
    {
        // Результат шейдеров приходит кадром позже: без этого после смены формы на экране
        // оставалась бы прошлая картинка, пока что-нибудь ещё не поменяется
        repaint();
    }
}

void OscillatorView::mouseMove (const juce::MouseEvent& e)
{
    const auto bounds = getLocalBounds().toFloat();
    tiltTarget = { juce::jlimit (-1.0f, 1.0f, (e.position.x - bounds.getCentreX()) / (bounds.getWidth() * 0.5f)),
                   juce::jlimit (-1.0f, 1.0f, (e.position.y - bounds.getCentreY()) / (bounds.getHeight() * 0.5f)) };
}

void OscillatorView::mouseExit (const juce::MouseEvent&)
{
    tiltTarget = {};
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
    if (! shader.draw (g, getLocalBounds().toFloat()))
        paintScreen (g, false);

    paintLabels (g);
}

void OscillatorView::paintScreen (juce::Graphics& g, bool forShader)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Palette::deep);
    g.fillRoundedRectangle (bounds, 6.0f);

    const int shape = currentShape();
    const auto area = bounds.reduced (10.0f, 10.0f);

    if (shape == kWavetableShape)
        paintWavetable (g, area);
    else
        paintBasicShape (g, area, shape);

    // Выключенный осциллятор: картинка приглушена
    if (! lastOn)
    {
        g.setColour (Palette::deep.withAlpha (0.72f));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (Palette::textFaint);
        g.setFont (makeFont (13.0f, true, 0.3f));
        g.drawText ("OFF", bounds, juce::Justification::centred);
    }

    if (! forShader)
        glass.draw (g, bounds);
}

void OscillatorView::paintLabels (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const int shape = currentShape();
    const auto caption = shape == kWavetableShape ? processor.wavetables.getDisplayName (oscillator)
                                                  : Choices::oscShapes()[shape];

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

    // Глубина стопки: медленно покачивается сама и поворачивается вслед за мышью
    float swayX = 0.0f, swayY = 0.0f;
    if (Settings::get().motion)
    {
        const double t = Visuals::now() + (double) oscillator * 3.1;
        swayX = 0.10f * (float) std::sin (t * 0.55) + 0.30f * tilt.x;
        swayY = 0.07f * (float) std::sin (t * 0.37 + 1.0) - 0.22f * tilt.y;
    }

    const float depthX = area.getWidth() * 0.22f * (1.0f + swayX);
    const float depthY = area.getHeight() * 0.38f * (1.0f + swayY);
    const float frameWidth = area.getWidth() * 0.74f;
    const float amplitude = area.getHeight() * 0.26f;
    const float originBase = area.getX() + (area.getWidth() - frameWidth - depthX) * 0.5f; // стопка по центру экрана

    const float power = visuals.getPower();
    const auto colour = visuals.trace();
    const auto bright = visuals.traceBright();
    const float position = lastLivePosition < 0.0f ? 0.0f : lastLivePosition;
    const int currentFrame = juce::roundToInt (position * (float) (numFrames - 1));
    const int baseFrame = juce::roundToInt (juce::jmax (0.0f, lastPosition) * (float) (numFrames - 1));

    const auto framePath = [&] (int frame, float depth)
    {
        const float* data = table->getFrame (frame);
        const float originX = originBase + depth * depthX;
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
        g.setColour (Palette::accent.withAlpha ((0.08f + 0.22f * (1.0f - depth)) * power));
        g.strokePath (framePath (frame, depth), juce::PathStrokeType (1.0f));
    }

    // Положение ручки (если модуляция его сдвинула - тонкой линией)
    if (baseFrame != currentFrame)
    {
        const float baseDepth = numFrames > 1 ? (float) baseFrame / (float) (numFrames - 1) : 0.0f;
        g.setColour (Palette::accent.withAlpha (0.6f * power));
        g.strokePath (framePath (baseFrame, baseDepth), juce::PathStrokeType (1.0f));
    }

    // Шлейф: кадры, через которые позиция только что прошла
    if (Settings::get().motion)
    {
        int previousFrame = currentFrame;

        for (int k = 1; k < kTrailLength; ++k)
        {
            const int frame = juce::roundToInt (juce::jlimit (0.0f, 1.0f, positionTrail[(size_t) k]) * (float) (numFrames - 1));
            if (frame == previousFrame)
                continue;

            previousFrame = frame;
            const float depth = numFrames > 1 ? (float) frame / (float) (numFrames - 1) : 0.0f;
            g.setColour (colour.withAlpha (0.32f * power * (1.0f - (float) k / (float) kTrailLength)));
            g.strokePath (framePath (frame, depth), juce::PathStrokeType (1.0f));
        }
    }

    // Текущая (живая) позиция
    const float currentDepth = numFrames > 1 ? (float) currentFrame / (float) (numFrames - 1) : 0.0f;
    const auto current = framePath (currentFrame, currentDepth);
    g.setColour (colour.withAlpha (0.25f * power));
    g.strokePath (current, { 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    g.setColour (bright.withAlpha (0.35f + 0.65f * power));
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

    const float power = visuals.getPower();
    g.setColour (visuals.trace().withAlpha (0.2f * power));
    g.strokePath (path, { 5.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded });
    g.setColour (visuals.traceBright().withAlpha (0.35f + 0.65f * power));
    g.strokePath (path, { 1.8f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded });
}

} // namespace sonder::ui
