#include "SonderLookAndFeel.h"
#include "Theme.h"

namespace sonder::ui
{

juce::Font makeFont (float height, bool bold, float kerning)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain))
               .withExtraKerningFactor (kerning);
}

SonderLookAndFeel::SonderLookAndFeel()
{
    applyPalette();
}

void SonderLookAndFeel::applyPalette()
{
    setColour (juce::ResizableWindow::backgroundColourId, Palette::background);

    setColour (juce::Slider::textBoxTextColourId, Palette::textDim);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Palette::accent.withAlpha (0.35f));

    setColour (juce::Label::textColourId, Palette::text);
    setColour (juce::Label::textWhenEditingColourId, Palette::text);
    setColour (juce::Label::backgroundWhenEditingColourId, Palette::deep);
    setColour (juce::Label::outlineWhenEditingColourId, Palette::accentDim);

    setColour (juce::ComboBox::backgroundColourId, Palette::deep);
    setColour (juce::ComboBox::outlineColourId, Palette::outline);
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::ComboBox::arrowColourId, Palette::accent);
    setColour (juce::ComboBox::focusedOutlineColourId, Palette::accentDim);

    setColour (juce::PopupMenu::backgroundColourId, Palette::panelTop);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::PopupMenu::headerTextColourId, Palette::accent);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::accent.withAlpha (0.22f));
    setColour (juce::PopupMenu::highlightedTextColourId, Palette::accentBright);

    setColour (juce::TextButton::buttonColourId, Palette::deep);
    setColour (juce::TextButton::buttonOnColourId, Palette::accentDim);
    setColour (juce::TextButton::textColourOffId, Palette::text);
    setColour (juce::TextButton::textColourOnId, Palette::accentBright);

    setColour (juce::TextEditor::backgroundColourId, Palette::deep);
    setColour (juce::TextEditor::textColourId, Palette::text);
    setColour (juce::TextEditor::outlineColourId, Palette::outline);
    setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent);
    setColour (juce::TextEditor::highlightColourId, Palette::accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, Palette::accent);

    setColour (juce::AlertWindow::backgroundColourId, Palette::panelTop);
    setColour (juce::AlertWindow::textColourId, Palette::text);
    setColour (juce::AlertWindow::outlineColourId, Palette::outline);

    setColour (juce::BubbleComponent::backgroundColourId, Palette::deep);
    setColour (juce::BubbleComponent::outlineColourId, Palette::accentDim);
    setColour (juce::TooltipWindow::backgroundColourId, Palette::deep);
    setColour (juce::TooltipWindow::textColourId, Palette::text);
    setColour (juce::TooltipWindow::outlineColourId, Palette::accentDim);
}

void SonderLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();

    const auto& properties = slider.getProperties();

    // Пока ручка плавно едет к значению нового пресета, рисуем её промежуточное положение
    if (const float animated = properties.getWithDefault ("animPos", -1.0f); animated >= 0.0f)
        sliderPos = animated;

    const float arcRadius = size * 0.5f - 4.0f;
    const float lineWidth = juce::jmax (2.5f, size * 0.055f);
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Дорожка
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (Palette::track);
    g.strokePath (track, { lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    // Значение: у биполярных параметров дуга идёт от центра
    const bool bipolar = slider.getProperties().getWithDefault ("bipolar", false);
    const float fromAngle = bipolar ? (rotaryStartAngle + rotaryEndAngle) * 0.5f : rotaryStartAngle;

    if (std::abs (angle - fromAngle) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);

        g.setColour (Palette::accent.withAlpha (0.16f));
        g.strokePath (value, { lineWidth * 2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        g.setColour (Palette::accent);
        g.strokePath (value, { lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }

    // Модуляция: кольцо диапазона между дорожкой и корпусом и точка живого значения
    if ((bool) properties.getWithDefault ("modActive", false))
    {
        const float ringRadius = arcRadius - lineWidth * 0.5f - 2.2f;
        const auto angleFor = [&] (float normalised) { return rotaryStartAngle + normalised * (rotaryEndAngle - rotaryStartAngle); };

        // Дуга "дышит": светящийся отрезок от положения ручки до живого значения
        if (const float live = properties.getWithDefault ("modLive", -1.0f); live >= 0.0f && Settings::get().motion)
        {
            const float liveAngle = angleFor (live);

            if (std::abs (liveAngle - angle) > 0.02f)
            {
                juce::Path breath;
                breath.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                      juce::jmin (angle, liveAngle), juce::jmax (angle, liveAngle), true);

                const float strength = juce::jmin (1.0f, std::abs (live - sliderPos) * 4.0f);
                g.setColour (Palette::accentBright.withAlpha (0.10f + 0.16f * strength));
                g.strokePath (breath, { lineWidth * 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
                g.setColour (Palette::accentBright.withAlpha (0.55f));
                g.strokePath (breath, { lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
            }
        }
        const float a0 = angleFor ((float) properties.getWithDefault ("modMin", 0.0f));
        const float a1 = angleFor ((float) properties.getWithDefault ("modMax", 0.0f));

        if (std::abs (a1 - a0) > 0.02f)
        {
            juce::Path ring;
            ring.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
            g.setColour (Palette::accentBright.withAlpha (0.85f));
            g.strokePath (ring, { 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        }

        // Модулятор, который сейчас редактируется Alt+drag, выделен белой дугой поверх общего диапазона
        if ((bool) properties.getWithDefault ("modEdit", false))
        {
            const float e0 = angleFor ((float) properties.getWithDefault ("modEditMin", 0.0f));
            const float e1 = angleFor ((float) properties.getWithDefault ("modEditMax", 0.0f));

            if (std::abs (e1 - e0) > 0.02f)
            {
                juce::Path edit;
                edit.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f, juce::jmin (e0, e1), juce::jmax (e0, e1), true);
                g.setColour (juce::Colours::white);
                g.strokePath (edit, { 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
            }
        }

        const float live = properties.getWithDefault ("modLive", -1.0f);
        if (live >= 0.0f)
        {
            const float a = angleFor (live);
            const juce::Point<float> dot (centre.x + ringRadius * std::sin (a), centre.y - ringRadius * std::cos (a));
            g.setColour (Palette::accent.withAlpha (0.4f));
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (dot));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (dot));
        }
    }

    // Сюда можно бросить источник модуляции
    if ((bool) properties.getWithDefault ("dropTarget", false))
    {
        g.setColour (Palette::accent.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (arcRadius * 2.0f + 6.0f, arcRadius * 2.0f + 6.0f).withCentre (centre));
    }

    // Корпус ручки
    const float bodyRadius = arcRadius - lineWidth - 3.5f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (body.translated (0.0f, 2.0f).expanded (1.0f));

    g.setGradientFill (juce::ColourGradient (Palette::knobTop, centre.x, body.getY(),
                                             Palette::knobBottom, centre.x, body.getBottom(), false));
    g.fillEllipse (body);

    g.setColour (Palette::knobEdge);
    g.drawEllipse (body.reduced (0.5f), 1.0f);

    // Указатель
    juce::Path pointer;
    const float pointerWidth = juce::jmax (2.0f, size * 0.04f);
    pointer.addRoundedRectangle (-pointerWidth * 0.5f, -bodyRadius * 0.86f, pointerWidth, bodyRadius * 0.46f, pointerWidth * 0.5f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));

    g.setColour (slider.isEnabled() ? Palette::accentBright : Palette::textFaint);
    g.fillPath (pointer);
}

void SonderLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }

    // Горизонтальная полоса: заливка от нуля (для биполярных от центра) и значение текстом поверх
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (0.0f, 3.0f);
    g.setColour (Palette::deep);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    const bool bipolar = slider.getProperties().getWithDefault ("bipolar", false);
    const float zeroX = bipolar ? bounds.getCentreX() : bounds.getX();
    const float left = juce::jmin (zeroX, sliderPos);
    const float right = juce::jmax (zeroX, sliderPos);

    if (right - left > 0.5f)
    {
        const auto fill = juce::Rectangle<float> (left, bounds.getY(), right - left, bounds.getHeight()).reduced (0.0f, 2.0f);
        g.setColour (Palette::accent.withAlpha (0.28f));
        g.fillRoundedRectangle (fill, 3.0f);
        g.setColour (Palette::accent);
        g.fillRect (juce::Rectangle<float> (sliderPos - 1.0f, fill.getY(), 2.0f, fill.getHeight()));
    }

    if (bipolar)
    {
        g.setColour (Palette::textFaint);
        g.fillRect (juce::Rectangle<float> (zeroX - 0.5f, bounds.getY() + 3.0f, 1.0f, bounds.getHeight() - 6.0f));
    }

    g.setColour (slider.isEnabled() ? Palette::text : Palette::textFaint);
    g.setFont (makeFont (12.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), bounds, juce::Justification::centred);
}

void SonderLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);

    g.setColour (Palette::deep);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.isMouseOver (true) ? Palette::accentDim : Palette::outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    // Шеврон
    const float arrowX = (float) width - 12.0f;
    const float arrowY = (float) height * 0.5f;
    juce::Path arrow;
    arrow.startNewSubPath (arrowX - 3.5f, arrowY - 1.5f);
    arrow.lineTo (arrowX, arrowY + 2.0f);
    arrow.lineTo (arrowX + 3.5f, arrowY - 1.5f);

    g.setColour (box.isEnabled() ? Palette::accent : Palette::textFaint);
    g.strokePath (arrow, { 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
}

juce::Font SonderLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return makeFont (13.0f);
}

void SonderLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (5, 1, box.getWidth() - 19, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setMinimumHorizontalScale (0.6f); // в узких ячейках текст сжимается, а не обрезается
}

juce::Font SonderLookAndFeel::getPopupMenuFont()
{
    return makeFont (14.0f);
}

void SonderLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                              bool isMouseOverButton, bool isButtonDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    g.setColour (isButtonDown ? Palette::accentDim.withAlpha (0.5f)
                              : (on ? Palette::accent.withAlpha (0.18f) : Palette::deep));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (on || isMouseOverButton ? Palette::accent : Palette::outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
}

juce::Font SonderLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return makeFont (juce::jmin (14.0f, (float) buttonHeight * 0.55f), true, 0.08f);
}

juce::Label* SonderLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (makeFont (12.0f));
    label->setColour (juce::Label::textColourId, Palette::textDim);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return label;
}

void SonderLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    // Без рамок: рамка только в режиме ввода значения
    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (label.isBeingEdited())
        return;

    const float alpha = label.isEnabled() ? 1.0f : 0.5f;
    g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
    g.setFont (label.getFont());
    g.drawFittedText (label.getText(), getLabelBorderSize (label).subtractedFrom (label.getLocalBounds()),
                      label.getJustificationType(), 1, label.getMinimumHorizontalScale());
}

} // namespace sonder::ui
