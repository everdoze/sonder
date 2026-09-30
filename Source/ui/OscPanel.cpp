#include "OscPanel.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "Theme.h"

namespace sonder::ui
{

namespace
{
    constexpr int kTabsX = 56, kTabWidth = 26, kTabHeight = 20, kTabGap = 4;
    constexpr int kTitleHeight = 26, kDisplayHeight = 86;
    constexpr int kCellWidth = 72, kCellHeight = 94;
    constexpr int kPulseShape = 1, kWavetableShape = 4;
}

OscPanel::OscPanel (SonderAudioProcessor& p, const Visuals& visuals)
    : processor (p),
      view (p, visuals, 0)
{
    for (int i = 0; i < kNumOscs; ++i)
    {
        auto& tab = tabs[(size_t) i];
        tab.setButtonText (juce::String (i + 1));
        tab.setTooltip ("Oscillator " + juce::String (i + 1));
        tab.setClickingTogglesState (true);
        tab.setRadioGroupId (0x05c);
        // В радиогруппе JUCE вызывает onClick и у вкладки, которую выключает: реагируем только на включение
        tab.onClick = [this, i]
        {
            if (tabs[(size_t) i].getToggleState())
                selectOsc (i);
        };
        addAndMakeVisible (tab);
    }

    onButton.setClickingTogglesState (true);
    onButton.setTooltip ("Switch this oscillator on or off. An oscillator that is off costs no CPU");
    addAndMakeVisible (onButton);

    syncButton.setClickingTogglesState (true);
    syncButton.setTooltip ("Hard sync: this oscillator restarts its cycle every time oscillator 1 does. "
                           "Tune it higher (Semi, or modulate its pitch) for the classic sync sweep");
    addChildComponent (syncButton);
    addAndMakeVisible (view);

    selectOsc (juce::jlimit (0, kNumOscs - 1, Settings::get().oscTab));
    startTimerHz (10);
}

bool OscPanel::isOn (int osc) const
{
    return processor.parameters.getRawParameterValue (ParamIDs::oscOn (osc))->load() > 0.5f;
}

std::array<ParameterControl*, 6> OscPanel::controls() const
{
    return { shapeControl.get(), positionControl.get(), widthControl.get(),
             semiControl.get(), fineControl.get(), levelControl.get() };
}

void OscPanel::selectOsc (int index)
{
    selected = index;
    Settings::get().oscTab = index;
    tabs[(size_t) index].setToggleState (true, juce::dontSendNotification);
    view.setOscillator (index);

    onAttachment.reset();
    onAttachment = std::make_unique<Apvts::ButtonAttachment> (processor.parameters, ParamIDs::oscOn (index), onButton);

    // Первый осциллятор - ведущий, синхронизироваться ему не с кем
    syncAttachment.reset();
    if (index > 0)
        syncAttachment = std::make_unique<Apvts::ButtonAttachment> (processor.parameters, ParamIDs::oscSync (index), syncButton);
    syncButton.setVisible (index > 0);

    shapeControl = std::make_unique<ParameterControl> (processor, ParamIDs::oscShape (index), "Shape");
    positionControl = std::make_unique<ParameterControl> (processor, ParamIDs::oscWtPos (index), "WT Pos");
    widthControl = std::make_unique<ParameterControl> (processor, ParamIDs::oscPw (index), "PW");
    semiControl = std::make_unique<ParameterControl> (processor, ParamIDs::oscSemi (index), "Semi");
    fineControl = std::make_unique<ParameterControl> (processor, ParamIDs::oscFine (index), "Fine");
    levelControl = std::make_unique<ParameterControl> (processor, ParamIDs::oscLevel (index), "Level");

    positionControl->setTooltipText ("Wavetable position: morphs through the frames of the table");
    widthControl->setTooltipText ("Pulse width of the Pulse shape");
    levelControl->setTooltipText ("Level of this oscillator in the mix");

    for (auto* control : controls())
        addAndMakeVisible (control);

    resized();
    timerCallback();
}

void OscPanel::beginMorph()
{
    for (auto* control : controls())
        if (control != nullptr)
            control->beginMorph();
}

void OscPanel::timerCallback()
{
    if (shapeControl == nullptr)
        return;

    // Ручки, которые для текущей формы ничего не меняют, и весь выключенный осциллятор - тусклее
    const int shape = (int) processor.parameters.getRawParameterValue (ParamIDs::oscShape (selected))->load();
    const float base = isOn (selected) ? 1.0f : 0.45f;

    for (auto* control : controls())
    {
        const bool relevant = (control != positionControl.get() || shape == kWavetableShape)
                           && (control != widthControl.get() || shape == kPulseShape);
        const float alpha = relevant ? base : base * 0.4f;

        if (control->getAlpha() != alpha)
            control->setAlpha (alpha);
    }

    repaint();
}

void OscPanel::resized()
{
    for (int i = 0; i < kNumOscs; ++i)
        tabs[(size_t) i].setBounds (kTabsX + i * (kTabWidth + kTabGap), 3, kTabWidth, kTabHeight);

    onButton.setBounds (getWidth() - 8 - 44, 3, 44, kTabHeight);
    syncButton.setBounds (onButton.getX() - 8 - 52, 3, 52, kTabHeight);

    auto area = getLocalBounds().reduced (8, 0).withTrimmedTop (kTitleHeight).withTrimmedBottom (6);
    view.setBounds (area.removeFromTop (kDisplayHeight));
    area.removeFromTop (4);

    const auto all = controls();
    const int cellHeight = juce::jmin (kCellHeight, area.getHeight());
    const int x = area.getX() + (area.getWidth() - kCellWidth * (int) all.size()) / 2;

    for (size_t i = 0; i < all.size(); ++i)
        if (all[i] != nullptr)
            all[i]->setBounds (x + (int) i * kCellWidth, area.getY(), kCellWidth, cellHeight);
}

void OscPanel::paintOverChildren (juce::Graphics& g)
{
    for (int i = 0; i < kNumOscs; ++i)
    {
        if (! isOn (i))
            continue;

        const auto tab = tabs[(size_t) i].getBounds().toFloat();
        g.setColour (Palette::accentBright);
        g.fillEllipse (tab.getRight() - 6.0f, tab.getY() + 3.0f, 3.5f, 3.5f);
    }
}

} // namespace sonder::ui
