#include "MainView.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "Theme.h"

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
    constexpr int kColdSteps = 32;
    constexpr float kMaxColdDim = 0.3f;

    // Кружок-образец цвета для пункта меню; выбранный обведён
    std::unique_ptr<juce::Drawable> makeSwatch (juce::Colour colour, bool selected)
    {
        juce::Path circle;
        circle.addEllipse (2.0f, 2.0f, 12.0f, 12.0f);

        auto swatch = std::make_unique<juce::DrawablePath>();
        swatch->setPath (circle);
        swatch->setFill (colour);
        swatch->setStrokeFill (selected ? Palette::text : juce::Colours::transparentBlack);
        swatch->setStrokeThickness (selected ? 2.0f : 0.0f);
        return swatch;
    }
}

bool MainView::PageButton::isInterestedInDragSource (const SourceDetails& details)
{
    return ModSourceHandle::sourceFromDrag (details.description) > 0;
}

void MainView::PageButton::timerCallback()
{
    // Как настоящее нажатие: включает кнопку и открывает страницу
    stopTimer();
    if (! getToggleState())
        triggerClick();
}

void MainView::ViewButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? Palette::accentDim.withAlpha (0.5f) : Palette::deep);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (highlighted ? Palette::accent : Palette::outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    // Значок: круг из двух половин - акцент и его светлый вариант
    const auto circle = juce::Rectangle<float> (14.0f, 14.0f).withCentre (bounds.getCentre());
    juce::Path half;
    half.addPieSegment (circle, juce::MathConstants<float>::pi * 0.25f, juce::MathConstants<float>::pi * 1.25f, 0.0f);

    g.setColour (Palette::accentBright);
    g.fillEllipse (circle);
    g.setColour (Palette::accent);
    g.fillPath (half);
}

MainView::MainView (SonderAudioProcessor& p, bool keyboardVisible)
    : processor (p),
      showKeyboard (keyboardVisible),
      visuals (p),
      oscPanel (p, visuals),
      filterPanel (p, visuals),
      scopeView (p.scope, visuals),
      fxRackView (p),
      filterEnvelopeView (p.parameters, ParamIDs::filterAttack, ParamIDs::filterDecay, ParamIDs::filterSustain, ParamIDs::filterRelease),
      ampEnvelopeView (p.parameters, ParamIDs::ampAttack, ParamIDs::ampDecay, ParamIDs::ampSustain, ParamIDs::ampRelease),
      filterEnvelopeHandle ([] { return ModSource::filterEnv; }),
      ampEnvelopeHandle ([] { return ModSource::ampEnv; }),
      lfoPanel (p),
      arpView (p),
      expressionPanel (p),
      tuningPanel (p),
      presetBar (p.presetManager),
      voiceLeds (p),
      masterControl (p, ParamIDs::masterGain, "Master", ParameterControl::Style::inline_),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    using namespace ParamIDs;
    panels.reserve (24);

    // ---------------------------------------------------------------- SYNTH
    // Четыре осциллятора на вкладках; панель раскладывает себя сама
    addPanel (synthPage, "OSC", { 12, 70, 570, 216 }, 1, {});

    addPanel (synthPage, "MIXER", { 592, 70, 304, 216 }, 5,
              { { oscLevel (0), "Osc 1" }, { oscLevel (1), "Osc 2" }, { oscLevel (2), "Osc 3" }, { oscLevel (3), "Osc 4" },
                { subLevel, "Sub" },
                { noiseLevel, "Noise" }, { noiseColor, "Color" }, { fmAmount, "FM" }, { ringLevel, "Ring" }, { foldAmount, "Fold" } });

    auto& scope = addPanel (synthPage, "SCOPE", { 906, 70, 482, 216 }, 1, {});
    scope.display = &scopeView;
    scope.displayHeight = 216 - kTitleHeight - 8;

    // Два фильтра на вкладках; панель раскладывает себя сама
    addPanel (synthPage, "FILTER", { 12, 296, 580, 224 }, 1, {});

    addPanel (synthPage, "DISTORTION", { 602, 296, 176, 224 }, 2,
              { { distType, "Type" }, { distDrive, "Drive" }, { distMix, "Mix" }, { distTone, "Tone" } });

    addPanel (synthPage, "LFO", { 788, 296, 600, 224 }, 1, {});

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

    addPanel (synthPage, "VOICE", { 830, 660, 558, 120 }, 9,
              { { voiceMode, "Mode" }, { unisonVoices, "Unison" }, { unisonDetune, "Detune" }, { unisonWidth, "Width" },
                { glide, "Glide" }, { glideCurve, "Curve" }, { glideMode, "G.Mode" },
                { bendRange, "Bend" }, { vibrato, "Vibrato" } });

    // ---------------------------------------------------------------- MOD
    addPanel (modPage, "MOD MATRIX", { 12, 70, 1376, 710 }, 1, {});

    // ---------------------------------------------------------------- FX
    // Рэк эффектов: пять полос видно сразу, остальные прокручиваются. Под ним осциллограф выхода.
    addPanel (fxPage, "EFFECTS", { 12, 70, 1376, 586 }, 1, {});

    auto& output = addPanel (fxPage, "OUTPUT", { 12, 666, 1376, 114 }, 1, {});
    output.display = &scopeView;
    output.displayHeight = 114 - kTitleHeight - 8;

    // ---------------------------------------------------------------- PLAY
    auto& arp = addPanel (playPage, "ARPEGGIATOR", { 12, 70, 1376, 330 }, 5,
                          { { arpMode, "Mode" }, { arpOctaves, "Octaves" }, { arpRate, "Rate" }, { arpGate, "Gate" }, { arpSwing, "Swing" } });
    arp.display = &arpView;
    arp.displayHeight = 196;

    addPanel (playPage, "EXPRESSION", { 12, 410, 680, 370 }, 1, {});
    addPanel (playPage, "TUNING", { 702, 410, 686, 370 }, 1, {});

    // ---------------------------------------------------------------- остальное
    addChildComponent (oscPanel);
    addChildComponent (filterPanel);

    // Кнопка в заголовке панели дисторшна: показывает текущее положение, клик переключает
    distPositionButton.setTooltip ("Distortion after the filter (POST) or before it (PRE): "
                                   "before the filter it distorts the raw oscillators and the filter smooths the result");
    distPositionAttachment = std::make_unique<juce::ParameterAttachment> (*p.parameters.getParameter (distPosition), [this] (float value)
    {
        distPositionButton.setButtonText (value > 0.5f ? "PRE" : "POST");
    });
    distPositionButton.onClick = [this]
    {
        const bool pre = processor.parameters.getRawParameterValue (ParamIDs::distPosition)->load() > 0.5f;
        distPositionAttachment->setValueAsCompleteGesture (pre ? 0.0f : 1.0f);
    };
    distPositionAttachment->sendInitialUpdate();
    addChildComponent (distPositionButton);
    addChildComponent (scopeView);

    addChildComponent (fxRackView);
    addChildComponent (arpView);
    addChildComponent (expressionPanel);
    addChildComponent (tuningPanel);

    arpOnButton.setClickingTogglesState (true);
    arpOnButton.setTooltip ("Switch the arpeggiator on: held notes play one after another");
    arpOnAttachment = std::make_unique<Apvts::ButtonAttachment> (p.parameters, arpOn, arpOnButton);
    arpHoldButton.setClickingTogglesState (true);
    arpHoldButton.setTooltip ("Hold: the chord keeps playing after the keys are released; a new chord replaces it");
    arpHoldAttachment = std::make_unique<Apvts::ButtonAttachment> (p.parameters, arpHold, arpHoldButton);
    mpeButton.setClickingTogglesState (true);
    mpeButton.setTooltip ("MPE: every note on its own MIDI channel with its own pitch bend, pressure and slide");
    mpeAttachment = std::make_unique<Apvts::ButtonAttachment> (p.parameters, mpeOn, mpeButton);

    for (auto* button : { &arpOnButton, &arpHoldButton, &mpeButton })
        addChildComponent (*button);

    // Источники модуляции над рэком: LFO, огибающие, контроллеры
    for (int lfo = 0; lfo < kNumLfos; ++lfo)
        fxSourceChips.push_back (std::make_unique<ModSourceChip> (sourceForLfo (lfo), "LFO " + juce::String (lfo + 1)));

    const std::pair<ModSource, const char*> otherSources[] {
        { ModSource::filterEnv, "F.ENV" }, { ModSource::ampEnv, "A.ENV" }, { ModSource::velocity, "VEL" },
        { ModSource::modWheel, "WHEEL" }, { ModSource::aftertouch, "PRESS" }, { ModSource::slide, "SLIDE" },
        { ModSource::key, "KEY" }, { ModSource::random, "RAND" },
    };

    for (const auto& [source, name] : otherSources)
        fxSourceChips.push_back (std::make_unique<ModSourceChip> (source, name));

    for (auto& chip : fxSourceChips)
        addChildComponent (*chip);
    addChildComponent (filterEnvelopeView);
    addChildComponent (ampEnvelopeView);
    addChildComponent (lfoPanel);
    addChildComponent (filterEnvelopeHandle);
    addChildComponent (ampEnvelopeHandle);
    filterEnvelopeHandle.setTooltip ("Drag onto a knob to modulate it with the filter envelope");
    ampEnvelopeHandle.setTooltip ("Drag onto a knob to modulate it with the amp envelope");

    for (int slot = 0; slot < kNumModSlots; ++slot)
        addChildComponent (*modSlots.emplace_back (std::make_unique<ModSlotView> (p.parameters, p.fxRack, p.lfoShapes, slot)));

    const char* pageNames[] { "SYNTH", "MOD", "FX", "PLAY" };
    for (int i = 0; i < numPages; ++i)
    {
        auto& button = pageButtons[(size_t) i];
        button.setButtonText (pageNames[i]);
        button.setClickingTogglesState (true);
        button.setRadioGroupId (0x9a6e);
        // onClick приходит и кнопке, которую радиогруппа выключает: переключаемся только по включённой
        button.onClick = [this, i]
        {
            if (pageButtons[(size_t) i].getToggleState())
                showPage (static_cast<Page> (i));
        };
        addAndMakeVisible (button);
    }

    viewButton.setTooltip ("Theme, accent colour, visual effects and sound quality");
    viewButton.onClick = [this] { showViewMenu(); };

    addAndMakeVisible (presetBar);
    addAndMakeVisible (viewButton);
    addAndMakeVisible (voiceLeds);
    addAndMakeVisible (masterControl);

    // При смене пресета ручки не прыгают, а доезжают до новых значений
    processor.presetManager.onBeforeLoad = [this]
    {
        if (! Settings::get().motion)
            return;

        for (auto& each : controls)
            each->beginMorph();

        oscPanel.beginMorph();
        filterPanel.beginMorph();
        masterControl.beginMorph();
    };

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
        { noiseColor, "Noise colour, morphs smoothly: White > Pink > Brown > vinyl Crackle > tape Hiss > pitch-tracked Digital" },
        { glideCurve, "Shape of the glide: +100% starts fast and eases in, 0% moves at a constant rate, -100% starts slowly" },
        { glideMode, "Auto: in Legato voice mode glide only between overlapping notes, otherwise always\n"
                     "Always: glide on every note\n"
                     "Legato: glide only when a new note starts while another key is still held" },
        { distType, "Distortion after the filter: Tube, Hard clip, Fold, Bit Crush" },
        { vibrato, "Vibrato depth controlled by the mod wheel" },
        { arpMode, "Up, Down, Up-Down, Random, or in the order the keys were pressed" },
        { arpOctaves, "How many octaves up the pattern repeats" },
        { arpRate, "Step length. With the host transport playing, steps follow its grid" },
        { arpGate, "How long each note sounds, as a share of the step" },
        { arpSwing, "Delays every second step: shuffle feel" },
    };

    for (const auto& [id, tip] : tips)
        if (auto it = controlsById.find (id); it != controlsById.end())
            it->second->setTooltipText (tip);

    if (showKeyboard)
    {
        keyboard.setAvailableRange (kFirstKey, kLastKey);
        keyboard.setScrollButtonsVisible (false);
        keyboard.setOctaveForMiddleC (4);
        keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, themed (0xffc6d3d7));
        keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, themed (0xff0d1518));
        keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, themed (0xff6a8188));
        keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, Palette::accent.withAlpha (0.3f));
        keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, Palette::accent.withAlpha (0.85f));
        keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.35f));
        addAndMakeVisible (keyboard);
    }

    setSize (kWidth, getDesignHeight());

    // Открываем ту страницу, на которой окно закрыли (или сменили тему)
    const int page = juce::jlimit (0, (int) numPages - 1, Settings::get().page);
    pageButtons[(size_t) page].setToggleState (true, juce::dontSendNotification);
    showPage (static_cast<Page> (page));
    startTimerHz (10);
}

MainView::~MainView()
{
    processor.presetManager.onBeforeLoad = nullptr;
}

void MainView::updateSourceChips()
{
    // Вкладок LFO столько, сколько открыто на странице SYNTH
    const int lfos = LfoPanel::visibleLfoCount (processor);
    if (lfos == shownLfoChips)
        return;

    shownLfoChips = lfos;
    for (size_t i = 0; i < fxSourceChips.size(); ++i)
        fxSourceChips[i]->setVisible (currentPage == fxPage && (i >= (size_t) kNumLfos || (int) i < lfos));

    resized();
}

void MainView::timerCallback()
{
    updateSourceChips();

    // Холодный синт: окно чуть тусклее и "разгорается" по мере прогрева. Перерисовывать всё окно дорого,
    // поэтому затемнение меняется ступенями.
    // Уровни выключенных осцилляторов в микшере - тусклее
    for (int osc = 0; osc < kNumOscs; ++osc)
    {
        if (const auto it = controlsById.find (ParamIDs::oscLevel (osc)); it != controlsById.end())
        {
            const bool on = processor.parameters.getRawParameterValue (ParamIDs::oscOn (osc))->load() > 0.5f;
            setDimmed (*it->second, on ? 1.0f : 0.4f);
        }
    }

    const int step = juce::roundToInt (juce::jlimit (0.0f, 1.0f, visuals.getCold()) * (float) kColdSteps);

    if (step != coldStep)
    {
        coldStep = step;
        repaint();
    }
}

void MainView::paintOverChildren (juce::Graphics& g)
{
    if (coldStep > 0)
        g.fillAll (juce::Colours::black.withAlpha (kMaxColdDim * (float) coldStep / (float) kColdSteps));
}

void MainView::showViewMenu()
{
    const auto& settings = Settings::get();
    const auto& themes = getThemes();
    const auto& accents = getAccents();

    constexpr int themeId = 100, accentId = 200, effectId = 300, qualityId = 400;
    juce::PopupMenu menu;

    menu.addSectionHeader ("THEME");
    for (int i = 0; i < (int) themes.size(); ++i)
        menu.addItem (themeId + i, themes[(size_t) i].name, true, settings.theme == i);

    menu.addSectionHeader ("ACCENT");
    for (int i = 0; i < (int) accents.size(); ++i)
    {
        const auto colour = i == 0 ? juce::Colour (themes[(size_t) settings.theme].accent) : juce::Colour (accents[(size_t) i].colour);

        juce::PopupMenu::Item item (accents[(size_t) i].name);
        item.itemID = accentId + i;
        item.image = makeSwatch (colour, settings.accent == i);
        menu.addItem (std::move (item));
    }

    menu.addSectionHeader ("EFFECTS");
    menu.addItem (effectId + 0, "Colour follows the timbre", true, settings.timbreColour);
    menu.addItem (effectId + 1, "CRT screens: afterglow and scanlines", true, settings.crtScreen);
    menu.addItem (effectId + 2, "Spectrum behind the filter curve", true, settings.spectrum);
    menu.addItem (effectId + 3, "Motion: wavetable, knobs, preset changes", true, settings.motion);
    menu.addItem (effectId + 4, "Warm-up and sag dim the screens", true, settings.analogGlow);
    menu.addItem (effectId + 5, "Sparks on the oscilloscope", true, settings.particles);

    // Шейдеры считаются на видеокарте; если OpenGL не завёлся, пишем почему
    if (shaderStage->isAvailable())
        menu.addItem (effectId + 6, "Shaders: bloom, curved glass, colour fringes", true, settings.shaderFx);
    else
        menu.addItem (effectId + 6, "Shaders are not available (" + shaderStage->getStatus() + ")", false, false);

    // Качество звука хранится в проекте, у каждого экземпляра своё
    using Quality = SonderAudioProcessor::Quality;
    const auto quality = processor.getQuality();
    menu.addSectionHeader ("SOUND QUALITY (THIS INSTANCE)");
    menu.addItem (qualityId + (int) Quality::eco, "Eco: voices at 1x, half the CPU, a bit more aliasing", true, quality == Quality::eco);
    menu.addItem (qualityId + (int) Quality::normal, "Normal: voices at 2x", true, quality == Quality::normal);
    menu.addItem (qualityId + (int) Quality::high, "High: voices at 4x, twice the CPU", true, quality == Quality::high);
    menu.addItem (qualityId + 99, "Offline renders always use High", false, false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (viewButton),
                        [safeThis = juce::Component::SafePointer<MainView> (this)] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;

        auto& current = Settings::get();

        if (result >= qualityId)
        {
            safeThis->processor.setQuality (static_cast<SonderAudioProcessor::Quality> (result - qualityId));
            return;
        }

        if (result >= effectId)
        {
            bool* flags[] { &current.timbreColour, &current.crtScreen, &current.spectrum, &current.motion, &current.analogGlow,
                            &current.particles, &current.shaderFx };
            if (juce::isPositiveAndBelow (result - effectId, (int) std::size (flags)))
                *flags[result - effectId] = ! *flags[result - effectId];

            current.save();
            safeThis->repaint();
            return;
        }

        if (result >= accentId)
        {
            current.accent = result - accentId;
        }
        else
        {
            // Новая тема приходит со своим цветом акцента
            current.theme = result - themeId;
            current.accent = 0;
        }

        current.save();
        applyTheme (current);

        // Колбэк пересоздаёт это окно, поэтому вызываем его копию и после этого ничего не трогаем
        if (const auto changed = safeThis->onThemeChanged)
            changed();
    });
}

int MainView::getDesignHeight() const noexcept
{
    return showKeyboard ? kContentBottom + 10 + kKeyboardHeight + kMargin
                        : kContentBottom + kMargin;
}

ParameterControl* MainView::control (const juce::String& id, const char* label)
{
    auto& created = controls.emplace_back (std::make_unique<ParameterControl> (processor, id, label));
    addChildComponent (*created);
    controlsById[id] = created.get();
    return created.get();
}

MainView::Panel& MainView::addPanel (Page page, const juce::String& title, juce::Rectangle<int> bounds, int columns,
                                     std::initializer_list<std::pair<juce::String, const char*>> items)
{
    Panel panel;
    panel.title = title;
    panel.page = page;
    panel.bounds = bounds;
    panel.columns = columns;

    for (const auto& [id, label] : items)
        panel.cells.push_back (id.isNotEmpty() ? control (id, label) : nullptr);

    panels.push_back (std::move (panel));
    return panels.back();
}

void MainView::showPage (Page page)
{
    currentPage = page;
    Settings::get().page = (int) page;

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

    fxRackView.setVisible (page == fxPage);
    expressionPanel.setVisible (page == playPage);
    tuningPanel.setVisible (page == playPage);
    arpOnButton.setVisible (page == playPage);
    shownLfoChips = -1;
    updateSourceChips();
    arpHoldButton.setVisible (page == playPage);
    mpeButton.setVisible (page == playPage);
    oscPanel.setVisible (page == synthPage);
    filterPanel.setVisible (page == synthPage);
    distPositionButton.setVisible (page == synthPage);
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

        if (panel.title == "OSC")
            oscPanel.setBounds (panel.bounds);

        if (panel.title == "FILTER")
            filterPanel.setBounds (panel.bounds);

        if (panel.title == "DISTORTION")
            distPositionButton.setBounds (panel.bounds.getRight() - 8 - 44, panel.bounds.getY() + 3, 44, 20);

        if (panel.title == "EFFECTS")
        {
            fxRackView.setBounds (panel.bounds);

            // Источники модуляции в заголовке рэка, после названия панели
            int x = panel.bounds.getX() + 110;
            for (auto& chip : fxSourceChips)
            {
                if (! chip->isVisible())
                    continue;

                chip->setBounds (x, panel.bounds.getY() + 4, 54, 18);
                x += 58;
            }
        }

        if (panel.title == "ARPEGGIATOR")
        {
            arpOnButton.setBounds (panel.bounds.getRight() - 8 - 44, panel.bounds.getY() + 3, 44, 20);
            arpHoldButton.setBounds (arpOnButton.getX() - 8 - 56, panel.bounds.getY() + 3, 56, 20);
        }

        if (panel.title == "EXPRESSION")
        {
            expressionPanel.setBounds (panel.bounds);
            mpeButton.setBounds (panel.bounds.getRight() - 8 - 52, panel.bounds.getY() + 3, 52, 20);
        }

        if (panel.title == "TUNING")
            tuningPanel.setBounds (panel.bounds);

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
        pageButtons[(size_t) i].setBounds (384 + i * 68, 22, 64, 28);

    presetBar.setBounds (660, 21, 386, 30);
    viewButton.setBounds (1054, 21, 30, 30);
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
        g.drawText (panel.title, panel.bounds.getX() + 21, panel.bounds.getY() + 4, panel.bounds.getWidth() - 60,
                    20, juce::Justification::centredLeft);
    }
}

} // namespace sonder::ui
