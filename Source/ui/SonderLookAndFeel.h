#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace sonder::ui
{

namespace Palette
{
    const juce::Colour background   { 0xff0a1114 };
    const juce::Colour headerTop    { 0xff122026 };
    const juce::Colour panelTop     { 0xff15242a };
    const juce::Colour panelBottom  { 0xff0f191e };
    const juce::Colour outline      { 0xff1f3239 };
    const juce::Colour deep         { 0xff060d10 };
    const juce::Colour track        { 0xff1a2b31 };
    const juce::Colour knobTop      { 0xff273c44 };
    const juce::Colour knobBottom   { 0xff111c21 };
    const juce::Colour knobEdge     { 0xff2f4851 };
    const juce::Colour accent       { 0xff2fb8c8 };
    const juce::Colour accentBright { 0xff8ae8f2 };
    const juce::Colour accentDim    { 0xff1c6f79 };
    const juce::Colour text         { 0xffd3e5e8 };
    const juce::Colour textDim      { 0xff7b959c };
    const juce::Colour textFaint    { 0xff4a6168 };
}

juce::Font makeFont (float height, bool bold = false, float kerning = 0.0f);

class SonderLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SonderLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getPopupMenuFont() override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
};

} // namespace sonder::ui
