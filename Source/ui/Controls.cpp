#include "Controls.h"
#include "PluginProcessor.h"
#include "synth/ModRouting.h"
#include "Parameters.h"
#include "synth/SynthParams.h"
#include "SonderLookAndFeel.h"
#include "dsp/Saturation.h"

namespace sonder::ui
{

namespace
{
    void markBipolarAndDefault (juce::Slider& slider, juce::RangedAudioParameter* parameter)
    {
        if (parameter == nullptr)
            return;

        const auto& range = parameter->getNormalisableRange();
        if (range.start < 0.0f && range.end > 0.0f)
            slider.getProperties().set ("bipolar", true);

        slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
    }

    void drawGlowPath (juce::Graphics& g, const juce::Path& path, float width)
    {
        g.setColour (Palette::accent.withAlpha (0.12f));
        g.strokePath (path, { width * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        g.setColour (Palette::accent.withAlpha (0.3f));
        g.strokePath (path, { width * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        g.setColour (Palette::accentBright);
        g.strokePath (path, { width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }

    void fillDisplayBackground (juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        g.setColour (Palette::deep);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (Palette::outline.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
    }
}

//==============================================================================
// Слайдер, который отдаёт владельцу Alt+drag и правый клик
class ParameterControl::Knob final : public juce::Slider
{
public:
    explicit Knob (ParameterControl& ownerControl)
        : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
          owner (ownerControl)
    {
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! owner.handleMouseDown (e))
            juce::Slider::mouseDown (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! owner.handleMouseDrag (e))
            juce::Slider::mouseDrag (e);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! owner.handleMouseUp (e))
            juce::Slider::mouseUp (e);
    }

private:
    ParameterControl& owner;
};

namespace
{
    const juce::String dragPrefix { "sonder-mod:" };

    int sourceFromDescription (const juce::var& description)
    {
        const auto text = description.toString();
        return text.startsWith (dragPrefix) ? text.fromFirstOccurrenceOf (dragPrefix, false, false).getIntValue() : -1;
    }
}

ParameterControl::ParameterControl (SonderAudioProcessor& p, const juce::String& id, const juce::String& labelText, Style s)
    : processor (p), parameterID (id), style (s)
{
    auto& state = processor.parameters;

    label.setText (labelText.toUpperCase(), juce::dontSendNotification);
    label.setFont (makeFont (10.5f, true, 0.1f));
    label.setColour (juce::Label::textColourId, Palette::textDim);
    label.setJustificationType (style == Style::standard ? juce::Justification::centred : juce::Justification::centredLeft);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    parameter = state.getParameter (parameterID);
    jassert (parameter != nullptr);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
    {
        comboBox = std::make_unique<juce::ComboBox>();
        comboBox->addItemList (choice->choices, 1);
        addAndMakeVisible (*comboBox);
        comboBoxAttachment = std::make_unique<Apvts::ComboBoxAttachment> (state, parameterID, *comboBox);
        return;
    }

    destination = destinationForParameter (parameterID);

    slider = std::make_unique<Knob> (*this);
    slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    markBipolarAndDefault (*slider, parameter);
    addAndMakeVisible (*slider);

    if (style == Style::standard)
    {
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    }
    else
    {
        // Компактный вариант: значение отдельной подписью справа от ручки
        slider->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        valueLabel.setFont (makeFont (13.0f));
        valueLabel.setColour (juce::Label::textColourId, Palette::text);
        valueLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (valueLabel);
        slider->onValueChange = [this] { valueLabel.setText (slider->getTextFromValue (slider->getValue()), juce::dontSendNotification); };
    }

    sliderAttachment = std::make_unique<Apvts::SliderAttachment> (state, parameterID, *slider);

    if (destination != ModDest::off)
    {
        slider->setTooltip ("Drag an LFO or envelope here to modulate.\nAlt+drag: modulation depth, right-click: modulation menu");
        startTimerHz (30);
    }
}

ParameterControl::~ParameterControl()
{
    stopTimer();
}

void ParameterControl::resized()
{
    auto area = getLocalBounds();

    if (style == Style::inline_)
    {
        if (slider != nullptr)
            slider->setBounds (area.removeFromLeft (area.getHeight()));

        area.removeFromLeft (6);
        label.setBounds (area.removeFromTop (area.getHeight() / 2).withTrimmedTop (2));
        valueLabel.setBounds (area.withTrimmedBottom (2));
        return;
    }

    label.setBounds (area.removeFromTop (16));

    if (slider != nullptr)
        slider->setBounds (area.withTrimmedBottom (2));

    if (comboBox != nullptr)
    {
        area.removeFromBottom (12);
        comboBox->setBounds (area.withSizeKeepingCentre (area.getWidth() - 6, 24));
    }
}

void ParameterControl::setTooltipText (const juce::String& text)
{
    if (slider != nullptr)
        slider->setTooltip (destination != ModDest::off ? text + "\n\nAlt+drag: modulation depth, right-click: modulation menu" : text);

    if (comboBox != nullptr)
        comboBox->setTooltip (text);
}

void ParameterControl::paintOverChildren (juce::Graphics& g)
{
    // Во время Alt+drag вместо значения показываем глубину модуляции
    if (amountOverlay.isEmpty() || slider == nullptr)
        return;

    const auto area = juce::Rectangle<int> (0, getHeight() - 18, getWidth(), 16).toFloat();
    g.setColour (Palette::deep);
    g.fillRoundedRectangle (area.reduced (2.0f, 0.0f), 3.0f);
    g.setColour (Palette::accentBright);
    g.setFont (makeFont (11.0f, true));
    g.drawText (amountOverlay, area, juce::Justification::centred);
}

//==============================================================================
std::vector<int> ParameterControl::modulationSlots() const
{
    std::vector<int> slots;
    if (destination == ModDest::off)
        return slots;

    auto& state = processor.parameters;
    for (int slot = 0; slot < kNumModSlots; ++slot)
    {
        const auto dest = (int) state.getRawParameterValue (ParamIDs::modDest (slot))->load();
        const auto source = (int) state.getRawParameterValue (ParamIDs::modSource (slot))->load();
        if (dest == (int) destination && source != (int) ModSource::off)
            slots.push_back (slot);
    }

    return slots;
}

int ParameterControl::pickActiveSlot()
{
    const auto slots = modulationSlots();
    if (slots.empty())
        return -1;

    if (std::find (slots.begin(), slots.end(), activeSlot) == slots.end())
        activeSlot = slots.back();

    return activeSlot;
}

juce::String ParameterControl::describeSlot (int slot) const
{
    auto& state = processor.parameters;
    const int source = (int) state.getRawParameterValue (ParamIDs::modSource (slot))->load();
    const float amount = state.getRawParameterValue (ParamIDs::modAmount (slot))->load();
    const int percent = juce::roundToInt (amount * 100.0f);
    return Choices::modSources()[source] + "  " + (percent > 0 ? "+" : "") + juce::String (percent) + "%";
}

void ParameterControl::setParameterValue (const juce::String& id, float value)
{
    if (auto* target = processor.parameters.getParameter (id))
    {
        target->beginChangeGesture();
        target->setValueNotifyingHost (target->convertTo0to1 (value));
        target->endChangeGesture();
    }
}

void ParameterControl::addModulation (int sourceIndex)
{
    if (destination == ModDest::off || sourceIndex <= 0)
        return;

    auto& state = processor.parameters;
    int freeSlot = -1;

    for (int slot = 0; slot < kNumModSlots; ++slot)
    {
        const int source = (int) state.getRawParameterValue (ParamIDs::modSource (slot))->load();
        const int dest = (int) state.getRawParameterValue (ParamIDs::modDest (slot))->load();

        // Такая связь уже есть - просто делаем её активной
        if (source == sourceIndex && dest == (int) destination)
        {
            activeSlot = slot;
            return;
        }

        if (freeSlot < 0 && source == 0 && dest == 0)
            freeSlot = slot;
    }

    if (freeSlot < 0)
        return;

    setParameterValue (ParamIDs::modSource (freeSlot), (float) sourceIndex);
    setParameterValue (ParamIDs::modDest (freeSlot), (float) (int) destination);
    setParameterValue (ParamIDs::modAmount (freeSlot), 0.25f);
    activeSlot = freeSlot;
}

void ParameterControl::showModulationMenu()
{
    const auto slots = modulationSlots();
    juce::PopupMenu menu;
    menu.addSectionHeader ("MODULATION");

    if (slots.empty())
        menu.addItem (1, "Drag an LFO or envelope onto this knob", false);

    for (int slot : slots)
    {
        juce::PopupMenu sub;
        sub.addItem (1000 + slot, "Edit with Alt+drag", true, slot == activeSlot);
        sub.addItem (2000 + slot, "Invert");
        sub.addItem (3000 + slot, "Remove");
        menu.addSubMenu (describeSlot (slot), sub);
    }

    if (slots.size() > 1)
    {
        menu.addSeparator();
        menu.addItem (9000, "Remove all");
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slider.get()),
                        [safeThis = juce::Component::SafePointer<ParameterControl> (this), slots] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;

        auto& control = *safeThis;
        const auto clear = [&control] (int slot)
        {
            for (const auto& id : { ParamIDs::modSource (slot), ParamIDs::modDest (slot), ParamIDs::modAmount (slot) })
                if (auto* target = control.processor.parameters.getParameter (id))
                    target->setValueNotifyingHost (target->getDefaultValue());
        };

        if (result == 9000)
        {
            for (int slot : slots)
                clear (slot);
        }
        else if (result >= 3000)
        {
            clear (result - 3000);
        }
        else if (result >= 2000)
        {
            const int slot = result - 2000;
            const float amount = control.processor.parameters.getRawParameterValue (ParamIDs::modAmount (slot))->load();
            control.setParameterValue (ParamIDs::modAmount (slot), -amount);
        }
        else if (result >= 1000)
        {
            control.activeSlot = result - 1000;
        }
    });
}

bool ParameterControl::handleMouseDown (const juce::MouseEvent& e)
{
    if (destination == ModDest::off)
        return false;

    if (e.mods.isPopupMenu())
    {
        showModulationMenu();
        return true;
    }

    if (! e.mods.isAltDown())
        return false;

    const int slot = pickActiveSlot();
    if (slot < 0)
        return false;

    draggingAmount = true;
    dragStartAmount = processor.parameters.getRawParameterValue (ParamIDs::modAmount (slot))->load();
    dragStartY = e.position.y;

    if (auto* amount = processor.parameters.getParameter (ParamIDs::modAmount (slot)))
        amount->beginChangeGesture();

    amountOverlay = describeSlot (slot);
    repaint();
    return true;
}

bool ParameterControl::handleMouseDrag (const juce::MouseEvent& e)
{
    if (! draggingAmount)
        return false;

    // Вверх - больше, Shift - точная подстройка
    const float sensitivity = e.mods.isShiftDown() ? 600.0f : 150.0f;
    const float amount = juce::jlimit (-1.0f, 1.0f, dragStartAmount + (dragStartY - e.position.y) / sensitivity);

    if (auto* target = processor.parameters.getParameter (ParamIDs::modAmount (activeSlot)))
        target->setValueNotifyingHost (target->convertTo0to1 (amount));

    amountOverlay = describeSlot (activeSlot);
    repaint();
    return true;
}

bool ParameterControl::handleMouseUp (const juce::MouseEvent&)
{
    if (! draggingAmount)
        return false;

    if (auto* amount = processor.parameters.getParameter (ParamIDs::modAmount (activeSlot)))
        amount->endChangeGesture();

    draggingAmount = false;
    amountOverlay.clear();
    repaint();
    return true;
}

void ParameterControl::timerCallback()
{
    if (slider == nullptr || parameter == nullptr)
        return;

    auto& state = processor.parameters;
    float low = 0.0f, high = 0.0f;
    bool active = false;

    for (int slot : modulationSlots())
    {
        const auto source = static_cast<ModSource> ((int) state.getRawParameterValue (ParamIDs::modSource (slot))->load());
        const float amount = state.getRawParameterValue (ParamIDs::modAmount (slot))->load();
        if (amount == 0.0f)
            continue;

        active = true;
        if (isBipolarSource (source))
        {
            low -= std::abs (amount);
            high += std::abs (amount);
        }
        else
        {
            low += juce::jmin (0.0f, amount);
            high += juce::jmax (0.0f, amount);
        }
    }

    const auto& range = parameter->getNormalisableRange();
    const float base = parameter->convertFrom0to1 (parameter->getValue());
    const auto normalised = [&] (float modulation)
    {
        const float value = juce::jlimit (range.start, range.end, applyModulation (destination, base, modulation));
        return range.convertTo0to1 (value);
    };

    auto& sliderProperties = slider->getProperties();
    const float modMin = active ? normalised (low) : 0.0f;
    const float modMax = active ? normalised (high) : 0.0f;
    const bool live = active && processor.displayVoiceActive.load();
    const float modLive = live ? normalised (processor.displayModulation[(size_t) destination].load()) : -1.0f;

    const bool changed = (bool) sliderProperties.getWithDefault ("modActive", false) != active
                      || (float) sliderProperties.getWithDefault ("modMin", 0.0f) != modMin
                      || (float) sliderProperties.getWithDefault ("modMax", 0.0f) != modMax
                      || (float) sliderProperties.getWithDefault ("modLive", -1.0f) != modLive;

    if (changed)
    {
        sliderProperties.set ("modActive", active);
        sliderProperties.set ("modMin", modMin);
        sliderProperties.set ("modMax", modMax);
        sliderProperties.set ("modLive", modLive);
        slider->repaint();
    }
}

//==============================================================================
bool ParameterControl::isInterestedInDragSource (const SourceDetails& details)
{
    return destination != ModDest::off && sourceFromDescription (details.description) > 0;
}

void ParameterControl::itemDragEnter (const SourceDetails&)
{
    slider->getProperties().set ("dropTarget", true);
    slider->repaint();
}

void ParameterControl::itemDragExit (const SourceDetails&)
{
    slider->getProperties().set ("dropTarget", false);
    slider->repaint();
}

void ParameterControl::itemDropped (const SourceDetails& details)
{
    slider->getProperties().set ("dropTarget", false);
    addModulation (sourceFromDescription (details.description));
    timerCallback();
}

//==============================================================================
ModSourceHandle::ModSourceHandle (std::function<ModSource()> provider)
    : sourceProvider (std::move (provider))
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag onto a knob to modulate it");
}

void ModSourceHandle::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const bool over = isMouseOver();

    g.setColour (over ? Palette::accent.withAlpha (0.25f) : Palette::deep);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (over ? Palette::accent : Palette::outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    // Четыре стрелки "перетащи меня"
    const auto c = bounds.getCentre();
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.32f;
    juce::Path arrows;
    arrows.startNewSubPath (c.x - r, c.y);
    arrows.lineTo (c.x + r, c.y);
    arrows.startNewSubPath (c.x, c.y - r);
    arrows.lineTo (c.x, c.y + r);

    const float h = r * 0.4f;
    for (auto [dx, dy] : { std::pair { 1.0f, 0.0f }, std::pair { -1.0f, 0.0f }, std::pair { 0.0f, 1.0f }, std::pair { 0.0f, -1.0f } })
    {
        const juce::Point<float> tip (c.x + dx * r, c.y + dy * r);
        arrows.startNewSubPath (tip.x - dx * h + dy * h, tip.y - dy * h + dx * h);
        arrows.lineTo (tip);
        arrows.lineTo (tip.x - dx * h - dy * h, tip.y - dy * h - dx * h);
    }

    g.setColour (Palette::accentBright);
    g.strokePath (arrows, { 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
}

void ModSourceHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 3)
        startModulationDrag (*this, sourceProvider());
}

void ModSourceHandle::startModulationDrag (juce::Component& source, ModSource modSource)
{
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (&source);
    if (container == nullptr || container->isDragAndDropActive())
        return;

    // Картинка-"пилюля" с именем источника
    const auto name = Choices::modSources()[(int) modSource];
    const auto font = makeFont (13.0f, true);
    const int width = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, name)) + 22;
    juce::Image image (juce::Image::ARGB, width, 24, true);
    {
        juce::Graphics g (image);
        const auto bounds = image.getBounds().toFloat().reduced (1.0f);
        g.setColour (Palette::deep.withAlpha (0.92f));
        g.fillRoundedRectangle (bounds, 11.0f);
        g.setColour (Palette::accent);
        g.drawRoundedRectangle (bounds, 11.0f, 1.5f);
        g.setColour (Palette::accentBright);
        g.setFont (font);
        g.drawText (name, bounds, juce::Justification::centred);
    }

    container->startDragging (dragPrefix + juce::String ((int) modSource), &source, juce::ScaledImage (image));
}

//==============================================================================
GroupedChoiceAttachment::GroupedChoiceAttachment (juce::RangedAudioParameter& parameter, juce::ComboBox& box,
                                                  std::function<void()> onChange)
    : comboBox (box),
      callback (std::move (onChange)),
      attachment (parameter, [this] (float value)
      {
          comboBox.setSelectedId (juce::roundToInt (value) + 1, juce::dontSendNotification);
          if (callback)
              callback();
      })
{
    comboBox.onChange = [this]
    {
        attachment.setValueAsCompleteGesture ((float) (comboBox.getSelectedId() - 1));
        if (callback)
            callback();
    };

    attachment.sendInitialUpdate();
}

namespace
{
    void addItems (juce::ComboBox& box, const juce::StringArray& names, std::initializer_list<int> indices)
    {
        for (int index : indices)
            box.addItem (names[index], index + 1);
    }

    void fillSources (juce::ComboBox& box)
    {
        const auto& names = Choices::modSources();
        using S = ModSource;
        box.addItem (names[0], 1);
        box.addSectionHeading ("LFO");
        for (int lfo = 0; lfo < kNumLfos; ++lfo)
            addItems (box, names, { (int) sourceForLfo (lfo) });
        box.addSectionHeading ("Envelopes");
        addItems (box, names, { (int) S::filterEnv, (int) S::ampEnv });
        box.addSectionHeading ("Performance");
        addItems (box, names, { (int) S::velocity, (int) S::modWheel, (int) S::aftertouch, (int) S::key, (int) S::random });
    }

    void fillDestinations (juce::ComboBox& box)
    {
        const auto& names = Choices::modDestinations();
        using D = ModDest;
        box.addItem (names[0], 1);
        box.addSectionHeading ("Pitch");
        addItems (box, names, { (int) D::pitch, (int) D::osc1Pitch, (int) D::osc2Pitch });
        box.addSectionHeading ("Oscillators");
        addItems (box, names, { (int) D::osc1WtPos, (int) D::osc2WtPos, (int) D::pulseWidth, (int) D::oscMix,
                                (int) D::fm, (int) D::fold, (int) D::sub, (int) D::noise });
        box.addSectionHeading ("Filter");
        addItems (box, names, { (int) D::cutoff, (int) D::resonance, (int) D::drive, (int) D::vowel });
        box.addSectionHeading ("Distortion");
        addItems (box, names, { (int) D::distDrive, (int) D::distMix });
        box.addSectionHeading ("Amp");
        addItems (box, names, { (int) D::amp, (int) D::pan });
        box.addSectionHeading ("LFO Rate");
        for (int lfo = 0; lfo < kNumLfos; ++lfo)
            addItems (box, names, { (int) rateDestForLfo (lfo) });
    }
}

ModSlotView::ModSlotView (Apvts& s, int slotIndex)
    : state (s), slot (slotIndex)
{
    fillSources (source);
    fillDestinations (destination);

    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    markBipolarAndDefault (amount, dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (ParamIDs::modAmount (slot))));

    clearButton.setTooltip ("Clear this slot");
    clearButton.onClick = [this] { clear(); };

    addAndMakeVisible (source);
    addAndMakeVisible (destination);
    addAndMakeVisible (amount);
    addAndMakeVisible (clearButton);

    const auto parameter = [this] (const juce::String& id) -> juce::RangedAudioParameter&
    {
        return *dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (id));
    };

    sourceAttachment = std::make_unique<GroupedChoiceAttachment> (parameter (ParamIDs::modSource (slot)), source,
                                                                  [this] { updateActiveState(); });
    destinationAttachment = std::make_unique<GroupedChoiceAttachment> (parameter (ParamIDs::modDest (slot)), destination,
                                                                       [this] { updateActiveState(); });
    amountAttachment = std::make_unique<Apvts::SliderAttachment> (state, ParamIDs::modAmount (slot), amount);
    updateActiveState();
}

bool ModSlotView::isInUse() const
{
    return source.getSelectedId() > 1 || destination.getSelectedId() > 1;
}

void ModSlotView::clear()
{
    for (const auto& id : { ParamIDs::modSource (slot), ParamIDs::modDest (slot), ParamIDs::modAmount (slot) })
    {
        if (auto* parameter = state.getParameter (id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
    }
}

void ModSlotView::updateActiveState()
{
    const bool active = source.getSelectedId() > 1 && destination.getSelectedId() > 1;
    amount.setAlpha (active ? 1.0f : 0.4f);
    clearButton.setAlpha (isInUse() ? 1.0f : 0.3f);
    repaint();
}

void ModSlotView::paint (juce::Graphics& g)
{
    const bool active = source.getSelectedId() > 1 && destination.getSelectedId() > 1;
    const auto numberArea = getLocalBounds().removeFromLeft (24).toFloat();

    g.setColour (active ? Palette::accent : Palette::textFaint);
    g.setFont (makeFont (12.0f, true));
    g.drawText (juce::String (slot + 1), numberArea, juce::Justification::centred);

    // Стрелка между источником и целью
    const float arrowX = (float) source.getRight() + 8.0f;
    const float arrowY = (float) getHeight() * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath (arrowX - 3.0f, arrowY - 4.0f);
    arrow.lineTo (arrowX + 2.0f, arrowY);
    arrow.lineTo (arrowX - 3.0f, arrowY + 4.0f);
    g.strokePath (arrow, { 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
}

void ModSlotView::resized()
{
    auto area = getLocalBounds();
    area.removeFromLeft (24);
    clearButton.setBounds (area.removeFromRight (26).withSizeKeepingCentre (24, 24));
    area.removeFromRight (8);

    const int comboWidth = (area.getWidth() - 16 - 10) * 30 / 100;
    source.setBounds (area.removeFromLeft (comboWidth).withSizeKeepingCentre (comboWidth, 26));
    area.removeFromLeft (16);
    destination.setBounds (area.removeFromLeft (comboWidth).withSizeKeepingCentre (comboWidth, 26));
    area.removeFromLeft (10);
    amount.setBounds (area.withSizeKeepingCentre (area.getWidth(), 28));
}

//==============================================================================
EnvelopeView::EnvelopeView (Apvts& state, const char* attackID, const char* decayID, const char* sustainID, const char* releaseID)
{
    const char* ids[] { attackID, decayID, sustainID, releaseID };
    for (size_t i = 0; i < parameters.size(); ++i)
        parameters[i] = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (ids[i]));

    startTimerHz (20);
}

void EnvelopeView::timerCallback()
{
    bool changed = false;
    for (size_t i = 0; i < parameters.size(); ++i)
    {
        const float value = parameters[i] != nullptr ? parameters[i]->getValue() : 0.0f;
        changed |= value != lastValues[i];
        lastValues[i] = value;
    }

    if (changed)
        repaint();
}

void EnvelopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    fillDisplayBackground (g, bounds);

    const auto plot = bounds.reduced (8.0f, 7.0f);
    const float attack = lastValues[0], decay = lastValues[1], sustain = lastValues[2], release = lastValues[3];

    // Ширина сегментов пропорциональна положению ручек (они уже в "слуховой" шкале)
    const float widths[] { 0.03f + attack, 0.03f + decay, 0.35f, 0.03f + release };
    const float scale = plot.getWidth() / (widths[0] + widths[1] + widths[2] + widths[3]);

    const auto toY = [&plot] (float level) { return plot.getBottom() - level * plot.getHeight(); };

    juce::Path curve;
    float x = plot.getX();
    curve.startNewSubPath (x, toY (0.0f));

    constexpr int steps = 28;
    const auto addSegment = [&] (float width, auto levelAt)
    {
        for (int i = 1; i <= steps; ++i)
        {
            const float t = (float) i / steps;
            curve.lineTo (x + width * t, toY (levelAt (t)));
        }
        x += width;
    };

    addSegment (widths[0] * scale, [] (float t) { return juce::jmin (1.0f, 1.3f * (1.0f - std::exp (-1.466f * t))); });
    const juce::Point<float> peak (x, toY (1.0f));
    addSegment (widths[1] * scale, [sustain] (float t) { return sustain + (1.0f - sustain) * std::exp (-6.9f * t); });
    const juce::Point<float> decayEnd (x, toY (sustain));
    addSegment (widths[2] * scale, [sustain] (float) { return sustain; });
    const juce::Point<float> releaseStart (x, toY (sustain));
    addSegment (widths[3] * scale, [sustain] (float t) { return sustain * std::exp (-6.9f * t); });

    auto fill = curve;
    fill.lineTo (x, plot.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.28f), 0.0f, plot.getY(),
                                             Palette::accent.withAlpha (0.0f), 0.0f, plot.getBottom(), false));
    g.fillPath (fill);

    drawGlowPath (g, curve, 1.6f);

    g.setColour (Palette::accentBright);
    for (auto point : { peak, decayEnd, releaseStart })
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (point));
}

//==============================================================================
VoiceLeds::VoiceLeds (const std::atomic<uint32_t>& voiceMask)
    : mask (voiceMask)
{
    startTimerHz (30);
}

void VoiceLeds::timerCallback()
{
    const uint32_t bits = mask.load();
    bool changed = false;

    for (size_t i = 0; i < brightness.size(); ++i)
    {
        const float target = (bits >> i) & 1u ? 1.0f : 0.0f;
        const float next = target > brightness[i] ? target : brightness[i] * 0.8f;
        changed |= std::abs (next - brightness[i]) > 0.001f;
        brightness[i] = next < 0.01f ? 0.0f : next;
    }

    if (changed)
        repaint();
}

void VoiceLeds::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    const auto caption = area.removeFromBottom (14.0f);

    g.setColour (Palette::textFaint);
    g.setFont (makeFont (9.5f, true, 0.2f));
    g.drawText ("VOICES", caption, juce::Justification::centred);

    const float step = area.getWidth() / (float) brightness.size();
    for (size_t i = 0; i < brightness.size(); ++i)
    {
        const auto centre = juce::Point<float> (area.getX() + step * ((float) i + 0.5f), area.getCentreY());
        const float b = brightness[i];

        if (b > 0.0f)
        {
            g.setColour (Palette::accent.withAlpha (0.25f * b));
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (centre));
        }

        g.setColour (Palette::track.interpolatedWith (Palette::accentBright, b));
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (centre));
    }
}

} // namespace sonder::ui
