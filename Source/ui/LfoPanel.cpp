#include "LfoPanel.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "synth/SynthParams.h"

namespace sonder::ui
{

namespace
{
    const juce::Identifier visibleLfosProperty { "visibleLfos" };
    constexpr int kTabsX = 60, kTabWidth = 26, kTabHeight = 20, kTabGap = 4;
    constexpr int kCellWidth = 72, kCellHeight = 94;
}

LfoPanel::LfoPanel (SonderAudioProcessor& p)
    : processor (p),
      dragHandle ([this] { return sourceForLfo (selected); }),
      editor (p)
{
    for (int i = 0; i < kNumLfos; ++i)
    {
        auto& tab = tabs[(size_t) i];
        tab.lfoIndex = i;
        tab.setButtonText (juce::String (i + 1));
        tab.setTooltip ("LFO " + juce::String (i + 1) + ": click to edit, drag onto a knob to modulate it");
        tab.setClickingTogglesState (true);
        tab.setRadioGroupId (0x1f0);
        // В радиогруппе JUCE вызывает onClick и у вкладки, которую выключает.
        // Реагируем только на включение, иначе старая вкладка снова выберет себя.
        tab.onClick = [this, i]
        {
            if (tabs[(size_t) i].getToggleState())
                selectLfo (i);
        };
        addChildComponent (tab);
    }

    addButton.setTooltip ("Add LFO");
    addButton.onClick = [this]
    {
        const int count = juce::jmin (kNumLfos, visibleCount() + 1);
        processor.parameters.state.setProperty (visibleLfosProperty, count, nullptr);
        timerCallback();
        selectLfo (count - 1);
    };

    dragHandle.setTooltip ("Drag onto a knob to modulate it with the selected LFO");
    addAndMakeVisible (dragHandle);
    addAndMakeVisible (addButton);
    addAndMakeVisible (editor);

    selectLfo (0);
    timerCallback();
    startTimerHz (5);
}

namespace
{
    bool lfoUsed (SonderAudioProcessor& processor, int index)
    {
        const auto source = (float) (int) sourceForLfo (index);

        for (int slot = 0; slot < kNumModSlots; ++slot)
            if (processor.parameters.getRawParameterValue (ParamIDs::modSource (slot))->load() == source)
                return true;

        return false;
    }
}

bool LfoPanel::isLfoUsed (int index) const
{
    return lfoUsed (processor, index);
}

int LfoPanel::visibleLfoCount (SonderAudioProcessor& processor)
{
    int count = juce::jmax (2, (int) processor.parameters.state.getProperty (visibleLfosProperty, 2));

    for (int i = count; i < kNumLfos; ++i)
        if (lfoUsed (processor, i))
            count = i + 1;

    return juce::jmin (kNumLfos, count);
}

int LfoPanel::visibleCount() const
{
    return visibleLfoCount (processor);
}

void LfoPanel::selectLfo (int index)
{
    selected = index;
    tabs[(size_t) index].setToggleState (true, juce::dontSendNotification);
    editor.setLfo (index);

    shapeControl = std::make_unique<ParameterControl> (processor, ParamIDs::lfoShape (index), "Shape");
    modeControl = std::make_unique<ParameterControl> (processor, ParamIDs::lfoMode (index), "Mode");
    rateControl = std::make_unique<ParameterControl> (processor, ParamIDs::lfoRate (index), "Rate");
    syncControl = std::make_unique<ParameterControl> (processor, ParamIDs::lfoSync (index), "Sync");

    modeControl->setTooltipText ("Free: runs continuously, shared by all voices\n"
                                 "Retrig: restarts on every note\n"
                                 "Env: plays once per note and holds the last value");

    for (auto* control : { shapeControl.get(), modeControl.get(), rateControl.get(), syncControl.get() })
        addAndMakeVisible (control);

    resized();
    timerCallback();
}

void LfoPanel::timerCallback()
{
    const int count = visibleCount();
    if (count != shownCount)
    {
        shownCount = count;
        for (int i = 0; i < kNumLfos; ++i)
            tabs[(size_t) i].setVisible (i < count);

        addButton.setVisible (count < kNumLfos);
        resized();
    }

    // Ручка частоты не действует, пока включена синхронизация с темпом
    if (rateControl != nullptr)
    {
        const bool synced = processor.parameters.getRawParameterValue (ParamIDs::lfoSync (selected))->load() > 0.5f;
        rateControl->setAlpha (synced ? 0.35f : 1.0f);
    }

    repaint();
}

void LfoPanel::resized()
{
    for (int i = 0; i < kNumLfos; ++i)
        tabs[(size_t) i].setBounds (kTabsX + i * (kTabWidth + kTabGap), 3, kTabWidth, kTabHeight);

    addButton.setBounds (kTabsX + shownCount * (kTabWidth + kTabGap), 3, kTabWidth, kTabHeight);
    dragHandle.setBounds (getWidth() - 8 - 22, 3, 22, kTabHeight);

    auto area = getLocalBounds().reduced (8, 0).withTrimmedTop (30).withTrimmedBottom (8);
    auto controls = area.removeFromRight (kCellWidth * 2);
    area.removeFromRight (10);
    editor.setBounds (area);

    const auto cell = [&controls] (int column, int row)
    {
        return juce::Rectangle<int> (controls.getX() + column * kCellWidth, controls.getY() + row * kCellHeight,
                                     kCellWidth, kCellHeight);
    };

    if (shapeControl != nullptr)
    {
        shapeControl->setBounds (cell (0, 0));
        modeControl->setBounds (cell (1, 0));
        rateControl->setBounds (cell (0, 1));
        syncControl->setBounds (cell (1, 1));
    }
}

void LfoPanel::paintOverChildren (juce::Graphics& g)
{
    for (int i = 0; i < shownCount; ++i)
    {
        if (! isLfoUsed (i))
            continue;

        const auto tab = tabs[(size_t) i].getBounds().toFloat();
        g.setColour (Palette::accentBright);
        g.fillEllipse (tab.getRight() - 6.0f, tab.getY() + 3.0f, 3.5f, 3.5f);
    }
}

} // namespace sonder::ui
