#include "Controls.h"
#include "PluginProcessor.h"
#include "synth/ModRouting.h"
#include "Parameters.h"
#include "synth/SynthParams.h"
#include "SonderLookAndFeel.h"
#include "Theme.h"
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
        owner.endMorph(); // рука на ручке важнее анимации

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

    void setModParameter (Apvts& state, const juce::String& id, float value)
    {
        if (auto* target = state.getParameter (id))
        {
            target->beginChangeGesture();
            target->setValueNotifyingHost (target->convertTo0to1 (value));
            target->endChangeGesture();
        }
    }
}

int addModulationSlot (Apvts& state, ModSource source, ModDest dest)
{
    if (source == ModSource::off || dest == ModDest::off)
        return -1;

    int freeSlot = -1;

    for (int slot = 0; slot < kNumModSlots; ++slot)
    {
        const int slotSource = (int) state.getRawParameterValue (ParamIDs::modSource (slot))->load();
        const int slotDest = (int) state.getRawParameterValue (ParamIDs::modDest (slot))->load();

        // Такая связь уже есть
        if (slotSource == (int) source && slotDest == (int) dest)
            return slot;

        if (freeSlot < 0 && slotSource == 0 && slotDest == 0)
            freeSlot = slot;
    }

    if (freeSlot < 0)
        return -1;

    setModParameter (state, ParamIDs::modSource (freeSlot), (float) (int) source);
    setModParameter (state, ParamIDs::modDest (freeSlot), (float) (int) dest);
    setModParameter (state, ParamIDs::modAmount (freeSlot), 0.25f);
    setModParameter (state, ParamIDs::modPolarity (freeSlot), (float) (int) ModPolarity::both);
    return freeSlot;
}

void clearModulationSlot (Apvts& state, int slot)
{
    for (const auto& id : { ParamIDs::modSource (slot), ParamIDs::modDest (slot), ParamIDs::modAmount (slot),
                            ParamIDs::modPolarity (slot) })
    {
        if (auto* parameter = state.getParameter (id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->getDefaultValue());
            parameter->endChangeGesture();
        }
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
        slider->setTooltip ("Drag an LFO or envelope here to modulate.\nAlt+drag: depth, Alt+click: next modulator, right-click: menu");
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

void ParameterControl::setBipolar (bool bipolar)
{
    if (slider != nullptr)
        slider->getProperties().set ("bipolar", bipolar);
}

void ParameterControl::setDefaultValue (double value)
{
    if (slider != nullptr)
        slider->setDoubleClickReturnValue (true, value);
}

void ParameterControl::setLabelScale (float minimumHorizontalScale)
{
    label.setMinimumHorizontalScale (minimumHorizontalScale);
}

void ParameterControl::setTooltipText (const juce::String& text)
{
    if (slider != nullptr)
        slider->setTooltip (destination != ModDest::off ? text + "\n\nAlt+drag: depth, Alt+click: next modulator, right-click: menu" : text);

    if (comboBox != nullptr)
        comboBox->setTooltip (text);
}

void ParameterControl::paintOverChildren (juce::Graphics& g)
{
    // Пока редактируется модуляция: вместо подписи - номер модулятора, вместо значения - источник и глубина
    if (amountOverlay.isEmpty() || slider == nullptr)
        return;

    const auto drawBadge = [&g] (juce::Rectangle<int> area, const juce::String& text, juce::Colour colour, float fontSize)
    {
        g.setColour (Palette::deep);
        g.fillRoundedRectangle (area.toFloat().reduced (1.0f, 0.0f), 3.0f);
        g.setColour (colour);
        g.setFont (makeFont (fontSize, true));
        g.drawFittedText (text, area, juce::Justification::centred, 1, 0.7f);
    };

    drawBadge ({ 0, 0, getWidth(), 16 }, overlayCaption, Palette::accent, 10.5f);
    drawBadge ({ 0, getHeight() - 18, getWidth(), 16 }, amountOverlay, Palette::accentBright, 11.0f);
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
    const int polarity = (int) state.getRawParameterValue (ParamIDs::modPolarity (slot))->load();
    const int percent = juce::roundToInt (amount * 100.0f);
    static const char* directions[] { "both ways", "up", "down" };
    return Choices::modSources()[source] + "  " + (percent > 0 ? "+" : "") + juce::String (percent) + "%, "
         + directions[juce::jlimit (0, 2, polarity)];
}

std::pair<float, float> ParameterControl::slotSpan (int slot) const
{
    auto& state = processor.parameters;
    const auto source = static_cast<ModSource> ((int) state.getRawParameterValue (ParamIDs::modSource (slot))->load());
    const auto polarity = static_cast<ModPolarity> (juce::jlimit (0, 2, (int) state.getRawParameterValue (ParamIDs::modPolarity (slot))->load()));
    const float amount = state.getRawParameterValue (ParamIDs::modAmount (slot))->load();
    return modulationSpan (source, polarity, amount);
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

    // Такая связь уже есть - просто делаем её активной
    if (const int slot = addModulationSlot (processor.parameters, static_cast<ModSource> (sourceIndex), destination); slot >= 0)
        activeSlot = slot;
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
        const int polarity = (int) processor.parameters.getRawParameterValue (ParamIDs::modPolarity (slot))->load();

        juce::PopupMenu sub;
        sub.addItem (1000 + slot, "Edit with Alt+drag", true, slot == activeSlot);
        sub.addSectionHeader ("DIRECTION");
        sub.addItem (4000 + slot * 4 + (int) ModPolarity::both, "Both ways from the knob", true, polarity == (int) ModPolarity::both);
        sub.addItem (4000 + slot * 4 + (int) ModPolarity::up, "Up only (adds)", true, polarity == (int) ModPolarity::up);
        sub.addItem (4000 + slot * 4 + (int) ModPolarity::down, "Down only (subtracts)", true, polarity == (int) ModPolarity::down);
        sub.addSeparator();
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
            for (const auto& id : { ParamIDs::modSource (slot), ParamIDs::modDest (slot), ParamIDs::modAmount (slot),
                                    ParamIDs::modPolarity (slot) })
                if (auto* target = control.processor.parameters.getParameter (id))
                    target->setValueNotifyingHost (target->getDefaultValue());
        };

        if (result >= 4000 && result < 9000)
        {
            const int slot = (result - 4000) / 4;
            control.setParameterValue (ParamIDs::modPolarity (slot), (float) ((result - 4000) % 4));
            control.activeSlot = slot;
            control.showOverlay (slot, 1500);
        }
        else if (result == 9000)
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
            control.showOverlay (control.activeSlot, 1500);
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
    dragMoved = false;
    dragStartAmount = processor.parameters.getRawParameterValue (ParamIDs::modAmount (slot))->load();
    dragStartY = e.position.y;

    if (auto* amount = processor.parameters.getParameter (ParamIDs::modAmount (slot)))
        amount->beginChangeGesture();

    showOverlay (slot, 0);
    return true;
}

bool ParameterControl::handleMouseDrag (const juce::MouseEvent& e)
{
    if (! draggingAmount)
        return false;

    // Пока мышь почти не сдвинулась, это ещё может оказаться Alt+клик (смена модулятора)
    if (! dragMoved && e.getDistanceFromDragStart() < 3)
        return true;

    dragMoved = true;

    // Вверх - больше, Shift - точная подстройка
    const float sensitivity = e.mods.isShiftDown() ? 600.0f : 150.0f;
    const float amount = juce::jlimit (-1.0f, 1.0f, dragStartAmount + (dragStartY - e.position.y) / sensitivity);

    if (auto* target = processor.parameters.getParameter (ParamIDs::modAmount (activeSlot)))
        target->setValueNotifyingHost (target->convertTo0to1 (amount));

    showOverlay (activeSlot, 0);
    return true;
}

bool ParameterControl::handleMouseUp (const juce::MouseEvent&)
{
    if (! draggingAmount)
        return false;

    if (auto* amount = processor.parameters.getParameter (ParamIDs::modAmount (activeSlot)))
        amount->endChangeGesture();

    draggingAmount = false;

    // Alt+клик без перетаскивания: переходим к следующему модулятору этой ручки
    if (! dragMoved)
    {
        const auto slots = modulationSlots();
        const auto it = std::find (slots.begin(), slots.end(), activeSlot);
        if (it != slots.end() && slots.size() > 1)
            activeSlot = slots[(size_t) ((it - slots.begin() + 1) % (std::ptrdiff_t) slots.size())];
    }

    // Подпись ещё немного висит, чтобы было видно, какой модулятор выбран
    showOverlay (activeSlot, 1500);
    return true;
}

void ParameterControl::showOverlay (int slot, int holdMilliseconds)
{
    // Ячейка узкая, поэтому имя источника сокращаем, а номер модулятора пишем вместо подписи ручки
    static const char* shortNames[] { "Off", "LFO 1", "LFO 2", "F.Env", "A.Env", "Vel", "M.Whl", "A.Tch", "Key", "Rnd",
                                      "LFO 3", "LFO 4", "LFO 5", "LFO 6", "LFO 7", "LFO 8", "Slide" };
    static const char* directions[] { "", " up", " down" };

    auto& state = processor.parameters;
    const int source = juce::jlimit (0, (int) std::size (shortNames) - 1,
                                     (int) state.getRawParameterValue (ParamIDs::modSource (slot))->load());
    const int percent = juce::roundToInt (state.getRawParameterValue (ParamIDs::modAmount (slot))->load() * 100.0f);
    const int polarity = juce::jlimit (0, 2, (int) state.getRawParameterValue (ParamIDs::modPolarity (slot))->load());
    amountOverlay = juce::String (shortNames[source]) + " " + (percent > 0 ? "+" : "") + juce::String (percent) + "%"
                  + directions[polarity];

    const auto slots = modulationSlots();
    const auto position = std::find (slots.begin(), slots.end(), slot) - slots.begin() + 1;
    overlayCaption = slots.size() > 1 ? "MOD " + juce::String ((int) position) + "/" + juce::String ((int) slots.size())
                                      : juce::String ("MOD");

    overlayExpiry = holdMilliseconds > 0 ? juce::Time::getMillisecondCounter() + (juce::uint32) holdMilliseconds : 0;
    updateModulationRing();
    repaint();
}

void ParameterControl::timerCallback()
{
    updateMorph();

    if (destination != ModDest::off)
        updateModulationRing();
}

void ParameterControl::beginMorph()
{
    if (slider == nullptr || ! isVisible())
        return;

    // Если прошлая анимация ещё идёт, продолжаем с того места, где ручка нарисована сейчас
    auto& sliderProperties = slider->getProperties();
    const float shown = sliderProperties.getWithDefault ("animPos", -1.0f);

    morphFrom = morphing && shown >= 0.0f ? shown : (float) slider->valueToProportionOfLength (slider->getValue());
    morphStart = juce::Time::getMillisecondCounter();
    morphing = true;
    sliderProperties.set ("animPos", morphFrom);
    startTimerHz (60);
}

void ParameterControl::updateMorph()
{
    if (! morphing)
        return;

    constexpr float duration = 380.0f; // мс
    const float t = (float) (juce::Time::getMillisecondCounter() - morphStart) / duration;

    if (t >= 1.0f)
    {
        endMorph();
        return;
    }

    // Быстро трогается и мягко останавливается
    const float eased = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    const float target = (float) slider->valueToProportionOfLength (slider->getValue());
    slider->getProperties().set ("animPos", morphFrom + (target - morphFrom) * eased);
    slider->repaint();
}

void ParameterControl::endMorph()
{
    if (! morphing)
        return;

    morphing = false;
    slider->getProperties().remove ("animPos");
    slider->repaint();

    // Таймер нужен дальше только ручкам с кольцом модуляции
    if (destination != ModDest::off)
        startTimerHz (30);
    else
        stopTimer();
}

void ParameterControl::updateModulationRing()
{
    if (slider == nullptr || parameter == nullptr)
        return;

    auto& state = processor.parameters;
    float low = 0.0f, high = 0.0f;
    bool active = false;

    for (int slot : modulationSlots())
    {
        if (state.getRawParameterValue (ParamIDs::modAmount (slot))->load() == 0.0f)
            continue;

        active = true;
        const auto [spanLow, spanHigh] = slotSpan (slot);
        low += spanLow;
        high += spanHigh;
    }

    const auto& range = parameter->getNormalisableRange();
    const float base = parameter->convertFrom0to1 (parameter->getValue());
    const auto normalised = [&] (float modulation)
    {
        const float value = juce::jlimit (range.start, range.end, applyModulation (destination, base, modulation, range));
        return range.convertTo0to1 (value);
    };

    auto& sliderProperties = slider->getProperties();
    const float modMin = active ? normalised (low) : 0.0f;
    const float modMax = active ? normalised (high) : 0.0f;

    // Общие цели (рэк, Master) живут и без звучащего голоса: их LFO идёт всегда
    const bool live = active && (isGlobalDest (destination) || processor.displayVoiceActive.load());
    const float modLive = live ? normalised (processor.displayModulation[(size_t) destination].load()) : -1.0f;

    // Подпись редактирования гаснет сама через заданное время
    if (! draggingAmount && amountOverlay.isNotEmpty() && overlayExpiry != 0
        && juce::Time::getMillisecondCounter() > overlayExpiry)
    {
        amountOverlay.clear();
        overlayCaption.clear();
        repaint();
    }

    // Диапазон редактируемого модулятора отдельной дугой: видно, какой из нескольких сейчас выбран
    const auto slots = modulationSlots();
    const bool editing = amountOverlay.isNotEmpty() && std::find (slots.begin(), slots.end(), activeSlot) != slots.end();
    float editMin = 0.0f, editMax = 0.0f;

    if (editing)
    {
        const auto [spanLow, spanHigh] = slotSpan (activeSlot);
        editMin = normalised (spanLow);
        editMax = normalised (spanHigh);
    }

    const bool changed = (bool) sliderProperties.getWithDefault ("modActive", false) != active
                      || (float) sliderProperties.getWithDefault ("modMin", 0.0f) != modMin
                      || (float) sliderProperties.getWithDefault ("modMax", 0.0f) != modMax
                      || (float) sliderProperties.getWithDefault ("modLive", -1.0f) != modLive
                      || (bool) sliderProperties.getWithDefault ("modEdit", false) != editing
                      || (float) sliderProperties.getWithDefault ("modEditMin", 0.0f) != editMin
                      || (float) sliderProperties.getWithDefault ("modEditMax", 0.0f) != editMax;

    if (changed)
    {
        sliderProperties.set ("modActive", active);
        sliderProperties.set ("modMin", modMin);
        sliderProperties.set ("modMax", modMax);
        sliderProperties.set ("modLive", modLive);
        sliderProperties.set ("modEdit", editing);
        sliderProperties.set ("modEditMin", editMin);
        sliderProperties.set ("modEditMax", editMax);
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
    updateModulationRing();
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

int ModSourceHandle::sourceFromDrag (const juce::var& description)
{
    return sourceFromDescription (description);
}

//==============================================================================
ModSourceChip::ModSourceChip (ModSource modSource, const juce::String& label)
    : source (modSource), text (label)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag onto a knob to modulate it with " + Choices::modSources()[(int) modSource]);
}

void ModSourceChip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const bool over = isMouseOver();

    g.setColour (over ? Palette::accent.withAlpha (0.25f) : Palette::deep);
    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
    g.setColour (over ? Palette::accent : Palette::outline);
    g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.0f);

    g.setColour (over ? Palette::accentBright : Palette::textDim);
    g.setFont (makeFont (10.0f, true, 0.08f));
    g.drawFittedText (text, getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.8f);
}

void ModSourceChip::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 3)
        ModSourceHandle::startModulationDrag (*this, source);
}

void ModSourceHandle::startModulationDrag (juce::Component& source, ModSource modSource)
{
    auto* container = juce::DragAndDropContainer::findParentDragContainerFor (&source);
    if (container == nullptr || container->isDragAndDropActive())
        return;

    container->startDragging (dragPrefix + juce::String ((int) modSource), &source,
                              makeDragImage (Choices::modSources()[(int) modSource]));
}

juce::ScaledImage makeDragImage (const juce::String& text)
{
    const auto font = makeFont (13.0f, true);
    const int width = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, text)) + 22;
    juce::Image image (juce::Image::ARGB, width, 24, true);

    juce::Graphics g (image);
    const auto bounds = image.getBounds().toFloat().reduced (1.0f);
    g.setColour (Palette::deep.withAlpha (0.92f));
    g.fillRoundedRectangle (bounds, 11.0f);
    g.setColour (Palette::accent);
    g.drawRoundedRectangle (bounds, 11.0f, 1.5f);
    g.setColour (Palette::accentBright);
    g.setFont (font);
    g.drawText (text, bounds, juce::Justification::centred);

    return juce::ScaledImage (image);
}

//==============================================================================
DragGrip::DragGrip()
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void DragGrip::paint (juce::Graphics& g)
{
    // Шесть точек: привычный значок "можно тащить"
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (isMouseOver() ? Palette::accentBright : Palette::textFaint);

    for (int column = 0; column < 3; ++column)
        for (int row = 0; row < 2; ++row)
            g.fillEllipse (juce::Rectangle<float> (2.6f, 2.6f)
                               .withCentre ({ bounds.getCentreX() + (float) (column - 1) * 5.0f,
                                              bounds.getCentreY() + ((float) row - 0.5f) * 5.0f }));
}

void DragGrip::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 3 && onDragStart != nullptr)
        onDragStart (*this);
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
        addItems (box, names, { (int) S::velocity, (int) S::modWheel, (int) S::aftertouch, (int) S::slide, (int) S::key,
                                (int) S::random });
    }

    void addRange (juce::ComboBox& box, const juce::StringArray& names, ModDest first, ModDest last)
    {
        for (int d = (int) first; d <= (int) last; ++d)
            box.addItem (names[d], d + 1);
    }

    void fillDestinations (juce::ComboBox& box, const FxRack& rack, const LfoShapeBank& lfoShapes, Apvts& state, int selected)
    {
        const auto& names = Choices::modDestinations();
        using D = ModDest;
        box.addItem (names[0], 1);
        box.addSectionHeading ("Pitch");
        addItems (box, names, { (int) D::pitch, (int) D::osc1Pitch, (int) D::osc2Pitch, (int) D::osc3Pitch, (int) D::osc4Pitch });
        box.addSectionHeading ("Oscillators");
        addItems (box, names, { (int) D::osc1WtPos, (int) D::osc2WtPos, (int) D::osc3WtPos, (int) D::osc4WtPos,
                                (int) D::pulseWidth, (int) D::fm, (int) D::fold });
        addRange (box, names, D::osc1Fine, D::osc4Fine);
        box.addSectionHeading ("Mixer");
        addItems (box, names, { (int) D::osc1Level, (int) D::osc2Level, (int) D::osc3Level, (int) D::osc4Level,
                                (int) D::sub, (int) D::noise, (int) D::noiseColor, (int) D::ring });
        box.addSectionHeading ("Filter 1");
        addItems (box, names, { (int) D::cutoff, (int) D::resonance, (int) D::drive, (int) D::vowel,
                                (int) D::filter1EnvAmount, (int) D::filter1KeyTrack, (int) D::filter1Velocity });
        box.addSectionHeading ("Filter 2");
        addItems (box, names, { (int) D::filter2Cutoff, (int) D::filter2Resonance, (int) D::filter2Drive, (int) D::filter2Vowel,
                                (int) D::filter2EnvAmount, (int) D::filter2KeyTrack, (int) D::filter2Velocity });
        box.addSectionHeading ("Distortion");
        addItems (box, names, { (int) D::distDrive, (int) D::distMix, (int) D::distTone });
        box.addSectionHeading ("Envelopes");
        addRange (box, names, D::filterEnvAttack, D::filterEnvRelease);
        addRange (box, names, D::ampAttack, D::ampVelocity);
        box.addSectionHeading ("Amp");
        addItems (box, names, { (int) D::amp, (int) D::pan });
        box.addSectionHeading ("Voice");
        addRange (box, names, D::unisonDetune, D::vibrato);
        box.addSectionHeading ("Analog");
        addRange (box, names, D::drift, D::spread);
        addItems (box, names, { (int) D::sag, (int) D::warmup });
        box.addSectionHeading ("LFO Rate");
        for (int lfo = 0; lfo < kNumLfos; ++lfo)
            addItems (box, names, { (int) rateDestForLfo (lfo) });
        box.addSectionHeading ("Output");
        addItems (box, names, { (int) D::master });

        // Ручки рэка: подменю на каждый занятый слот, пункты подписаны по эффекту в слоте
        box.addSectionHeading ("Effects rack");
        bool selectedListed = false;

        for (int slot = 0; slot < kNumFxSlots; ++slot)
        {
            const auto type = rack.getType (slot);
            if (type == FxType::none)
                continue;

            const auto& info = getFxTypeInfo (type);
            juce::PopupMenu sub;

            for (int param = 0; param < (int) info.params.size(); ++param)
            {
                if (info.params[(size_t) param].isChoice())
                    continue;

                const int id = (int) fxDest (slot, param) + 1;
                sub.addItem (id, "FX " + juce::String (slot + 1) + " " + info.name + " " + info.params[(size_t) param].label);
                selectedListed |= id == selected;
            }

            box.getRootMenu()->addSubMenu ("Slot " + juce::String (slot + 1) + ": " + info.name, sub);
        }

        // Точки пользовательских форм LFO: подменю на каждый LFO
        box.addSectionHeading ("LFO shape points");

        for (int lfo = 0; lfo < kNumLfos; ++lfo)
        {
            if ((int) state.getRawParameterValue (ParamIDs::lfoShape (lfo))->load() != (int) LfoShape::custom)
                continue;

            juce::PopupMenu sub;
            const int points = juce::jmin (kMaxModulatedPoints, (int) lfoShapes.getPoints (lfo).size());

            for (int point = 0; point < points; ++point)
                for (bool position : { false, true })
                {
                    const int id = (int) lfoPointDest (lfo, point, position) + 1;
                    sub.addItem (id, "LFO " + juce::String (lfo + 1) + " Point " + juce::String (point + 1)
                                         + (position ? " time" : " height"));
                    selectedListed |= id == selected;
                }

            box.getRootMenu()->addSubMenu ("LFO " + juce::String (lfo + 1), sub);
        }

        // Выбранная цель, которой сейчас нет в списке (эффект убран, точка удалена, форма не своя):
        // пункт нужен, чтобы список показал её имя
        if (selected - 1 >= (int) D::fxFirst && ! selectedListed)
            box.addItem (names[selected - 1] + " (inactive)", selected);
    }
}

void PolarityButton::drawArrows (juce::Graphics& g, juce::Rectangle<float> area, ModPolarity polarity, juce::Colour colour)
{
    const auto centre = area.getCentre();
    const float half = juce::jmin (area.getWidth(), area.getHeight()) * 0.38f;
    const float head = half * 0.55f;

    juce::Path arrows;
    const auto arrow = [&] (float direction) // -1 вверх, +1 вниз
    {
        const float tip = centre.y + direction * half;
        arrows.startNewSubPath (centre.x, centre.y);
        arrows.lineTo (centre.x, tip);
        arrows.startNewSubPath (centre.x - head, tip - direction * head);
        arrows.lineTo (centre.x, tip);
        arrows.lineTo (centre.x + head, tip - direction * head);
    };

    if (polarity != ModPolarity::down)
        arrow (-1.0f);
    if (polarity != ModPolarity::up)
        arrow (1.0f);

    g.setColour (colour);
    g.strokePath (arrows, { 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
}

void PolarityButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? Palette::accentDim.withAlpha (0.5f) : Palette::deep);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (highlighted ? Palette::accent : Palette::outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
    drawArrows (g, bounds.reduced (3.0f), polarity, isEnabled() ? Palette::accentBright : Palette::textFaint);
}

//==============================================================================
ModSlotView::ModSlotView (Apvts& s, const FxRack& fxRack, const LfoShapeBank& shapes, int slotIndex)
    : state (s), rack (fxRack), lfoShapes (shapes), slot (slotIndex)
{
    fillSources (source);
    fillDestinationList();

    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    markBipolarAndDefault (amount, dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (ParamIDs::modAmount (slot))));

    clearButton.setTooltip ("Clear this slot");
    clearButton.onClick = [this] { clear(); };

    polarityButton.setTooltip ("Direction of the modulation from the knob position: both ways, up only or down only. Click to change");
    polarityButton.onClick = [this]
    {
        const int current = (int) state.getRawParameterValue (ParamIDs::modPolarity (slot))->load();
        polarityAttachment->setValueAsCompleteGesture ((float) ((current + 1) % 3));
    };

    addAndMakeVisible (source);
    addAndMakeVisible (destination);
    addAndMakeVisible (polarityButton);
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
    polarityAttachment = std::make_unique<juce::ParameterAttachment> (parameter (ParamIDs::modPolarity (slot)), [this] (float value)
    {
        polarityButton.setPolarity (static_cast<ModPolarity> (juce::jlimit (0, 2, juce::roundToInt (value))));
    });
    polarityAttachment->sendInitialUpdate();

    signature = targetsSignature();
    updateActiveState();
    startTimerHz (4);
}

juce::String ModSlotView::targetsSignature() const
{
    // Список целей зависит от эффектов в рэке и от точек пользовательских форм LFO
    juce::String result (rack.getVersion());
    for (int lfo = 0; lfo < kNumLfos; ++lfo)
    {
        const bool custom = (int) state.getRawParameterValue (ParamIDs::lfoShape (lfo))->load() == (int) LfoShape::custom;
        result << "," << (custom ? (int) lfoShapes.getPoints (lfo).size() : 0);
    }
    return result;
}

void ModSlotView::fillDestinationList()
{
    const int selected = juce::roundToInt (state.getRawParameterValue (ParamIDs::modDest (slot))->load()) + 1;
    destination.clear (juce::dontSendNotification);
    fillDestinations (destination, rack, lfoShapes, state, selected);
    destination.setSelectedId (selected, juce::dontSendNotification);
}

void ModSlotView::timerCallback()
{
    // В рэке сменились эффекты или у LFO сменилось число точек: список целей устарел.
    // Выбранная цель вне списка тоже требует пересборки, чтобы показать её имя.
    const auto current = targetsSignature();
    const int selected = juce::roundToInt (state.getRawParameterValue (ParamIDs::modDest (slot))->load()) + 1;

    if (current != signature || destination.getSelectedId() != selected)
    {
        signature = current;
        fillDestinationList();
    }
}

bool ModSlotView::isInUse() const
{
    return source.getSelectedId() > 1 || destination.getSelectedId() > 1;
}

void ModSlotView::clear()
{
    for (const auto& id : { ParamIDs::modSource (slot), ParamIDs::modDest (slot), ParamIDs::modAmount (slot),
                            ParamIDs::modPolarity (slot) })
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
    polarityButton.setAlpha (active ? 1.0f : 0.4f);
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
    area.removeFromLeft (8);
    polarityButton.setBounds (area.removeFromLeft (24).withSizeKeepingCentre (24, 24));
    area.removeFromLeft (8);
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

    // Экран меняется только вместе с ручками, поэтому и шейдеры считаются только тогда
    const bool shaders = shader.isAvailable() && Visuals::wantsShaders (*this);
    const int look = (shaders ? 2 : 0) + (Settings::get().crtScreen ? 1 : 0);

    if (changed || look != lastLook)
    {
        lastLook = look;

        if (shaders)
        {
            auto fx = Visuals::staticScreenFx (false);
            fx.cornerRadius = 5.0f;
            shader.render (*this, fx, [this] (juce::Graphics& g) { paintScreen (g, true); }, true);
        }
        else
        {
            shader.invalidate();
        }

        repaint();
    }
}

void EnvelopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    if (! shader.draw (g, getLocalBounds().toFloat()))
        paintScreen (g, false);

    g.setColour (Palette::outline.withAlpha (0.8f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
}

void EnvelopeView::paintScreen (juce::Graphics& g, bool forShader)
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

    if (! forShader)
        glass.draw (g, bounds, 5.0f);
}

//==============================================================================
VoiceLeds::VoiceLeds (SonderAudioProcessor& p)
    : processor (p)
{
    startTimerHz (30);
}

void VoiceLeds::timerCallback()
{
    const uint32_t active = processor.activeVoiceMask.load();
    bool changed = false;

    for (size_t i = 0; i < brightness.size(); ++i)
    {
        // Яркость по уровню голоса; вспыхивает сразу, гаснет плавно
        const float level = (active >> i) & 1u ? processor.displayVoiceLevel[i].load() : 0.0f;
        const float target = level > 1.0e-4f ? juce::jlimit (0.2f, 1.0f, std::sqrt (level) * 1.3f) : 0.0f;
        const float next = target > brightness[i] ? target : juce::jmax (target, brightness[i] * 0.8f);

        if (target > 0.0f)
        {
            const float note = processor.displayVoicePitch[i].load();
            changed |= std::abs (note - pitch[i]) > 0.25f;
            pitch[i] = note;
        }

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

    const auto base = Palette::accent.interpolatedWith (Palette::accentBright, 0.5f);
    const bool byPitch = Settings::get().timbreColour;

    const float step = area.getWidth() / (float) brightness.size();
    for (size_t i = 0; i < brightness.size(); ++i)
    {
        const auto centre = juce::Point<float> (area.getX() + step * ((float) i + 0.5f), area.getCentreY());
        const float b = brightness[i];

        // Две с половиной октавы вниз и вверх от до первой октавы
        const auto colour = byPitch ? shiftColour (base, juce::jlimit (-1.0f, 1.0f, (pitch[i] - 60.0f) / 30.0f)) : Palette::accentBright;

        if (b > 0.0f)
        {
            g.setColour (colour.withAlpha (0.3f * b));
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (centre));
        }

        g.setColour (Palette::track.interpolatedWith (colour, b));
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (centre));
    }
}

} // namespace sonder::ui
