#include "MainView.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"

namespace sonder::ui
{

namespace
{
    constexpr int kMargin = 12;
    constexpr int kHeaderHeight = 48;
    constexpr int kTitleHeight = 26;
    constexpr int kCellHeight = 94;
    constexpr int kModRowHeight = 40;
    constexpr int kKeyboardHeight = 62;
    constexpr int kContentBottom = 780;
    constexpr int kFirstKey = 36, kLastKey = 96;
}

MainView::MainView (SonderAudioProcessor& p, bool keyboardVisible)
    : processor (p),
      showKeyboard (keyboardVisible),
      osc1View (p, 0),
      osc2View (p, 1),
      filterView (p),
      scopeView (p.scope),
      filterEnvelopeView (p.parameters, ParamIDs::filterAttack, ParamIDs::filterDecay, ParamIDs::filterSustain, ParamIDs::filterRelease),
      ampEnvelopeView (p.parameters, ParamIDs::ampAttack, ParamIDs::ampDecay, ParamIDs::ampSustain, ParamIDs::ampRelease),
      filterEnvelopeHandle ([] { return ModSource::filterEnv; }),
      ampEnvelopeHandle ([] { return ModSource::ampEnv; }),
      lfoPanel (p),
      presetBar (p.presetManager),
      voiceLeds (p.activeVoiceMask),
      masterControl (p, ParamIDs::masterGain, "Master", ParameterControl::Style::inline_),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    using namespace ParamIDs;
    panels.reserve (24);

    // ---------------------------------------------------------------- SYNTH
    auto& osc1 = addPanel (synthPage, "OSC 1", { 12, 70, 250, 216 }, 3,
                           { { osc1Shape, "Shape" }, { osc1WtPos, "WT Pos" }, { pulseWidth, "PW" } });
    osc1.display = &osc1View;
    osc1.displayHeight = 86;

    auto& osc2 = addPanel (synthPage, "OSC 2", { 272, 70, 310, 216 }, 4,
                           { { osc2Shape, "Shape" }, { osc2WtPos, "WT Pos" }, { osc2Semi, "Semi" }, { osc2Fine, "Fine" } });
    osc2.display = &osc2View;
    osc2.displayHeight = 86;

    addPanel (synthPage, "MIXER", { 592, 70, 232, 216 }, 3,
              { { oscMix, "Mix" }, { subLevel, "Sub" }, { noiseLevel, "Noise" },
                { fmAmount, "FM" }, { ringLevel, "Ring" }, { foldAmount, "Fold" } });

    auto& scope = addPanel (synthPage, "SCOPE", { 834, 70, 554, 216 }, 1, {});
    scope.display = &scopeView;
    scope.displayHeight = 216 - kTitleHeight - 8;

    auto& filter = addPanel (synthPage, "FILTER", { 12, 296, 580, 224 }, 8,
                             { { filterMode, "Mode" }, { cutoff, "Cutoff" }, { resonance, "Reso" }, { drive, "Drive" },
                               { vowel, "Vowel" }, { filterEnvAmt, "Env Amt" }, { keyTrack, "Key Trk" }, { velToCutoff, "Vel" } });
    filter.display = &filterView;
    filter.displayHeight = 94;

    addPanel (synthPage, "DISTORTION", { 602, 296, 160, 224 }, 2,
              { { distType, "Type" }, { distDrive, "Drive" }, { distMix, "Mix" }, { distTone, "Tone" } });

    addPanel (synthPage, "LFO", { 772, 296, 616, 224 }, 1, {});

    auto& filterEnv = addPanel (synthPage, "FILTER ENV", { 12, 530, 360, 250 }, 4,
                                { { filterAttack, "Attack" }, { filterDecay, "Decay" },
                                  { filterSustain, "Sustain" }, { filterRelease, "Release" } });
    filterEnv.display = &filterEnvelopeView;
    filterEnv.displayHeight = 118;

    auto& ampEnv = addPanel (synthPage, "AMP ENV", { 382, 530, 438, 250 }, 5,
                             { { ampAttack, "Attack" }, { ampDecay, "Decay" }, { ampSustain, "Sustain" },
                               { ampRelease, "Release" }, { ampVelocity, "Velocity" } });
    ampEnv.display = &ampEnvelopeView;
    ampEnv.displayHeight = 118;

    addPanel (synthPage, "ANALOG", { 830, 530, 558, 120 }, 7,
              { { drift, "Drift" }, { jitter, "Jitter" }, { spread, "Spread" }, { sag, "Sag" },
                { warmup, "Warm-up" }, { unit, "Unit" } });

    addPanel (synthPage, "VOICE", { 830, 660, 558, 120 }, 7,
              { { voiceMode, "Mode" }, { unisonVoices, "Unison" }, { unisonDetune, "Detune" }, { unisonWidth, "Width" },
                { glide, "Glide" }, { bendRange, "Bend" }, { vibrato, "Vibrato" } });

    // ---------------------------------------------------------------- MOD
    addPanel (modPage, "MOD MATRIX", { 12, 70, 1376, 710 }, 1, {});

    // ---------------------------------------------------------------- FX
    addPanel (fxPage, "CHORUS", { 12, 70, 440, 216 }, 2, { { chorusMode, "Mode" }, { chorusMix, "Mix" } });
    addPanel (fxPage, "DELAY", { 462, 70, 540, 216 }, 5,
              { { delaySync, "Sync" }, { delayTime, "Time" }, { delayFeedback, "Feedback" },
                { delayMix, "Mix" }, { delayTape, "Tape" } });
    addPanel (fxPage, "REVERB", { 1012, 70, 376, 216 }, 2, { { reverbSize, "Size" }, { reverbMix, "Mix" } });

    auto& output = addPanel (fxPage, "OUTPUT", { 12, 296, 1376, 484 }, 1, {});
    output.display = &scopeView;
    output.displayHeight = 484 - kTitleHeight - 8;

    // ---------------------------------------------------------------- остальное
    addChildComponent (osc1View);
    addChildComponent (osc2View);
    addChildComponent (filterView);
    addChildComponent (scopeView);
    addChildComponent (filterEnvelopeView);
    addChildComponent (ampEnvelopeView);
    addChildComponent (lfoPanel);
    addChildComponent (filterEnvelopeHandle);
    addChildComponent (ampEnvelopeHandle);
    filterEnvelopeHandle.setTooltip ("Drag onto a knob to modulate it with the filter envelope");
    ampEnvelopeHandle.setTooltip ("Drag onto a knob to modulate it with the amp envelope");

    for (int slot = 0; slot < kNumModSlots; ++slot)
        addChildComponent (*modSlots.emplace_back (std::make_unique<ModSlotView> (p.parameters, slot)));

    const char* pageNames[] { "SYNTH", "MOD", "FX" };
    for (int i = 0; i < numPages; ++i)
    {
        auto& button = pageButtons[(size_t) i];
        button.setButtonText (pageNames[i]);
        button.setClickingTogglesState (true);
        button.setRadioGroupId (0x9a6e);
        button.onClick = [this, i] { showPage (static_cast<Page> (i)); };
        addAndMakeVisible (button);
    }

    addAndMakeVisible (presetBar);
    addAndMakeVisible (voiceLeds);
    addAndMakeVisible (masterControl);

    // Подсказки к "необычным" ручкам
    const std::pair<const char*, const char*> tips[] {
        { drift, "Slow random pitch and cutoff wander of every oscillator" },
        { jitter, "Fast pitch instability of the oscillators" },
        { spread, "Component tolerances between voice cards: tuning, cutoff, envelope speed, level, pan" },
        { sag, "Power supply sag: loud chords pull the pitch down and compress the level" },
        { warmup, "Cold start: after loading or turning this knob the synth drifts flat and settles over a minute" },
        { unit, "Pick a different 'hardware unit' with its own set of component tolerances" },
        { osc1WtPos, "Wavetable position: morphs through the frames of the table" },
        { osc2WtPos, "Wavetable position: morphs through the frames of the table" },
        { fmAmount, "Oscillator 2 modulates the frequency of oscillator 1" },
        { ringLevel, "Ring modulation: oscillator 1 multiplied by oscillator 2" },
        { foldAmount, "Wavefolder: folds the waveform back on itself for rich harmonics" },
        { vowel, "Vowel of the formant filter (A-E-I-O-U), works in Vowel mode" },
        { filterMode, "Vowel mode turns the filter into a formant 'talking' filter; Cutoff shifts the formants" },
        { distType, "Distortion after the filter: Tube, Hard clip, Fold, Bit Crush" },
        { vibrato, "Vibrato depth controlled by the mod wheel" },
        { delayTape, "Tape character of the delay: wow, flutter, darker and saturated repeats" },
    };

    for (const auto& [id, tip] : tips)
        if (auto it = controlsById.find (id); it != controlsById.end())
            it->second->setTooltipText (tip);

    if (showKeyboard)
    {
        keyboard.setAvailableRange (kFirstKey, kLastKey);
        keyboard.setScrollButtonsVisible (false);
        keyboard.setOctaveForMiddleC (4);
        keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffc6d3d7));
        keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff0d1518));
        keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff6a8188));
        keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::accent.withAlpha (0.3f));
        keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Palette::accent.withAlpha (0.85f));
        keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.35f));
        addAndMakeVisible (keyboard);
    }

    setSize (kWidth, getDesignHeight());
    pageButtons[0].setToggleState (true, juce::dontSendNotification);
    showPage (synthPage);
}

int MainView::getDesignHeight() const noexcept
{
    return showKeyboard ? kContentBottom + 10 + kKeyboardHeight + kMargin
                        : kContentBottom + kMargin;
}

ParameterControl* MainView::control (const char* id, const char* label)
{
    auto& created = controls.emplace_back (std::make_unique<ParameterControl> (processor, id, label));
    addChildComponent (*created);
    controlsById[id] = created.get();
    return created.get();
}

MainView::Panel& MainView::addPanel (Page page, const juce::String& title, juce::Rectangle<int> bounds, int columns,
                                     std::initializer_list<std::pair<const char*, const char*>> items)
{
    Panel panel;
    panel.title = title;
    panel.page = page;
    panel.bounds = bounds;
    panel.columns = columns;

    for (const auto& [id, label] : items)
        panel.cells.push_back (id != nullptr ? control (id, label) : nullptr);

    panels.push_back (std::move (panel));
    return panels.back();
}

void MainView::showPage (Page page)
{
    currentPage = page;

    // Сначала прячем всё, потом показываем текущую страницу: экран осциллографа общий для двух страниц
    for (auto& panel : panels)
    {
        for (auto* cell : panel.cells)
            if (cell != nullptr)
                cell->setVisible (false);

        if (panel.display != nullptr)
            panel.display->setVisible (false);
    }

    for (auto& panel : panels)
    {
        if (panel.page != page)
            continue;

        for (auto* cell : panel.cells)
            if (cell != nullptr)
                cell->setVisible (true);

        if (panel.display != nullptr)
            panel.display->setVisible (true);
    }

    lfoPanel.setVisible (page == synthPage);
    filterEnvelopeHandle.setVisible (page == synthPage);
    ampEnvelopeHandle.setVisible (page == synthPage);
    for (auto& slot : modSlots)
        slot->setVisible (page == modPage);

    resized();
    repaint();
}

void MainView::layoutPanel (Panel& panel)
{
    auto content = panel.bounds.reduced (8, 0).withTrimmedTop (kTitleHeight).withTrimmedBottom (6);

    if (panel.display != nullptr)
    {
        panel.display->setBounds (content.removeFromTop (panel.displayHeight));
        content.removeFromTop (4);
    }

    if (panel.cells.empty())
        return;

    const int rows = ((int) panel.cells.size() + panel.columns - 1) / panel.columns;
    const int cellWidth = juce::jmin (72, content.getWidth() / panel.columns);
    const int cellHeight = juce::jmin (kCellHeight, content.getHeight() / rows);
    const int gridWidth = cellWidth * panel.columns;
    const int gridHeight = cellHeight * rows;

    // Сетка по центру свободного места
    auto grid = content.withSizeKeepingCentre (gridWidth, juce::jmin (gridHeight, content.getHeight()));
    if (panel.display != nullptr)
        grid.setY (content.getY());

    for (size_t i = 0; i < panel.cells.size(); ++i)
    {
        if (panel.cells[i] == nullptr)
            continue;

        const int row = (int) i / panel.columns;
        const int column = (int) i % panel.columns;
        panel.cells[i]->setBounds (grid.getX() + column * cellWidth, grid.getY() + row * cellHeight, cellWidth, cellHeight);
    }
}

void MainView::resized()
{
    for (auto& panel : panels)
        if (panel.page == currentPage)
            layoutPanel (panel);

    for (auto& panel : panels)
    {
        if (panel.title == "LFO")
            lfoPanel.setBounds (panel.bounds);

        // Значки-источники в заголовках огибающих
        if (panel.title == "FILTER ENV")
            filterEnvelopeHandle.setBounds (panel.bounds.getRight() - 30, panel.bounds.getY() + 3, 22, 20);
        if (panel.title == "AMP ENV")
            ampEnvelopeHandle.setBounds (panel.bounds.getRight() - 30, panel.bounds.getY() + 3, 22, 20);

        if (panel.title == "MOD MATRIX")
        {
            // Две колонки по 16 слотов
            auto area = panel.bounds.reduced (14, 0).withTrimmedTop (kTitleHeight + 8);
            const int columnWidth = (area.getWidth() - 24) / 2;
            const int half = kNumModSlots / 2;

            for (int slot = 0; slot < kNumModSlots; ++slot)
            {
                const int column = slot / half;
                const int row = slot % half;
                modSlots[(size_t) slot]->setBounds (area.getX() + column * (columnWidth + 24), area.getY() + row * kModRowHeight,
                                                    columnWidth, kModRowHeight);
            }
        }
    }

    // Шапка
    for (int i = 0; i < numPages; ++i)
        pageButtons[(size_t) i].setBounds (384 + i * 84, 22, 78, 28);

    presetBar.setBounds (660, 21, 420, 30);
    voiceLeds.setBounds (1102, 17, 150, 38);
    masterControl.setBounds (1266, 16, 122, 40);

    if (showKeyboard)
    {
        const auto area = juce::Rectangle<int> (kMargin, kContentBottom + 10, kWidth - 2 * kMargin, kKeyboardHeight);
        keyboard.setBounds (area);

        // Клавиши растягиваются на всю ширину
        int whiteKeys = 0;
        for (int note = kFirstKey; note <= kLastKey; ++note)
            if (! juce::MidiMessage::isMidiNoteBlack (note))
                ++whiteKeys;

        keyboard.setKeyWidth ((float) area.getWidth() / (float) whiteKeys);
        keyboard.setLowestVisibleKey (kFirstKey);
    }
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (Palette::background);

    // Шапка
    const auto header = juce::Rectangle<int> (0, 0, getWidth(), kMargin + kHeaderHeight + 2).toFloat();
    g.setGradientFill (juce::ColourGradient (Palette::headerTop, 0.0f, 0.0f, Palette::background, 0.0f, header.getBottom(), false));
    g.fillRect (header);

    // Логотип: синусоида и название
    juce::Path logoWave;
    const float waveX = 20.0f, waveY = 36.0f;
    for (int i = 0; i <= 40; ++i)
    {
        const float t = (float) i / 40.0f;
        const float y = waveY - std::sin (t * juce::MathConstants<float>::twoPi) * 8.0f * (1.0f - 0.35f * t);
        if (i == 0)
            logoWave.startNewSubPath (waveX + t * 26.0f, y);
        else
            logoWave.lineTo (waveX + t * 26.0f, y);
    }

    g.setColour (Palette::accent.withAlpha (0.25f));
    g.strokePath (logoWave, { 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    g.setColour (Palette::accentBright);
    g.strokePath (logoWave, { 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    g.setColour (Palette::text);
    g.setFont (makeFont (26.0f, true, 0.3f));
    g.drawText ("SONDER", 56, 18, 170, 32, juce::Justification::centredLeft);

    g.setColour (Palette::textFaint);
    g.setFont (makeFont (10.0f, true, 0.25f));
    g.drawText ("ANALOG POLYSYNTH", 222, 27, 150, 16, juce::Justification::centredLeft);

    // Панели текущей страницы
    for (const auto& panel : panels)
    {
        if (panel.page != currentPage)
            continue;

        const auto bounds = panel.bounds.toFloat();

        g.setGradientFill (juce::ColourGradient (Palette::panelTop, 0.0f, bounds.getY(),
                                                 Palette::panelBottom, 0.0f, bounds.getBottom(), false));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (Palette::outline);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (Palette::accent);
        g.fillRoundedRectangle (bounds.getX() + 12.0f, bounds.getY() + 9.0f, 3.0f, 10.0f, 1.5f);

        g.setColour (Palette::text.withAlpha (0.85f));
        g.setFont (makeFont (11.0f, true, 0.18f));
        g.drawText (panel.title, panel.bounds.getX() + 21, panel.bounds.getY() + 4, panel.bounds.getWidth() - 30,
                    20, juce::Justification::centredLeft);
    }
}

} // namespace sonder::ui
