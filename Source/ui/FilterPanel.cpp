#include "FilterPanel.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "Theme.h"
#include "synth/SynthParams.h"

namespace sonder::ui
{

namespace
{
    constexpr int kTabsX = 72, kTabWidth = 26, kTabHeight = 20, kTabGap = 4;
    constexpr int kTitleHeight = 26, kDisplayHeight = 94;
    constexpr int kCellWidth = 72, kCellHeight = 94;
}

//==============================================================================
FilterPanel::ModeControl::ModeControl (SonderAudioProcessor& processor, int filter)
{
    label.setText ("MODE", juce::dontSendNotification);
    label.setFont (makeFont (10.5f, true, 0.1f));
    label.setColour (juce::Label::textColourId, Palette::textDim);
    label.setJustificationType (juce::Justification::centred);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    // Режимы по типам; ID пункта - номер режима в параметре плюс один
    const auto& names = Choices::filterModes();
    const auto add = [&] (std::initializer_list<int> indices)
    {
        for (int index : indices)
            box.addItem (names[index], index + 1);
    };

    box.addSectionHeading ("Low pass");
    add ({ 0, 1 });
    box.addSectionHeading ("High pass");
    add ({ 3, 5 });
    box.addSectionHeading ("Band pass");
    add ({ 6, 2 });
    box.addSectionHeading ("Other");
    add ({ 7, kVowelFilterMode });

    box.setTooltip ("LP lets lows through, HP lets highs through, BP keeps a band around the cutoff, Notch removes one.\n"
                    "24 - steep (4 poles), 12 - gentle (2 poles).\n"
                    "Vowel turns the filter into a formant 'talking' filter: Cutoff shifts the formants, Vowel picks the vowel");
    addAndMakeVisible (box);

    auto& parameter = *processor.parameters.getParameter (ParamIDs::filterParam (filter, FilterParam::mode));
    attachment = std::make_unique<GroupedChoiceAttachment> (parameter, box);
}

void FilterPanel::ModeControl::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    area.removeFromBottom (12);
    box.setBounds (area.withSizeKeepingCentre (area.getWidth() - 6, 24));
}

//==============================================================================
FilterPanel::FilterPanel (SonderAudioProcessor& p, Visuals& visuals)
    : processor (p),
      view (p, visuals)
{
    for (int i = 0; i < kNumFilters; ++i)
    {
        auto& tab = tabs[(size_t) i];
        tab.setButtonText (juce::String (i + 1));
        tab.setTooltip ("Filter " + juce::String (i + 1));
        tab.setClickingTogglesState (true);
        tab.setRadioGroupId (0x0f1);
        // В радиогруппе JUCE вызывает onClick и у вкладки, которую выключает: реагируем только на включение
        tab.onClick = [this, i]
        {
            if (tabs[(size_t) i].getToggleState())
                selectFilter (i);
        };
        addAndMakeVisible (tab);
    }

    onButton.setClickingTogglesState (true);
    onButton.setTooltip ("Switch this filter on or off. A filter that is off lets the sound through untouched");
    addAndMakeVisible (onButton);

    // Соединение фильтров: кнопка показывает текущее, клик переключает
    routingButton.setTooltip ("SERIAL: the sound goes through filter 1, then filter 2.\n"
                              "PARALLEL: both filters get the same sound and their outputs are mixed");
    routingAttachment = std::make_unique<juce::ParameterAttachment> (*p.parameters.getParameter (ParamIDs::filterRouting), [this] (float value)
    {
        routingButton.setButtonText (value > 0.5f ? "PARALLEL" : "SERIAL");
    });
    routingButton.onClick = [this]
    {
        const bool parallel = processor.parameters.getRawParameterValue (ParamIDs::filterRouting)->load() > 0.5f;
        routingAttachment->setValueAsCompleteGesture (parallel ? 0.0f : 1.0f);
    };
    routingAttachment->sendInitialUpdate();
    addAndMakeVisible (routingButton);

    addAndMakeVisible (view);

    selectFilter (juce::jlimit (0, kNumFilters - 1, Settings::get().filterTab));
    startTimerHz (10);
}

bool FilterPanel::isOn (int filter) const
{
    return processor.parameters.getRawParameterValue (ParamIDs::filterParam (filter, FilterParam::on))->load() > 0.5f;
}

std::array<ParameterControl*, 7> FilterPanel::knobs() const
{
    return { cutoffControl.get(), resonanceControl.get(), driveControl.get(), vowelControl.get(),
             envControl.get(), keyTrackControl.get(), velocityControl.get() };
}

void FilterPanel::selectFilter (int index)
{
    selected = index;
    Settings::get().filterTab = index;
    tabs[(size_t) index].setToggleState (true, juce::dontSendNotification);
    view.setSelectedFilter (index);

    onAttachment.reset();
    onAttachment = std::make_unique<Apvts::ButtonAttachment> (processor.parameters, ParamIDs::filterParam (index, FilterParam::on), onButton);

    const auto id = [index] (FilterParam param) { return ParamIDs::filterParam (index, param); };
    modeControl = std::make_unique<ModeControl> (processor, index);
    cutoffControl = std::make_unique<ParameterControl> (processor, id (FilterParam::cutoff), "Cutoff");
    resonanceControl = std::make_unique<ParameterControl> (processor, id (FilterParam::resonance), "Reso");
    driveControl = std::make_unique<ParameterControl> (processor, id (FilterParam::drive), "Drive");
    vowelControl = std::make_unique<ParameterControl> (processor, id (FilterParam::vowel), "Vowel");
    envControl = std::make_unique<ParameterControl> (processor, id (FilterParam::envAmount), "Env Amt");
    keyTrackControl = std::make_unique<ParameterControl> (processor, id (FilterParam::keyTrack), "Key Trk");
    velocityControl = std::make_unique<ParameterControl> (processor, id (FilterParam::velocity), "Vel");

    vowelControl->setTooltipText ("Vowel of the formant filter (A-E-I-O-U), works in Vowel mode");
    envControl->setTooltipText ("How far the filter envelope moves the cutoff; negative closes the filter");

    addAndMakeVisible (*modeControl);
    for (auto* knob : knobs())
        addAndMakeVisible (knob);

    resized();
    timerCallback();
}

void FilterPanel::beginMorph()
{
    for (auto* knob : knobs())
        if (knob != nullptr)
            knob->beginMorph();
}

void FilterPanel::timerCallback()
{
    if (vowelControl == nullptr)
        return;

    // Выключенный фильтр целиком тусклее; ручка Vowel работает только в режиме Vowel
    const bool vowelMode = (int) processor.parameters.getRawParameterValue (ParamIDs::filterParam (selected, FilterParam::mode))->load()
                           == kVowelFilterMode;
    const float base = isOn (selected) ? 1.0f : 0.45f;

    for (juce::Component* control : { (juce::Component*) modeControl.get(), (juce::Component*) cutoffControl.get(),
                                      (juce::Component*) resonanceControl.get(), (juce::Component*) driveControl.get(),
                                      (juce::Component*) vowelControl.get(), (juce::Component*) envControl.get(),
                                      (juce::Component*) keyTrackControl.get(), (juce::Component*) velocityControl.get() })
    {
        const float alpha = control == vowelControl.get() && ! vowelMode ? base * 0.4f : base;
        if (control->getAlpha() != alpha)
            control->setAlpha (alpha);
    }

    repaint();
}

void FilterPanel::resized()
{
    for (int i = 0; i < kNumFilters; ++i)
        tabs[(size_t) i].setBounds (kTabsX + i * (kTabWidth + kTabGap), 3, kTabWidth, kTabHeight);

    onButton.setBounds (getWidth() - 8 - 44, 3, 44, kTabHeight);
    routingButton.setBounds (onButton.getX() - 8 - 80, 3, 80, kTabHeight);

    auto area = getLocalBounds().reduced (8, 0).withTrimmedTop (kTitleHeight).withTrimmedBottom (6);
    view.setBounds (area.removeFromTop (kDisplayHeight));
    area.removeFromTop (4);

    const int cellWidth = juce::jmin (kCellWidth, area.getWidth() / 8);
    const int cellHeight = juce::jmin (kCellHeight, area.getHeight());
    const int x = area.getX() + (area.getWidth() - cellWidth * 8) / 2;

    if (modeControl != nullptr)
        modeControl->setBounds (x, area.getY(), cellWidth, cellHeight);

    const auto all = knobs();
    for (size_t i = 0; i < all.size(); ++i)
        if (all[i] != nullptr)
            all[i]->setBounds (x + (int) (i + 1) * cellWidth, area.getY(), cellWidth, cellHeight);
}

void FilterPanel::paintOverChildren (juce::Graphics& g)
{
    for (int i = 0; i < kNumFilters; ++i)
    {
        if (! isOn (i))
            continue;

        const auto tab = tabs[(size_t) i].getBounds().toFloat();
        g.setColour (Palette::accentBright);
        g.fillEllipse (tab.getRight() - 6.0f, tab.getY() + 3.0f, 3.5f, 3.5f);
    }
}

} // namespace sonder::ui
