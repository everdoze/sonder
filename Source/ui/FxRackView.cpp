#include "FxRackView.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"

namespace sonder::ui
{

namespace
{
    constexpr int kTitleHeight = 26;
    constexpr int kStripGap = 6;
    constexpr int kHeaderWidth = 150, kDisplayWidth = 250;
    constexpr int kCellWidth = 72, kCellHeight = 92;
    constexpr int kScrollBarWidth = 8;
    const juce::String fxDragPrefix { "sonder-fx:" };

    // Подсказки к ручкам, назначение которых не очевидно из названия
    juce::String tooltipFor (FxType type, int param)
    {
        switch (type)
        {
            case FxType::distortion:
                if (param == fxp::distortion::level) return "Output level after the distortion";
                break;

            case FxType::phaser:
                if (param == fxp::phaser::centre) return "Frequency the notches sweep around";
                if (param == fxp::phaser::spread) return "Phase offset of the sweep between the left and right channels";
                break;

            case FxType::flanger:
                if (param == fxp::flanger::feedback) return "Negative feedback gives a hollow, 'through-zero' colour";
                if (param == fxp::flanger::spread)   return "Phase offset of the sweep between the left and right channels";
                break;

            case FxType::chorus:
                if (param == fxp::chorus::mode) return "I and II are the classic fixed modes; Free uses the Rate and Depth knobs";
                break;

            case FxType::delay:
                if (param == fxp::delay::sync)   return "Tempo-synced delay time. Free uses the Time knob";
                if (param == fxp::delay::tape)   return "Tape character: wow, flutter, darker and saturated repeats";
                if (param == fxp::delay::width)  return "100% is ping-pong, 0% is a plain mono echo";
                if (param == fxp::delay::offset) return "Shifts the delay time of the right channel";
                break;

            case FxType::compressor:
                if (param == fxp::compressor::sidechainHp) return "High-pass filter in the detector: bass triggers less compression";
                if (param == fxp::compressor::mix)         return "Blend with the dry signal (parallel compression)";
                break;

            case FxType::reverb:
                if (param == fxp::reverb::modulation) return "Slow modulation of the tail: removes metallic ringing";
                if (param == fxp::reverb::damping)    return "How fast the high frequencies of the tail die away";
                break;

            case FxType::equalizer:
                return "Drag the points on the display: frequency and gain. Mouse wheel changes Q";

            case FxType::filter:
                if (param == fxp::filter::depth)  return "How far the LFO moves the cutoff, in octaves each way. 0 - the LFO is off";
                if (param == fxp::filter::sync)   return "Tempo-synced LFO. Free uses the Rate knob";
                if (param == fxp::filter::spread) return "Phase offset of the LFO in the right channel: the sweep moves across the stereo field";
                break;

            case FxType::tremolo:
                if (param == fxp::tremolo::mode)   return "Tremolo moves the volume, Autopan moves the sound between left and right";
                if (param == fxp::tremolo::phase)  return "Phase offset of the right channel (tremolo mode): stereo tremolo";
                if (param == fxp::tremolo::smooth) return "Rounds the edges of the square and saw shapes: less clicking";
                break;

            case FxType::widener:
                if (param == fxp::widener::width)    return "0% - mono, 100% - unchanged, 200% - twice as wide";
                if (param == fxp::widener::spread)   return "Adds a delayed copy of the centre to the sides: mono sounds get wide too, and it disappears in mono";
                if (param == fxp::widener::bassMono) return "Below this frequency the sound stays in the centre. 20 Hz - off";
                break;

            case FxType::limiter:
                if (param == fxp::limiter::gain)    return "Drives the sound into the limiter: louder, denser";
                if (param == fxp::limiter::ceiling) return "The output never goes above this level";
                break;

            case FxType::multiband:
                if (param == fxp::multiband::upward)   return "Brings quiet parts of each band up";
                if (param == fxp::multiband::downward) return "Pushes loud parts of each band down";
                if (param == fxp::multiband::depth)    return "Blend with the unprocessed sound";
                if (param == fxp::multiband::time)     return "Speed of the compressors: below x1 faster and grainier, above slower and smoother";
                break;

            case FxType::shifter:
                if (param == fxp::shifter::shift)    return "Moves every frequency by the same number of hertz: harmonics stop being harmonic, the sound turns metallic";
                if (param == fxp::shifter::feedback) return "Feeds the output back into the shifter: a rising or falling 'barber pole' effect";
                if (param == fxp::shifter::spread)   return "Right channel shifts the other way: wide, swirling stereo";
                break;

            case FxType::none:
            case FxType::count:
                break;
        }

        return {};
    }
}

//==============================================================================
FxParamControl::FxParamControl (SonderAudioProcessor& processor, int slot, int param, const FxParamInfo& info)
{
    if (! info.isChoice())
    {
        knob = std::make_unique<ParameterControl> (processor, ParamIDs::fxParam (slot, param), info.label);
        knob->setLabelScale (0.7f);
        knob->setDefaultValue (info.toNormalised (info.defaultValue));

        // Дуга от середины у ручек с нулём посередине (усиление полос, обратная связь флэнжера)
        knob->setBipolar (info.minimum < 0.0f && std::abs (info.toNormalised (0.0f) - 0.5f) < 0.01f);
        addAndMakeVisible (*knob);
        return;
    }

    label.setText (info.label.toUpperCase(), juce::dontSendNotification);
    label.setFont (makeFont (10.5f, true, 0.1f));
    label.setColour (juce::Label::textColourId, Palette::textDim);
    label.setJustificationType (juce::Justification::centred);
    label.setMinimumHorizontalScale (0.7f);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    auto* parameter = processor.fxController.getParameter (slot, param);
    jassert (parameter != nullptr);

    {
        comboBox = std::make_unique<juce::ComboBox>();
        comboBox->addItemList (info.choices, 1);
        addAndMakeVisible (*comboBox);

        // Параметр слота хранит нормированное значение, список - номер варианта
        const auto* description = &info;
        comboAttachment = std::make_unique<juce::ParameterAttachment> (*parameter, [this, description] (float normalised)
        {
            comboBox->setSelectedId (juce::roundToInt (description->toReal (normalised)) + 1, juce::dontSendNotification);
        });

        comboBox->onChange = [this, description]
        {
            comboAttachment->setValueAsCompleteGesture (description->toNormalised ((float) (comboBox->getSelectedId() - 1)));
        };

        comboAttachment->sendInitialUpdate();
    }
}

void FxParamControl::setTooltipText (const juce::String& text)
{
    if (knob != nullptr)
        knob->setTooltipText (text.isNotEmpty() ? text : juce::String ("Drag an LFO or envelope here to modulate"));

    if (comboBox != nullptr)
        comboBox->setTooltip (text);
}

void FxParamControl::resized()
{
    auto area = getLocalBounds();

    if (knob != nullptr)
    {
        knob->setBounds (area);
        return;
    }

    label.setBounds (area.removeFromTop (16));

    if (comboBox != nullptr)
    {
        area.removeFromBottom (12);
        comboBox->setBounds (area.withSizeKeepingCentre (area.getWidth() - 6, 24));
    }
}

//==============================================================================
FxStrip::FxStrip (SonderAudioProcessor& p, int slotIndex, FxType effectType)
    : processor (p),
      slot (slotIndex),
      type (effectType),
      upButton ("Move up", 0.75f, Palette::textDim),
      downButton ("Move down", 0.25f, Palette::textDim),
      display (p, slotIndex)
{
    const auto& info = getFxTypeInfo (type);

    grip.setTooltip ("Drag to move the effect along the chain");
    grip.onDragStart = [this] (DragGrip& source)
    {
        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
        if (container != nullptr && ! container->isDragAndDropActive())
            container->startDragging (fxDragPrefix + juce::String (slot), &source,
                                      makeDragImage (getFxTypeInfo (type).name.toUpperCase()));
    };
    addAndMakeVisible (grip);

    onButton.setClickingTogglesState (true);
    onButton.setTooltip ("Switch the effect on or off");
    onButton.onStateChange = [this] { updateBypassLook(); };
    addAndMakeVisible (onButton);
    onAttachment = std::make_unique<Apvts::ButtonAttachment> (processor.parameters, ParamIDs::fxOn (slot), onButton);

    upButton.setTooltip ("Move earlier in the chain");
    upButton.onClick = [this] { processor.fxController.moveEffect (slot, position - 1); };
    addAndMakeVisible (upButton);

    // "Встать перед эффектом через один" и есть сдвиг на одну позицию вниз
    downButton.setTooltip ("Move later in the chain");
    downButton.onClick = [this] { processor.fxController.moveEffect (slot, position + 2); };
    addAndMakeVisible (downButton);

    removeButton.setTooltip ("Remove the effect");
    removeButton.onClick = [this] { processor.fxController.removeEffect (slot); };
    addAndMakeVisible (removeButton);

    addAndMakeVisible (display);

    for (int param = 0; param < juce::jmin (kNumFxParams, (int) info.params.size()); ++param)
    {
        auto& control = *controls.emplace_back (std::make_unique<FxParamControl> (processor, slot, param, info.params[(size_t) param]));
        control.setTooltipText (tooltipFor (type, param));
        addAndMakeVisible (control);
    }

    shownOn = onButton.getToggleState();
    updateBypassLook();
}

void FxStrip::setPosition (int newPosition, int chainLength)
{
    position = newPosition;
    upButton.setEnabled (position > 0);
    downButton.setEnabled (position + 1 < chainLength);
    upButton.setAlpha (upButton.isEnabled() ? 1.0f : 0.3f);
    downButton.setAlpha (downButton.isEnabled() ? 1.0f : 0.3f);
    repaint();
}

void FxStrip::updateBypassLook()
{
    // Выключенный эффект остаётся в рэке, но его ручки тускнеют
    const bool on = onButton.getToggleState();
    const float alpha = on ? 1.0f : 0.4f;

    for (auto& control : controls)
        setDimmed (*control, alpha);

    if (on != shownOn)
    {
        shownOn = on;
        repaint();
    }
}

void FxStrip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const bool on = onButton.getToggleState();

    g.setColour (Palette::deep.withAlpha (0.5f));
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    // Полоска слева: эффект участвует в обработке
    g.setColour (on ? Palette::accent : Palette::track);
    g.fillRoundedRectangle (bounds.getX() + 1.0f, bounds.getY() + 8.0f, 3.0f, bounds.getHeight() - 16.0f, 1.5f);

    g.setColour (on ? Palette::accent : Palette::textFaint);
    g.setFont (makeFont (13.0f, true));
    g.drawText (juce::String (position + 1), 32, 10, 18, 20, juce::Justification::centredLeft);

    g.setColour (on ? Palette::text.withAlpha (0.9f) : Palette::textDim);
    g.setFont (makeFont (11.5f, true, 0.16f));
    g.drawFittedText (getFxTypeInfo (type).name.toUpperCase(), 50, 10, kHeaderWidth - 54, 20,
                      juce::Justification::centredLeft, 1, 0.8f);

    // Номер слота: под этим номером ручки эффекта видны в автоматизации хоста
    g.setColour (Palette::textFaint);
    g.setFont (makeFont (9.5f, true, 0.2f));
    g.drawText ("SLOT " + juce::String (slot + 1), 12, getHeight() - 26, kHeaderWidth - 20, 14, juce::Justification::centredLeft);
}

void FxStrip::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromLeft (kHeaderWidth);

    grip.setBounds (8, 10, 22, 20);

    auto buttons = header.withTrimmedLeft (12).withTrimmedRight (8).withY (42).withHeight (22);
    onButton.setBounds (buttons.removeFromLeft (44));
    buttons.removeFromLeft (10);
    upButton.setBounds (buttons.removeFromLeft (18).withSizeKeepingCentre (14, 14));
    buttons.removeFromLeft (4);
    downButton.setBounds (buttons.removeFromLeft (18).withSizeKeepingCentre (14, 14));
    removeButton.setBounds (buttons.removeFromRight (24));

    display.setBounds (area.removeFromLeft (kDisplayWidth).reduced (0, 7));
    area.removeFromLeft (8);
    area.removeFromRight (6);

    if (controls.empty())
        return;

    // Ручки в один ряд; если их слишком много, ячейки сужаются
    const int cellWidth = juce::jmin (kCellWidth, area.getWidth() / (int) controls.size());
    const int y = (getHeight() - kCellHeight) / 2;

    for (size_t i = 0; i < controls.size(); ++i)
        controls[i]->setBounds (area.getX() + (int) i * cellWidth, y, cellWidth, kCellHeight);
}

//==============================================================================
FxRackView::FxRackView (SonderAudioProcessor& p)
    : processor (p)
{
    addButton.setTooltip ("Add an effect to the end of the chain. The same effect can be used several times");
    addButton.onClick = [this] { showAddMenu(); };
    addAndMakeVisible (addButton);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (kScrollBarWidth);
    viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, Palette::knobEdge);
    addAndMakeVisible (viewport);

    rebuild();
    startTimerHz (20);
}

void FxRackView::timerCallback()
{
    // Рэк меняется не только отсюда: пресеты, загрузка проекта
    if (processor.fxRack.getVersion() != rackVersion)
        rebuild();
}

void FxRackView::rebuild()
{
    rackVersion = processor.fxRack.getVersion();
    const auto chain = processor.fxController.getChain();

    // Полосы эффектов, которые остались на своих слотах, переиспользуются: при перестановке
    // ничего не пересоздаётся, и перетаскиваемый грип остаётся жив
    std::vector<std::unique_ptr<FxStrip>> next;

    for (int slot : chain)
    {
        const auto type = processor.fxRack.getType (slot);
        const auto existing = std::find_if (strips.begin(), strips.end(), [slot, type] (const auto& strip)
        {
            return strip != nullptr && strip->getSlot() == slot && strip->getType() == type;
        });

        if (existing != strips.end())
        {
            next.push_back (std::move (*existing));
        }
        else
        {
            next.push_back (std::make_unique<FxStrip> (processor, slot, type));
            content.addAndMakeVisible (*next.back());
        }
    }

    strips = std::move (next);
    addButton.setEnabled ((int) strips.size() < kNumFxSlots);
    layoutStrips();
    repaint();
}

void FxRackView::layoutStrips()
{
    const int width = juce::jmax (0, viewport.getWidth() - kScrollBarWidth - 2);
    const int count = (int) strips.size();

    content.setSize (width, juce::jmax (1, count * (FxStrip::kHeight + kStripGap) - kStripGap));

    for (int i = 0; i < count; ++i)
    {
        strips[(size_t) i]->setBounds (0, i * (FxStrip::kHeight + kStripGap), width, FxStrip::kHeight);
        strips[(size_t) i]->setPosition (i, count);
    }
}

void FxRackView::resized()
{
    auto area = getLocalBounds();
    auto title = area.removeFromTop (kTitleHeight);

    addButton.setBounds (title.removeFromRight (128).withTrimmedRight (8).withSizeKeepingCentre (120, 20));
    viewport.setBounds (area.reduced (8, 0).withTrimmedBottom (6));
    layoutStrips();
}

void FxRackView::showAddMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("ADD EFFECT");

    for (int i = (int) FxType::none + 1; i < (int) FxType::count; ++i)
        menu.addItem (i, getFxTypeInfo (static_cast<FxType> (i)).name);

    menu.addSeparator();
    menu.addItem (1000, "Remove all effects", ! strips.empty());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addButton),
                        [safeThis = juce::Component::SafePointer<FxRackView> (this)] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;

        auto& controller = safeThis->processor.fxController;

        if (result == 1000)
        {
            controller.clear (true);
            return;
        }

        controller.addEffect (static_cast<FxType> (result));
        safeThis->rebuild();

        // Новый эффект встаёт в конец цепочки: показываем его
        safeThis->viewport.setViewPosition (0, juce::jmax (0, safeThis->content.getHeight() - safeThis->viewport.getHeight()));
    });
}

//==============================================================================
int FxRackView::dropPositionAt (juce::Point<int> position) const
{
    // Позиция в цепочке: сколько полос лежит выше курсора
    const int y = position.y - viewport.getY() + viewport.getViewPositionY();
    int result = 0;

    for (const auto& strip : strips)
        if (y > strip->getBounds().getCentreY())
            ++result;

    return result;
}

bool FxRackView::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith (fxDragPrefix);
}

void FxRackView::itemDragMove (const SourceDetails& details)
{
    // У краёв списка он прокручивается, чтобы дотянуться до полос за пределами окна
    constexpr int edge = 24, step = 14;
    const int y = details.localPosition.y;

    if (y < viewport.getY() + edge)
        viewport.setViewPosition (0, juce::jmax (0, viewport.getViewPositionY() - step));
    else if (y > viewport.getBottom() - edge)
        viewport.setViewPosition (0, viewport.getViewPositionY() + step);

    dropPosition = dropPositionAt (details.localPosition);
    repaint();
}

void FxRackView::itemDragExit (const SourceDetails&)
{
    dropPosition = -1;
    repaint();
}

void FxRackView::itemDropped (const SourceDetails& details)
{
    const int slot = details.description.toString().fromFirstOccurrenceOf (fxDragPrefix, false, false).getIntValue();
    const int position = dropPositionAt (details.localPosition);

    dropPosition = -1;
    if (juce::isPositiveAndBelow (slot, kNumFxSlots))
        processor.fxController.moveEffect (slot, position);

    // Полосы переставит таймер, когда заметит новую версию рэка
    repaint();
}

void FxRackView::paint (juce::Graphics& g)
{
    // Сколько слотов занято
    g.setColour (Palette::textFaint);
    g.setFont (makeFont (10.5f, true, 0.12f));
    g.drawText (juce::String ((int) strips.size()) + " / " + juce::String (kNumFxSlots),
                addButton.getX() - 70, 4, 60, 20, juce::Justification::centredRight);

    if (strips.empty())
    {
        g.setColour (Palette::textDim);
        g.setFont (makeFont (14.0f));
        g.drawText ("The rack is empty. Press + ADD EFFECT to build the chain.", viewport.getBounds(),
                    juce::Justification::centred);
    }
}

void FxRackView::paintOverChildren (juce::Graphics& g)
{
    // Метка места, куда встанет перетаскиваемый эффект
    if (dropPosition < 0 || strips.empty())
        return;

    const int contentY = dropPosition * (FxStrip::kHeight + kStripGap) - kStripGap / 2;
    const int y = juce::jlimit (viewport.getY() + 2, viewport.getBottom() - 2,
                                viewport.getY() + contentY - viewport.getViewPositionY());
    const auto marker = juce::Rectangle<float> ((float) viewport.getX() + 4.0f, (float) y - 2.0f,
                                                (float) content.getWidth() - 8.0f, 4.0f);

    g.setColour (Palette::accent.withAlpha (0.3f));
    g.fillRoundedRectangle (marker.expanded (0.0f, 3.0f), 4.0f);
    g.setColour (Palette::accentBright);
    g.fillRoundedRectangle (marker, 2.0f);
}

} // namespace sonder::ui
