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
    constexpr int kModRowHeight = 36;
    constexpr int kKeyboardHeight = 62;
    constexpr int kContentBottom = 746;
    constexpr int kFirstKey = 36, kLastKey = 96;
}

MainView::MainView (SonderAudioProcessor& p, bool keyboardVisible)
    : processor (p),
      showKeyboard (keyboardVisible),
      scopeView (p.scope),
      filterEnvelopeView (p.parameters, ParamIDs::filterAttack, ParamIDs::filterDecay, ParamIDs::filterSustain, ParamIDs::filterRelease),
      ampEnvelopeView (p.parameters, ParamIDs::ampAttack, ParamIDs::ampDecay, ParamIDs::ampSustain, ParamIDs::ampRelease),
      lfo1View (p.parameters, ParamIDs::lfo1Shape, p.lfo1Phase),
      lfo2View (p.parameters, ParamIDs::lfo2Shape, p.lfo2Phase),
      presetBar (p.presetManager),
      voiceLeds (p.activeVoiceMask),
      masterControl (p.parameters, ParamIDs::masterGain, "Master", ParameterControl::Style::inline_),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    using namespace ParamIDs;
    panels.reserve (16);

    addPanel ("OSCILLATORS", { 12, 70, 448, 216 }, 6,
              { { osc1Shape, "Osc 1" }, { osc2Shape, "Osc 2" }, { osc2Semi, "Semi" }, { osc2Fine, "Fine" },
                { oscMix, "Mix" }, { pulseWidth, "PW" }, { subLevel, "Sub" }, { noiseLevel, "Noise" },
                { fmAmount, "FM" }, { ringLevel, "Ring" }, { foldAmount, "Fold" } });

    addPanel ("FILTER", { 470, 70, 304, 216 }, 4,
              { { filterMode, "Mode" }, { cutoff, "Cutoff" }, { resonance, "Reso" }, { drive, "Drive" },
                { filterEnvAmt, "Env Amt" }, { keyTrack, "Key Trk" }, { velToCutoff, "Vel" } });

    auto& scopePanel = addPanel ("SCOPE", { 784, 70, 444, 216 }, 1, {});
    scopePanel.display = &scopeView;
    scopePanel.displayHeight = 216 - kTitleHeight - 8;

    auto& filterEnv = addPanel ("FILTER ENV", { 12, 296, 304, 190 }, 4,
                                { { filterAttack, "Attack" }, { filterDecay, "Decay" },
                                  { filterSustain, "Sustain" }, { filterRelease, "Release" } });
    filterEnv.display = &filterEnvelopeView;
    filterEnv.displayHeight = 60;

    auto& ampEnv = addPanel ("AMP ENV", { 326, 296, 376, 190 }, 5,
                             { { ampAttack, "Attack" }, { ampDecay, "Decay" }, { ampSustain, "Sustain" },
                               { ampRelease, "Release" }, { ampVelocity, "Velocity" } });
    ampEnv.display = &ampEnvelopeView;
    ampEnv.displayHeight = 60;

    auto& lfo1 = addPanel ("LFO 1", { 712, 296, 253, 190 }, 3,
                           { { lfo1Shape, "Shape" }, { lfo1Rate, "Rate" }, { lfo1Sync, "Sync" } });
    lfo1.display = &lfo1View;
    lfo1.displayHeight = 60;

    auto& lfo2 = addPanel ("LFO 2", { 975, 296, 253, 190 }, 3,
                           { { lfo2Shape, "Shape" }, { lfo2Rate, "Rate" }, { lfo2Sync, "Sync" } });
    lfo2.display = &lfo2View;
    lfo2.displayHeight = 60;

    addPanel ("ANALOG", { 12, 496, 520, 120 }, 7,
              { { drift, "Drift" }, { jitter, "Jitter" }, { spread, "Spread" }, { sag, "Sag" },
                { warmup, "Warm-up" }, { unit, "Unit" } });

    addPanel ("VOICE", { 12, 626, 520, 120 }, 7,
              { { voiceMode, "Mode" }, { unisonVoices, "Unison" }, { unisonDetune, "Detune" }, { unisonWidth, "Width" },
                { glide, "Glide" }, { bendRange, "Bend" }, { vibrato, "Vibrato" } });

    addPanel ("MOD MATRIX", { 542, 496, 334, 250 }, 1, {});

    addPanel ("EFFECTS", { 886, 496, 342, 250 }, 5,
                              { { chorusMode, "Chorus" }, { chorusMix, "Ch Mix" }, { nullptr, nullptr },
                                { reverbSize, "Rev Size" }, { reverbMix, "Rev Mix" },
                                { delaySync, "Dly Sync" }, { delayTime, "Time" }, { delayFeedback, "Feedback" },
                                { delayMix, "Dly Mix" }, { delayTape, "Tape" } });

    for (auto& panel : panels)
        if (panel.display != nullptr)
            addAndMakeVisible (panel.display);

    for (int slot = 0; slot < kNumModSlots; ++slot)
        addAndMakeVisible (*modSlots.emplace_back (std::make_unique<ModSlotView> (p.parameters, slot)));

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
        { fmAmount, "Oscillator 2 modulates the frequency of oscillator 1" },
        { ringLevel, "Ring modulation: oscillator 1 multiplied by oscillator 2" },
        { foldAmount, "Wavefolder: folds the waveform back on itself for rich harmonics" },
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
    startTimerHz (10);
}

int MainView::getDesignHeight() const noexcept
{
    return showKeyboard ? kContentBottom + 10 + kKeyboardHeight + kMargin
                        : kContentBottom + kMargin;
}

ParameterControl* MainView::control (const char* id, const char* label)
{
    auto& created = controls.emplace_back (std::make_unique<ParameterControl> (processor.parameters, id, label));
    addAndMakeVisible (*created);
    controlsById[id] = created.get();
    return created.get();
}

MainView::Panel& MainView::addPanel (const juce::String& title, juce::Rectangle<int> bounds, int columns,
                                     std::initializer_list<std::pair<const char*, const char*>> items)
{
    Panel panel;
    panel.title = title;
    panel.bounds = bounds;
    panel.columns = columns;

    for (const auto& [id, label] : items)
        panel.cells.push_back (id != nullptr ? control (id, label) : nullptr);

    panels.push_back (std::move (panel));
    return panels.back();
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
    const int gridWidth = cellWidth * panel.columns;
    const int gridHeight = kCellHeight * rows;

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
        panel.cells[i]->setBounds (grid.getX() + column * cellWidth, grid.getY() + row * kCellHeight, cellWidth, kCellHeight);
    }
}

void MainView::resized()
{
    for (auto& panel : panels)
        layoutPanel (panel);

    // Мод-матрица
    for (auto& panel : panels)
    {
        if (panel.title != "MOD MATRIX")
            continue;

        auto area = panel.bounds.reduced (10, 0).withTrimmedTop (kTitleHeight + 4);
        for (auto& slot : modSlots)
            slot->setBounds (area.removeFromTop (kModRowHeight));
    }

    // Шапка
    presetBar.setBounds (420, 21, 400, 30);
    voiceLeds.setBounds (866, 17, 176, 38);
    masterControl.setBounds (1090, 16, 138, 40);

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

void MainView::timerCallback()
{
    // Ручка частоты не действует, пока включена синхронизация с темпом
    const auto dimWhenSynced = [this] (const char* rateID, std::atomic<float>* sync)
    {
        if (auto it = controlsById.find (rateID); it != controlsById.end())
            it->second->setAlpha (sync->load() > 0.5f ? 0.35f : 1.0f);
    };

    dimWhenSynced (ParamIDs::lfo1Rate, processor.parameters.getRawParameterValue (ParamIDs::lfo1Sync));
    dimWhenSynced (ParamIDs::lfo2Rate, processor.parameters.getRawParameterValue (ParamIDs::lfo2Sync));
    dimWhenSynced (ParamIDs::delayTime, processor.parameters.getRawParameterValue (ParamIDs::delaySync));
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
    g.drawText ("ANALOG POLYSYNTH", 222, 27, 180, 16, juce::Justification::centredLeft);

    // Панели
    for (const auto& panel : panels)
    {
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
