#include "Controls.h"
#include "Parameters.h"
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
ParameterControl::ParameterControl (Apvts& state, const juce::String& parameterID, const juce::String& labelText, Style s)
    : style (s)
{
    label.setText (labelText.toUpperCase(), juce::dontSendNotification);
    label.setFont (makeFont (10.5f, true, 0.1f));
    label.setColour (juce::Label::textColourId, Palette::textDim);
    label.setJustificationType (style == Style::standard ? juce::Justification::centred : juce::Justification::centredLeft);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    auto* parameter = state.getParameter (parameterID);
    jassert (parameter != nullptr);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
    {
        comboBox = std::make_unique<juce::ComboBox>();
        comboBox->addItemList (choice->choices, 1);
        addAndMakeVisible (*comboBox);
        comboBoxAttachment = std::make_unique<Apvts::ComboBoxAttachment> (state, parameterID, *comboBox);
        return;
    }

    slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    markBipolarAndDefault (*slider, dynamic_cast<juce::RangedAudioParameter*> (parameter));
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
        slider->setBounds (area);

    if (comboBox != nullptr)
    {
        area.removeFromBottom (12);
        comboBox->setBounds (area.withSizeKeepingCentre (area.getWidth() - 6, 24));
    }
}

void ParameterControl::setTooltipText (const juce::String& text)
{
    if (slider != nullptr)
        slider->setTooltip (text);

    if (comboBox != nullptr)
        comboBox->setTooltip (text);
}

void ParameterControl::paint (juce::Graphics&) {}

//==============================================================================
ModSlotView::ModSlotView (Apvts& state, int slotIndex)
    : slot (slotIndex)
{
    source.addItemList (Choices::modSources(), 1);
    destination.addItemList (Choices::modDestinations(), 1);

    amount.setSliderStyle (juce::Slider::LinearHorizontal);
    amount.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    markBipolarAndDefault (amount, dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (ParamIDs::modAmount (slot))));

    addAndMakeVisible (source);
    addAndMakeVisible (destination);
    addAndMakeVisible (amount);

    sourceAttachment = std::make_unique<Apvts::ComboBoxAttachment> (state, ParamIDs::modSource (slot), source);
    destinationAttachment = std::make_unique<Apvts::ComboBoxAttachment> (state, ParamIDs::modDest (slot), destination);
    amountAttachment = std::make_unique<Apvts::SliderAttachment> (state, ParamIDs::modAmount (slot), amount);

    source.onChange = [this] { updateActiveState(); };
    destination.onChange = [this] { updateActiveState(); };
    updateActiveState();
}

void ModSlotView::updateActiveState()
{
    const bool active = source.getSelectedItemIndex() > 0 && destination.getSelectedItemIndex() > 0;
    amount.setAlpha (active ? 1.0f : 0.4f);
    repaint();
}

void ModSlotView::paint (juce::Graphics& g)
{
    const bool active = source.getSelectedItemIndex() > 0 && destination.getSelectedItemIndex() > 0;
    const auto numberArea = getLocalBounds().removeFromLeft (20).toFloat();

    g.setColour (active ? Palette::accent : Palette::textFaint);
    g.setFont (makeFont (12.0f, true));
    g.drawText (juce::String (slot + 1), numberArea, juce::Justification::centred);

    // Стрелка между источником и целью
    const float arrowX = (float) source.getRight() + 7.0f;
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
    area.removeFromLeft (20);

    const int comboWidth = (area.getWidth() - 14 - 8) * 36 / 100;
    source.setBounds (area.removeFromLeft (comboWidth).withSizeKeepingCentre (comboWidth, 24));
    area.removeFromLeft (14);
    destination.setBounds (area.removeFromLeft (comboWidth).withSizeKeepingCentre (comboWidth, 24));
    area.removeFromLeft (8);
    amount.setBounds (area.withSizeKeepingCentre (area.getWidth(), 26));
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
LfoView::LfoView (Apvts& state, const char* shapeID, const std::atomic<float>& phaseSource)
    : shape (state.getRawParameterValue (shapeID)),
      phase (phaseSource)
{
    startTimerHz (30);
}

float LfoView::shapeValue (int shapeIndex, float p)
{
    // Для случайных форм показываем фиксированный "образец"
    static constexpr float randomSteps[] { 0.2f, -0.7f, 0.9f, -0.1f, 0.55f, -0.9f, 0.35f, -0.4f };

    switch (shapeIndex)
    {
        case 0: return fastSinCycles (p);
        case 1: return 1.0f - 4.0f * std::abs (p - 0.5f);
        case 2: return 2.0f * p - 1.0f;
        case 3: return 1.0f - 2.0f * p;
        case 4: return p < 0.5f ? 1.0f : -1.0f;
        case 5: return randomSteps[juce::jlimit (0, 7, (int) (p * 8.0f))];
        case 6:
        {
            const float position = p * 8.0f;
            const int index = juce::jlimit (0, 7, (int) position);
            const float t = 0.5f - 0.5f * std::cos ((position - (float) index) * juce::MathConstants<float>::pi);
            return randomSteps[index] + (randomSteps[(index + 1) % 8] - randomSteps[index]) * t;
        }
        default: return 0.0f;
    }
}

void LfoView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    fillDisplayBackground (g, bounds);

    const auto plot = bounds.reduced (8.0f, 8.0f);
    const int shapeIndex = shape != nullptr ? (int) shape->load() : 0;
    const auto toPoint = [&plot] (float p, float value)
    {
        return juce::Point<float> (plot.getX() + p * plot.getWidth(), plot.getCentreY() - value * plot.getHeight() * 0.5f);
    };

    g.setColour (Palette::outline);
    g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

    juce::Path path;
    constexpr int steps = 160;
    for (int i = 0; i <= steps; ++i)
    {
        const float p = juce::jmin ((float) i / steps, 0.9999f);
        const auto point = toPoint (p, shapeValue (shapeIndex, p));
        if (i == 0)
            path.startNewSubPath (point);
        else
            path.lineTo (point);
    }

    g.setColour (Palette::accent.withAlpha (0.55f));
    g.strokePath (path, { 1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    const float currentPhase = phase.load();
    const auto dot = toPoint (currentPhase, shapeValue (shapeIndex, currentPhase));

    g.setColour (Palette::accent.withAlpha (0.18f));
    g.fillRect (juce::Rectangle<float> (dot.x - 0.5f, plot.getY(), 1.0f, plot.getHeight()));
    g.setColour (Palette::accent.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (dot));
    g.setColour (Palette::accentBright);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (dot));
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
