#include "PlayPanels.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "Theme.h"

namespace sonder::ui
{

namespace
{
    constexpr int kTitleHeight = 26;

    void fillScreenBackground (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (Palette::deep);
        g.fillRoundedRectangle (bounds, 5.0f);
    }

    void drawScreenFrame (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (Palette::outline.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
    }

    // Экран с шейдерами: перерисовывается только при изменениях
    template <typename Paint>
    void renderScreen (juce::Component& owner, ShaderScreen& shader, Paint&& paint)
    {
        if (shader.isAvailable() && Visuals::wantsShaders (owner))
        {
            auto fx = Visuals::staticScreenFx (false);
            fx.cornerRadius = 5.0f;
            shader.render (owner, fx, std::forward<Paint> (paint), true);
        }
        else
        {
            shader.invalidate();
        }

        owner.repaint();
    }

    int currentLook (ShaderScreen& shader, juce::Component& owner)
    {
        const bool shaders = shader.isAvailable() && Visuals::wantsShaders (owner);
        return (shaders ? 2 : 0) + (Settings::get().crtScreen ? 1 : 0);
    }
}

//==============================================================================
ArpView::ArpView (SonderAudioProcessor& p)
    : processor (p)
{
    startTimerHz (30);
}

void ArpView::timerCallback()
{
    const auto& arp = processor.getArpeggiator();
    const bool isOn = processor.parameters.getRawParameterValue (ParamIDs::arpOn)->load() > 0.5f;

    std::vector<int> next ((size_t) arp.getPatternSize());
    for (size_t i = 0; i < next.size(); ++i)
        next[i] = arp.getPatternNote ((int) i);

    const int nextStep = arp.getCurrentStep();
    const int look = currentLook (shader, *this);

    if (next == pattern && nextStep == step && isOn == on && look == lastLook)
        return;

    pattern = std::move (next);
    step = nextStep;
    on = isOn;
    lastLook = look;
    renderScreen (*this, shader, [this] (juce::Graphics& g) { paintScreen (g, true); });
}

void ArpView::paint (juce::Graphics& g)
{
    if (! shader.draw (g, getLocalBounds().toFloat()))
        paintScreen (g, false);

    drawScreenFrame (g, getLocalBounds().toFloat().reduced (1.0f));

    // Подписи поверх шейдеров: резкие
    const auto plot = getLocalBounds().toFloat().reduced (12.0f, 10.0f);
    g.setFont (makeFont (13.0f));
    g.setColour (Palette::textDim);

    if (! on)
        g.drawText ("The arpeggiator is off. Switch it on with ON in the panel title", plot, juce::Justification::centred);
    else if (pattern.empty())
        g.drawText ("Hold a chord: the arpeggiator plays it as a pattern", plot, juce::Justification::centred);
    else if (juce::isPositiveAndBelow (step, (int) pattern.size()))
    {
        g.setColour (Palette::accentBright);
        g.setFont (makeFont (12.0f, true, 0.08f));
        g.drawText (juce::MidiMessage::getMidiNoteName (pattern[(size_t) step], true, true, 4), plot, juce::Justification::topRight);
    }
}

void ArpView::paintScreen (juce::Graphics& g, bool forShader)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    fillScreenBackground (g, bounds);

    if (on && ! pattern.empty())
    {
        const auto plot = bounds.reduced (14.0f, 16.0f);
        const auto [lowest, highest] = std::minmax_element (pattern.begin(), pattern.end());
        const float low = (float) *lowest - 2.0f, high = (float) *highest + 2.0f;
        const float width = plot.getWidth() / (float) pattern.size();

        for (size_t i = 0; i < pattern.size(); ++i)
        {
            const float level = ((float) pattern[i] - low) / juce::jmax (1.0f, high - low);
            auto bar = juce::Rectangle<float> (plot.getX() + (float) i * width, plot.getY(), width, plot.getHeight())
                           .reduced (juce::jmin (4.0f, width * 0.15f), 0.0f);
            const float y = plot.getBottom() - level * plot.getHeight();
            const bool current = (int) i == step;

            g.setColour (Palette::track);
            g.fillRoundedRectangle (bar, 2.0f);

            // Нота - яркая риска на своей высоте, под ней столбик
            g.setColour (Palette::accent.withAlpha (current ? 0.5f : 0.18f));
            g.fillRect (bar.withTop (y));
            g.setColour (current ? Palette::accentBright : Palette::accent);
            g.fillRoundedRectangle (bar.withY (y - 2.0f).withHeight (4.0f), 2.0f);

            if (current)
            {
                g.setColour (Palette::accent.withAlpha (0.25f));
                g.fillRoundedRectangle (bar.withY (y - 6.0f).withHeight (12.0f), 5.0f);
            }
        }
    }

    if (! forShader)
        glass.draw (g, bounds, 5.0f);
}

//==============================================================================
void ValueBar::mouseDoubleClick (const juce::MouseEvent&)
{
    // Точное значение вводится с клавиатуры
    editor = std::make_unique<juce::TextEditor>();
    editor->setFont (makeFont (12.0f));
    editor->setJustification (juce::Justification::centred);
    editor->setText (juce::String (getValue(), 2), false);
    editor->selectAll();
    editor->onReturnKey = [this] { finishEditing (true); };
    editor->onEscapeKey = [this] { finishEditing (false); };
    editor->onFocusLost = [this] { finishEditing (true); };
    addAndMakeVisible (*editor);
    editor->setBounds (getLocalBounds().reduced (2, 4));
    editor->grabKeyboardFocus();
}

void ValueBar::finishEditing (bool apply)
{
    if (editor == nullptr)
        return;

    if (apply)
        setValue (editor->getText().retainCharacters ("-+.0123456789").getDoubleValue(), juce::sendNotificationSync);

    // Редактор удаляется после выхода из его же обработчика
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<ValueBar> (this)]
    {
        if (safeThis != nullptr)
            safeThis->editor.reset();
    });
}

//==============================================================================
TuningView::TuningView (SonderAudioProcessor& p)
    : processor (p)
{
    setTooltip ("Drag a note left or right to retune it (Shift - finer). Double-click returns it to an equal step");
    startTimerHz (30);
}

juce::Rectangle<float> TuningView::plotArea() const
{
    return getLocalBounds().toFloat().reduced (18.0f, 0.0f).withTrimmedTop (26.0f).withTrimmedBottom (22.0f);
}

std::vector<double> TuningView::markerCents() const
{
    const auto& tuning = processor.tuning;
    std::vector<double> cents;

    if (tuning.isTable())
    {
        // Таблица: двенадцать клавиш от тоники на своих высотах
        const float root = tuning.pitchFor (tuning.getRootNote());
        for (int k = 0; k < 12; ++k)
            cents.push_back (100.0 * (tuning.pitchFor (tuning.getRootNote() + k) - root));
    }
    else
    {
        for (int d = 0; d < tuning.getNumDegrees(); ++d)
            cents.push_back (tuning.getDegree (d));
    }

    return cents;
}

double TuningView::periodCents() const
{
    return processor.tuning.isTable() ? 1200.0 : processor.tuning.getPeriod();
}

float TuningView::xForCents (double cents) const
{
    const auto plot = plotArea();
    return plot.getX() + (float) (cents / periodCents()) * plot.getWidth();
}

int TuningView::markerAt (float x) const
{
    if (processor.tuning.isTable())
        return -1;

    const auto cents = markerCents();
    int best = -1;
    float bestDistance = 9.0f;

    for (int d = 1; d < (int) cents.size(); ++d)
    {
        const float distance = std::abs (xForCents (cents[(size_t) d]) - x);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = d;
        }
    }

    return best;
}

void TuningView::mouseMove (const juce::MouseEvent& e)
{
    const int marker = markerAt (e.position.x);
    setMouseCursor (marker > 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);

    if (marker != hovered)
    {
        hovered = marker;
        needsRender = true;
    }
}

void TuningView::mouseExit (const juce::MouseEvent&)
{
    if (dragged < 0 && hovered >= 0)
    {
        hovered = -1;
        needsRender = true;
    }
}

void TuningView::mouseDown (const juce::MouseEvent& e)
{
    dragged = markerAt (e.position.x);
    if (dragged > 0)
    {
        dragStartCents = processor.tuning.getDegree (dragged);
        dragStartX = e.position.x;
    }
}

void TuningView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragged <= 0)
        return;

    const double perPixel = periodCents() / plotArea().getWidth() * (e.mods.isShiftDown() ? 0.1 : 1.0);
    processor.tuning.setDegree (dragged, dragStartCents + (e.position.x - dragStartX) * perPixel);
}

void TuningView::mouseUp (const juce::MouseEvent&)
{
    dragged = -1;
    needsRender = true;
}

void TuningView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int marker = markerAt (e.position.x);
    auto& tuning = processor.tuning;

    if (marker > 0)
        tuning.setDegree (marker, tuning.getPeriod() * marker / tuning.getNumDegrees());
}

void TuningView::timerCallback()
{
    const int look = currentLook (shader, *this);
    if (processor.tuning.getVersion() == version && look == lastLook && ! needsRender)
        return;

    version = processor.tuning.getVersion();
    lastLook = look;
    needsRender = false;
    renderScreen (*this, shader, [this] (juce::Graphics& g) { paintScreen (g, true); });
}

void TuningView::paint (juce::Graphics& g)
{
    if (! shader.draw (g, getLocalBounds().toFloat()))
        paintScreen (g, false);

    drawScreenFrame (g, getLocalBounds().toFloat().reduced (1.0f));

    const auto& tuning = processor.tuning;
    const auto plot = plotArea();
    const double period = periodCents();

    // Клавиши равномерного строя от тоники
    g.setFont (makeFont (10.0f, true));
    g.setColour (Palette::textFaint);
    for (int step = 0; step * 100.0 <= period + 0.01; ++step)
    {
        const float x = xForCents (step * 100.0);
        g.drawText (juce::MidiMessage::getMidiNoteName (tuning.getRootNote() + step, true, false, 4),
                    juce::Rectangle<float> (x - 14.0f, plot.getBottom() + 5.0f, 28.0f, 14.0f), juce::Justification::centred);
    }

    // Над каждой ступенью - отклонение от ближайшей клавиши равномерного строя, в центах
    const auto cents = markerCents();
    const float spacing = plot.getWidth() / (float) juce::jmax (1, (int) cents.size());
    g.setFont (makeFont (10.5f, true));

    for (size_t d = 0; d < cents.size(); ++d)
    {
        const bool active = (int) d == dragged || ((int) d == hovered && dragged < 0);
        if (spacing < 30.0f && ! active)
            continue;

        const double deviation = cents[d] - 100.0 * std::round (cents[d] / 100.0);
        const auto text = active ? juce::String (cents[d], 2) + " ct"
                                 : (std::abs (deviation) < 0.05 ? juce::String ("0") : (deviation > 0 ? "+" : "") + juce::String (deviation, 1));
        const float x = xForCents (cents[d]);
        const auto area = juce::Rectangle<float> (x - 40.0f, 6.0f, 80.0f, 16.0f);

        g.setColour (active ? Palette::accentBright : (std::abs (deviation) < 0.05 ? Palette::textFaint : Palette::textDim));
        g.drawText (text, area, juce::Justification::centred);
    }
}

void TuningView::paintScreen (juce::Graphics& g, bool forShader)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    fillScreenBackground (g, bounds);

    const auto plot = plotArea();
    const double period = periodCents();

    // Клавиши равномерного строя - тонкие риски снизу
    g.setColour (Palette::textFaint.withAlpha (0.7f));
    for (int step = 0; step * 100.0 <= period + 0.01; ++step)
        g.fillRect (juce::Rectangle<float> (xForCents (step * 100.0) - 0.5f, plot.getY() + plot.getHeight() * 0.6f,
                                            1.0f, plot.getHeight() * 0.4f));

    // Ступени строя - яркие риски сверху; та, что под мышью, толще
    const auto cents = markerCents();
    for (size_t d = 0; d <= cents.size(); ++d)
    {
        const bool periodMark = d == cents.size();
        const float x = xForCents (periodMark ? period : cents[d]);
        const bool active = (int) d == dragged || ((int) d == hovered && dragged < 0);
        const float width = active ? 4.0f : 2.0f;
        const float height = plot.getHeight() * (periodMark || d == 0 ? 0.62f : 0.58f);

        g.setColour (Palette::accent.withAlpha (active ? 0.45f : 0.22f));
        g.fillRect (juce::Rectangle<float> (x - width * 1.5f, plot.getY(), width * 3.0f, height));
        g.setColour (periodMark || d == 0 ? Palette::accent : Palette::accentBright);
        g.fillRect (juce::Rectangle<float> (x - width * 0.5f, plot.getY(), width, height));
    }

    if (! forShader)
        glass.draw (g, bounds, 5.0f);
}

//==============================================================================
TuningPanel::TuningPanel (SonderAudioProcessor& p)
    : processor (p), view (p)
{
    addAndMakeVisible (view);

    for (int d = 0; d < 12; ++d)
    {
        auto& cell = cells[(size_t) d];
        cell.setRange (-100.0, 100.0, 0.01);
        cell.getProperties().set ("bipolar", true);
        cell.textFromValueFunction = [] (double value)
        {
            return std::abs (value) < 0.005 ? juce::String ("0") : (value > 0.0 ? "+" : "") + juce::String (value, 1);
        };
        cell.setTooltip ("Offset of this note from equal temperament, in cents. Double-click to type a value");
        cell.onValueChange = [this, d]
        {
            if (! updating)
                processor.tuning.setDegree (d, 100.0 * d + cells[(size_t) d].getValue());
        };
        addAndMakeVisible (cell);
    }

    cells[0].setEnabled (false);

    presetsButton.setTooltip ("Ready-made tunings");
    presetsButton.onClick = [this] { showPresets(); };

    for (int pc = 0; pc < 12; ++pc)
        rootBox.addItem ("Root " + juce::MidiMessage::getMidiNoteName (pc, true, false, 4), pc + 1);
    rootBox.setTooltip ("The key the scale starts from. It keeps its equal-temperament pitch, the other keys follow the scale");
    rootBox.onChange = [this]
    {
        if (! updating)
            processor.tuning.setRootNote (60 + rootBox.getSelectedId() - 1);
    };

    referenceBar.setRange (380.0, 500.0, 0.1);
    referenceBar.textFromValueFunction = [] (double hz) { return "A4 = " + juce::String (hz, 1) + " Hz"; };
    referenceBar.setTooltip ("Concert pitch: moves the whole tuning. Double-click to type a value");
    referenceBar.onValueChange = [this]
    {
        if (! updating)
            processor.tuning.setReference (referenceBar.getValue());
    };

    periodBar.setRange (600.0, 2400.0, 0.01);
    periodBar.setSkewFactorFromMidPoint (1200.0);
    periodBar.textFromValueFunction = [] (double cents) { return "Period " + juce::String (cents, 2) + " ct"; };
    periodBar.setTooltip ("Size of the repeating interval, usually an octave (1200 cents). A little more than 1200 gives a "
                          "'stretched' piano-like tuning. Double-click to type a value");
    periodBar.onValueChange = [this]
    {
        if (! updating)
            processor.tuning.setPeriod (periodBar.getValue());
    };

    loadButton.setTooltip ("Load a Scala (.scl) or AnaMark (.tun) tuning file");
    loadButton.onClick = [this] { chooseFile(); };
    saveButton.setTooltip ("Save the scale as a Scala (.scl) file");
    saveButton.onClick = [this] { saveFile(); };

    for (auto* component : std::initializer_list<juce::Component*> { &presetsButton, &rootBox, &referenceBar, &periodBar, &loadButton, &saveButton })
        addAndMakeVisible (*component);

    timerCallback();
    startTimerHz (10);
}

void TuningPanel::timerCallback()
{
    const auto& tuning = processor.tuning;
    if (tuning.getVersion() == version)
        return;

    version = tuning.getVersion();
    const bool table = tuning.isTable();
    const bool twelve = ! table && tuning.getNumDegrees() == 12;

    updating = true;

    for (int d = 0; d < 12; ++d)
    {
        cells[(size_t) d].setVisible (twelve);
        if (twelve)
            cells[(size_t) d].setValue (tuning.getDegree (d) - 100.0 * d, juce::dontSendNotification);
    }

    rootBox.setSelectedId ((tuning.getRootNote() % 12) + 1, juce::dontSendNotification);
    referenceBar.setValue (tuning.getReference(), juce::dontSendNotification);
    periodBar.setValue (table ? 1200.0 : tuning.getPeriod(), juce::dontSendNotification);
    updating = false;

    rootBox.setEnabled (! table);
    periodBar.setEnabled (! table);
    saveButton.setEnabled (! table);
    repaint();
}

void TuningPanel::showPresets()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("TUNINGS");

    const auto& names = Tuning::getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (i + 1, names[i], true, processor.tuning.getName() == names[i]);

    menu.addSeparator();
    menu.addItem (1000, "Reset everything: 12-TET, root C, A4 = 440 Hz");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetsButton),
                        [safeThis = juce::Component::SafePointer<TuningPanel> (this)] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;

        if (result == 1000)
            safeThis->processor.tuning.reset();
        else
            safeThis->processor.tuning.loadPreset (result - 1);
    });
}

juce::File TuningPanel::tuningFolder()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Sonder").getChildFile ("Tunings");
    folder.createDirectory();
    return folder;
}

void TuningPanel::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Load a tuning", tuningFolder(), "*.scl;*.tun");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis = juce::Component::SafePointer<TuningPanel> (this)] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr || fc.getResult() == juce::File())
            return;

        juce::String error;
        if (! safeThis->processor.tuning.loadFile (fc.getResult(), error))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Tuning", error);
    });
}

void TuningPanel::saveFile()
{
    const auto fileName = juce::File::createLegalFileName (processor.tuning.getName().replace (" (edited)", "").trim());
    chooser = std::make_unique<juce::FileChooser> ("Save the scale", tuningFolder().getChildFile (fileName + ".scl"), "*.scl");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis = juce::Component::SafePointer<TuningPanel> (this)] (const juce::FileChooser& fc)
    {
        if (safeThis == nullptr || fc.getResult() == juce::File())
            return;

        if (! safeThis->processor.tuning.saveScala (fc.getResult().withFileExtension ("scl")))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Tuning", "The file cannot be written");
    });
}

void TuningPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 0).withTrimmedTop (kTitleHeight + 2).withTrimmedBottom (8);

    auto controls = area.removeFromBottom (26);
    area.removeFromBottom (6);
    infoArea = area.removeFromBottom (34);
    area.removeFromBottom (4);
    cellsArea = area.removeFromBottom (40);
    area.removeFromBottom (6);
    view.setBounds (area);

    const float cellWidth = (float) cellsArea.getWidth() / 12.0f;
    for (int d = 0; d < 12; ++d)
        cells[(size_t) d].setBounds (juce::Rectangle<float> ((float) cellsArea.getX() + (float) d * cellWidth, (float) cellsArea.getY() + 14.0f,
                                                             cellWidth, 26.0f).reduced (2.0f, 0.0f).toNearestInt());

    presetsButton.setBounds (controls.removeFromLeft (84));
    controls.removeFromLeft (6);
    rootBox.setBounds (controls.removeFromLeft (92));
    controls.removeFromLeft (6);
    saveButton.setBounds (controls.removeFromRight (84));
    controls.removeFromRight (6);
    loadButton.setBounds (controls.removeFromRight (60));
    controls.removeFromRight (6);

    const int half = (controls.getWidth() - 6) / 2;
    referenceBar.setBounds (controls.removeFromLeft (half));
    controls.removeFromLeft (6);
    periodBar.setBounds (controls);
}

void TuningPanel::paint (juce::Graphics& g)
{
    const auto& tuning = processor.tuning;

    // Подписи ячеек: клавиши от тоники
    if (cells[0].isVisible())
    {
        g.setFont (makeFont (10.0f, true));
        g.setColour (Palette::textFaint);
        for (int d = 0; d < 12; ++d)
            g.drawText (juce::MidiMessage::getMidiNoteName (tuning.getRootNote() + d, true, false, 4),
                        cells[(size_t) d].getBounds().withY (cellsArea.getY()).withHeight (14), juce::Justification::centred);
    }
    else
    {
        g.setFont (makeFont (12.0f));
        g.setColour (Palette::textDim);
        g.drawFittedText (tuning.isTable() ? "Per-key table from a .tun file: pick a ready tuning or load a .scl to edit it"
                                           : "Drag the notes on the screen to retune them",
                          cellsArea, juce::Justification::centred, 2);
    }

    g.setColour (Palette::text);
    g.setFont (makeFont (14.0f, true));
    g.drawText (tuning.getName(), infoArea.withHeight (18), juce::Justification::centredLeft);

    g.setColour (Palette::textDim);
    g.setFont (makeFont (12.0f));
    g.drawFittedText (tuning.getDescription(), infoArea.withTrimmedTop (20), juce::Justification::centredLeft, 1);
}

//==============================================================================
ExpressionPanel::ExpressionPanel (SonderAudioProcessor& p)
    : bendRange (p, ParamIDs::mpeBendRange, "MPE Bend")
{
    bendRange.setTooltipText ("Pitch bend range of each note in MPE mode (MPE controllers usually send 48 semitones)");
    addAndMakeVisible (bendRange);
}

void ExpressionPanel::resized()
{
    auto area = getLocalBounds().reduced (8, 0).withTrimmedTop (kTitleHeight + 6);
    bendRange.setBounds (area.removeFromLeft (80).withHeight (94));
}

void ExpressionPanel::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().reduced (8, 0).withTrimmedTop (kTitleHeight + 8).withTrimmedLeft (100).withTrimmedRight (8);

    const std::pair<const char*, const char*> lines[] {
        { "Aftertouch", "channel pressure plays on every note; polyphonic aftertouch and MPE pressure only on their own note. "
                        "Use the Aftertouch source in the matrix or drag it onto a knob" },
        { "Slide", "CC74 (the MPE 'third dimension'): a new modulation source, Slide" },
        { "MPE", "with MPE on, channel 1 is shared by all notes, channels 2-16 carry one note each: "
                 "its own pitch bend (MPE Bend range), pressure and slide" },
    };

    for (const auto& [title, text] : lines)
    {
        auto row = area.removeFromTop (58);
        g.setColour (Palette::accent);
        g.setFont (makeFont (12.0f, true, 0.08f));
        g.drawText (juce::String (title).toUpperCase(), row.removeFromTop (18), juce::Justification::centredLeft);

        g.setColour (Palette::textDim);
        g.setFont (makeFont (12.5f));
        g.drawFittedText (text, row, juce::Justification::topLeft, 3, 1.0f);
    }
}

} // namespace sonder::ui
