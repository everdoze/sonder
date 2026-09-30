#include "LfoEditor.h"
#include "Controls.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "Theme.h"

namespace sonder::ui
{

namespace
{
    constexpr float kPointRadius = 4.5f;
    constexpr float kHitRadius = 9.0f;
    constexpr float kDropRadius = 16.0f;

    bool isRandomShape (LfoShape shape)
    {
        return shape == LfoShape::sampleAndHold || shape == LfoShape::smoothRandom;
    }
}

LfoEditor::LfoEditor (SonderAudioProcessor& p)
    : processor (p)
{
    startTimerHz (30);
}

void LfoEditor::setLfo (int index)
{
    lfo = index;
    hover = drag = {};
    refresh();
}

LfoShape LfoEditor::currentShape() const
{
    return static_cast<LfoShape> ((int) processor.parameters.getRawParameterValue (ParamIDs::lfoShape (lfo))->load());
}

void LfoEditor::setShapeParameter (LfoShape shape)
{
    if (auto* parameter = processor.parameters.getParameter (ParamIDs::lfoShape (lfo)))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) shape));
        parameter->endChangeGesture();
    }
}

void LfoEditor::makeCustom()
{
    if (isCustom())
        return;

    processor.lfoShapes.setPoints (lfo, LfoMath::pointsForShape (currentShape()));
    setShapeParameter (LfoShape::custom);
}

std::vector<LfoPoint> LfoEditor::displayPoints() const
{
    return isCustom() ? processor.lfoShapes.getPoints (lfo) : LfoMath::pointsForShape (currentShape());
}

std::vector<LfoPoint> LfoEditor::livePoints() const
{
    auto points = displayPoints();
    if (! isCustom() || ! processor.displayVoiceActive.load() || ! hasPointModulation())
        return points;

    // Та же модуляция, что в голосе (Voice::updateLfoPoints), по живым значениям звучащего голоса
    const auto base = points;
    const int count = (int) points.size();

    for (int i = 0; i < juce::jmin (count, kMaxModulatedPoints); ++i)
    {
        auto& point = points[(size_t) i];
        point.y = juce::jlimit (-1.0f, 1.0f, point.y + 2.0f * processor.displayModulation[(size_t) lfoPointDest (lfo, i, false)].load());

        if (i > 0 && i < count - 1)
            point.x = juce::jlimit (points[(size_t) i - 1].x, base[(size_t) i + 1].x,
                                    point.x + processor.displayModulation[(size_t) lfoPointDest (lfo, i, true)].load());
    }

    return points;
}

float LfoEditor::displayValue (float phase) const
{
    const auto shape = currentShape();
    if (shape == LfoShape::custom || isRandomShape (shape))
        return LfoMath::evaluatePoints (livePoints(), phase);

    return LfoMath::evaluate (shape, juce::jmin (phase, 0.9999f), 0, 0, nullptr);
}

//==============================================================================
std::vector<int> LfoEditor::pointSlots (int point) const
{
    std::vector<int> slots;
    auto& state = processor.parameters;

    for (int slot = 0; slot < kNumModSlots; ++slot)
    {
        const auto dest = static_cast<ModDest> ((int) state.getRawParameterValue (ParamIDs::modDest (slot))->load());
        const int source = (int) state.getRawParameterValue (ParamIDs::modSource (slot))->load();
        int slotLfo = 0, slotPoint = 0;
        bool position = false;

        if (source != 0 && lfoPointFromDest (dest, slotLfo, slotPoint, position) && slotLfo == lfo && slotPoint == point)
            slots.push_back (slot);
    }

    return slots;
}

bool LfoEditor::hasPointModulation() const
{
    for (int point = 0; point < kMaxModulatedPoints; ++point)
        if (! pointSlots (point).empty())
            return true;

    return false;
}

std::pair<float, float> LfoEditor::pointSpan (int point, bool position) const
{
    auto& state = processor.parameters;
    float low = 0.0f, high = 0.0f;

    for (int slot : pointSlots (point))
    {
        if ((int) state.getRawParameterValue (ParamIDs::modDest (slot))->load() != (int) lfoPointDest (lfo, point, position))
            continue;

        const auto source = static_cast<ModSource> ((int) state.getRawParameterValue (ParamIDs::modSource (slot))->load());
        const auto polarity = static_cast<ModPolarity> (juce::jlimit (0, 2, (int) state.getRawParameterValue (ParamIDs::modPolarity (slot))->load()));
        const auto [spanLow, spanHigh] = modulationSpan (source, polarity, state.getRawParameterValue (ParamIDs::modAmount (slot))->load());
        low += spanLow;
        high += spanHigh;
    }

    // Высота: 100% - вся шкала от -1 до 1
    return position ? std::pair { low, high } : std::pair { 2.0f * low, 2.0f * high };
}

void LfoEditor::remapPointModulation (const std::function<int (int)>& newIndex)
{
    auto& state = processor.parameters;

    for (int slot = 0; slot < kNumModSlots; ++slot)
    {
        const auto dest = static_cast<ModDest> ((int) state.getRawParameterValue (ParamIDs::modDest (slot))->load());
        int slotLfo = 0, point = 0;
        bool position = false;

        if (! lfoPointFromDest (dest, slotLfo, point, position) || slotLfo != lfo)
            continue;

        const int moved = newIndex (point);
        if (moved < 0 || moved >= kMaxModulatedPoints)
        {
            clearModulationSlot (state, slot);
        }
        else if (moved != point)
        {
            if (auto* parameter = state.getParameter (ParamIDs::modDest (slot)))
                parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) (int) lfoPointDest (lfo, moved, position)));
        }
    }
}

int LfoEditor::pointNear (juce::Point<float> position) const
{
    const auto points = displayPoints();
    int best = -1;
    float bestDistance = kDropRadius;

    for (int i = 0; i < juce::jmin ((int) points.size(), kMaxModulatedPoints); ++i)
    {
        const float distance = toScreen (points[(size_t) i].x, points[(size_t) i].y).getDistanceFrom (position);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }

    return best;
}

//==============================================================================
bool LfoEditor::isInterestedInDragSource (const SourceDetails& details)
{
    return ModSourceHandle::sourceFromDrag (details.description) > 0;
}

void LfoEditor::itemDragMove (const SourceDetails& details)
{
    const int point = pointNear (details.localPosition.toFloat());
    if (point != dropPoint)
    {
        dropPoint = point;
        refresh();
    }
}

void LfoEditor::itemDragExit (const SourceDetails&)
{
    dropPoint = -1;
    refresh();
}

void LfoEditor::itemDropped (const SourceDetails& details)
{
    const int point = pointNear (details.localPosition.toFloat());
    dropPoint = -1;

    if (point >= 0)
    {
        makeCustom();

        // С Alt - положение точки по времени (у крайних точек его нет)
        const int count = (int) displayPoints().size();
        const bool position = juce::ModifierKeys::currentModifiers.isAltDown() && point > 0 && point < count - 1;
        const auto source = static_cast<ModSource> (ModSourceHandle::sourceFromDrag (details.description));
        addModulationSlot (processor.parameters, source, lfoPointDest (lfo, point, position));
    }

    refresh();
}

juce::Rectangle<float> LfoEditor::plotArea() const
{
    return getLocalBounds().toFloat().reduced (12.0f, 12.0f);
}

juce::Point<float> LfoEditor::toScreen (float x, float y) const
{
    const auto plot = plotArea();
    return { plot.getX() + x * plot.getWidth(), plot.getCentreY() - y * plot.getHeight() * 0.5f };
}

juce::Point<float> LfoEditor::fromScreen (juce::Point<float> position) const
{
    const auto plot = plotArea();
    return { juce::jlimit (0.0f, 1.0f, (position.x - plot.getX()) / plot.getWidth()),
             juce::jlimit (-1.0f, 1.0f, (plot.getCentreY() - position.y) / (plot.getHeight() * 0.5f)) };
}

LfoEditor::Hit LfoEditor::hitTest (juce::Point<float> position) const
{
    if (! isCustom())
        return {};

    const auto points = displayPoints();

    for (int i = (int) points.size() - 1; i >= 0; --i)
        if (toScreen (points[(size_t) i].x, points[(size_t) i].y).getDistanceFrom (position) < kHitRadius)
            return { Target::point, i };

    for (int i = 0; i + 1 < (int) points.size(); ++i)
    {
        const auto& a = points[(size_t) i];
        const auto& b = points[(size_t) i + 1];
        if (b.x - a.x < 0.02f)
            continue;

        const float midX = 0.5f * (a.x + b.x);
        if (toScreen (midX, LfoMath::evaluatePoints (points, midX)).getDistanceFrom (position) < kHitRadius)
            return { Target::curve, i };
    }

    return {};
}

void LfoEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto hit = hitTest (e.position);
    if (hit.target != hover.target || hit.index != hover.index)
    {
        hover = hit;
        refresh();
    }

    setMouseCursor (hit.target == Target::point ? juce::MouseCursor::DraggingHandCursor
                    : hit.target == Target::curve ? juce::MouseCursor::UpDownResizeCursor
                                                  : juce::MouseCursor::NormalCursor);
}

void LfoEditor::mouseExit (const juce::MouseEvent&)
{
    hover = {};
    refresh();
}

void LfoEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto hit = hitTest (e.position);
    const bool modulatedPoint = hit.target == Target::point && ! pointSlots (hit.index).empty();

    if (e.mods.isPopupMenu())
    {
        if (modulatedPoint)
            showPointMenu (hit.index);
        else
            showMenu();
        return;
    }

    // Alt+drag по модулированной точке: глубина её последней модуляции
    if (e.mods.isAltDown() && modulatedPoint)
    {
        amountSlot = pointSlots (hit.index).back();
        amountStart = processor.parameters.getRawParameterValue (ParamIDs::modAmount (amountSlot))->load();
        dragStartY = e.position.y;

        if (auto* amount = processor.parameters.getParameter (ParamIDs::modAmount (amountSlot)))
            amount->beginChangeGesture();
        return;
    }

    makeCustom();
    drag = hitTest (e.position);

    if (drag.target == Target::curve)
    {
        dragStartCurve = displayPoints()[(size_t) drag.index].curve;
        dragStartY = e.position.y;
    }
}

void LfoEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (amountSlot >= 0)
    {
        const float sensitivity = e.mods.isShiftDown() ? 600.0f : 150.0f;
        const float amount = juce::jlimit (-1.0f, 1.0f, amountStart + (dragStartY - e.position.y) / sensitivity);

        if (auto* parameter = processor.parameters.getParameter (ParamIDs::modAmount (amountSlot)))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (amount));

        refresh();
        return;
    }

    if (drag.target == Target::none)
        return;

    auto points = processor.lfoShapes.getPoints (lfo);
    const int last = (int) points.size() - 1;
    auto& point = points[(size_t) drag.index];

    if (drag.target == Target::point)
    {
        auto position = fromScreen (e.position);

        if (e.mods.isShiftDown())
        {
            position.x = std::round (position.x * 16.0f) / 16.0f;
            position.y = std::round (position.y * 8.0f) / 8.0f;
        }

        // Точки не перескакивают через соседей, концы прибиты к краям цикла
        if (drag.index == 0)
            position.x = 0.0f;
        else if (drag.index == last)
            position.x = 1.0f;
        else
            position.x = juce::jlimit (points[(size_t) drag.index - 1].x, points[(size_t) drag.index + 1].x, position.x);

        point.x = position.x;
        point.y = position.y;
    }
    else
    {
        // Тянем вверх - сегмент выгибается вверх, независимо от направления наклона
        const float delta = (dragStartY - e.position.y) / (plotArea().getHeight() * 0.5f);
        const bool rising = points[(size_t) drag.index + 1].y > point.y;
        point.curve = juce::jlimit (-1.0f, 1.0f, dragStartCurve + delta * (rising ? -1.0f : 1.0f));
    }

    processor.lfoShapes.setPoints (lfo, std::move (points));
    refresh();
}

void LfoEditor::mouseUp (const juce::MouseEvent&)
{
    if (amountSlot >= 0)
        if (auto* amount = processor.parameters.getParameter (ParamIDs::modAmount (amountSlot)))
            amount->endChangeGesture();

    amountSlot = -1;
    drag = {};
}

void LfoEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    makeCustom();
    auto points = processor.lfoShapes.getPoints (lfo);
    const auto hit = hitTest (e.position);

    if (hit.target == Target::point)
    {
        if (hit.index > 0 && hit.index < (int) points.size() - 1)
        {
            // Модуляции удалённой точки уходят, у следующих номер сдвигается
            const int removed = hit.index;
            points.erase (points.begin() + removed);
            remapPointModulation ([removed] (int i) { return i == removed ? -1 : (i > removed ? i - 1 : i); });
        }
    }
    else if (hit.target == Target::curve)
    {
        points[(size_t) hit.index].curve = 0.0f;
    }
    else
    {
        const auto position = fromScreen (e.position);
        if (position.x > 0.0f && position.x < 1.0f && (int) points.size() < LfoPointSet::kMaxPoints)
        {
            // Новая точка встаёт между соседями: номера точек правее сдвигаются
            const int inserted = (int) std::count_if (points.begin(), points.end(), [&] (const LfoPoint& p) { return p.x <= position.x; });
            points.push_back ({ position.x, position.y, 0.0f });
            remapPointModulation ([inserted] (int i) { return i >= inserted ? i + 1 : i; });
        }
    }

    processor.lfoShapes.setPoints (lfo, std::move (points));
    hover = {};
    refresh();
}

void LfoEditor::showMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("LOAD SHAPE");
    menu.addItem (1, "Sine");
    menu.addItem (2, "Triangle");
    menu.addItem (3, "Saw Up");
    menu.addItem (4, "Saw Down");
    menu.addItem (5, "Square");
    menu.addItem (6, "Random Steps");
    menu.addItem (7, "Exponential Rise");
    menu.addItem (8, "Pluck");
    menu.addSeparator();
    menu.addItem (10, "Flip Vertical");
    menu.addItem (11, "Flip Horizontal");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                        [safeThis = juce::Component::SafePointer<LfoEditor> (this)] (int result)
    {
        if (safeThis == nullptr || result == 0)
            return;

        auto& editor = *safeThis;
        editor.makeCustom();
        auto points = editor.processor.lfoShapes.getPoints (editor.lfo);
        const int oldCount = (int) points.size();

        switch (result)
        {
            case 1: points = LfoMath::pointsForShape (LfoShape::sine); break;
            case 2: points = LfoMath::pointsForShape (LfoShape::triangle); break;
            case 3: points = LfoMath::pointsForShape (LfoShape::sawUp); break;
            case 4: points = LfoMath::pointsForShape (LfoShape::sawDown); break;
            case 5: points = LfoMath::pointsForShape (LfoShape::square); break;
            case 6: points = LfoMath::pointsForShape (LfoShape::sampleAndHold); break;
            case 7: points = { { 0.0f, -1.0f, 0.8f }, { 1.0f, 1.0f, 0.0f } }; break;
            case 8: points = { { 0.0f, 1.0f, -0.8f }, { 1.0f, -1.0f, 0.0f } }; break;

            case 10:
                for (auto& p : points)
                    p.y = -p.y;
                break;

            case 11:
            {
                // Разворот во времени: изгиб сегмента меняет знак, модуляции едут за своими точками
                std::vector<LfoPoint> flipped;
                for (int i = (int) points.size() - 1; i >= 0; --i)
                {
                    const float curve = i > 0 ? -points[(size_t) i - 1].curve : 0.0f;
                    flipped.push_back ({ 1.0f - points[(size_t) i].x, points[(size_t) i].y, curve });
                }
                points = flipped;
                editor.remapPointModulation ([oldCount] (int i) { return oldCount - 1 - i; });
                break;
            }

            default:
                return;
        }

        // Новая форма: модуляции точек, которых в ней нет, уходят
        const int newCount = (int) points.size();
        if (result < 10)
            editor.remapPointModulation ([newCount] (int i) { return i < newCount ? i : -1; });

        editor.processor.lfoShapes.setPoints (editor.lfo, std::move (points));
        editor.refresh();
    });
}

void LfoEditor::showPointMenu (int point)
{
    auto& state = processor.parameters;
    const auto slots = pointSlots (point);

    juce::PopupMenu menu;
    menu.addSectionHeader ("POINT " + juce::String (point + 1) + " MODULATION");

    for (int slot : slots)
    {
        const int source = (int) state.getRawParameterValue (ParamIDs::modSource (slot))->load();
        const int percent = juce::roundToInt (state.getRawParameterValue (ParamIDs::modAmount (slot))->load() * 100.0f);
        const int polarity = (int) state.getRawParameterValue (ParamIDs::modPolarity (slot))->load();
        const bool position = (int) state.getRawParameterValue (ParamIDs::modDest (slot))->load() == (int) lfoPointDest (lfo, point, true);

        juce::PopupMenu sub;
        sub.addItem (100 + slot, "Moves the height", true, ! position);
        sub.addItem (200 + slot, "Moves the time", point > 0 && point < (int) displayPoints().size() - 1, position);
        sub.addSectionHeader ("DIRECTION");
        sub.addItem (1000 + slot * 4 + (int) ModPolarity::both, "Both ways", true, polarity == (int) ModPolarity::both);
        sub.addItem (1000 + slot * 4 + (int) ModPolarity::up, "Up / later only", true, polarity == (int) ModPolarity::up);
        sub.addItem (1000 + slot * 4 + (int) ModPolarity::down, "Down / earlier only", true, polarity == (int) ModPolarity::down);
        sub.addSeparator();
        sub.addItem (300 + slot, "Invert");
        sub.addItem (400 + slot, "Remove");

        menu.addSubMenu (Choices::modSources()[source] + "  " + (percent > 0 ? "+" : "") + juce::String (percent) + "% "
                             + (position ? "time" : "height"), sub);
    }

    menu.addSeparator();
    menu.addItem (1, "Alt+drag the point to change the depth", false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                        [safeThis = juce::Component::SafePointer<LfoEditor> (this), point] (int result)
    {
        if (safeThis == nullptr || result <= 1)
            return;

        auto& editor = *safeThis;
        auto& parameters = editor.processor.parameters;
        const auto set = [&parameters] (const juce::String& id, float value)
        {
            if (auto* parameter = parameters.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
                parameter->endChangeGesture();
            }
        };

        if (result >= 1000)
        {
            const int slot = (result - 1000) / 4;
            set (ParamIDs::modPolarity (slot), (float) ((result - 1000) % 4));
        }
        else if (result >= 400)
        {
            clearModulationSlot (parameters, result - 400);
        }
        else if (result >= 300)
        {
            const int slot = result - 300;
            set (ParamIDs::modAmount (slot), -parameters.getRawParameterValue (ParamIDs::modAmount (slot))->load());
        }
        else if (result >= 200)
        {
            set (ParamIDs::modDest (result - 200), (float) (int) lfoPointDest (editor.lfo, point, true));
        }
        else if (result >= 100)
        {
            set (ParamIDs::modDest (result - 100), (float) (int) lfoPointDest (editor.lfo, point, false));
        }

        editor.refresh();
    });
}

void LfoEditor::render (bool immediate)
{
    if (shader.isAvailable() && Visuals::wantsShaders (*this))
        shader.render (*this, Visuals::staticScreenFx (true), [this] (juce::Graphics& g) { paintScreen (g, true); }, immediate);
    else
        shader.invalidate();
}

void LfoEditor::timerCallback()
{
    render (drag.target != Target::none);
    repaint();
}

void LfoEditor::refresh()
{
    render (true);
    repaint();
}

void LfoEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    if (! shader.draw (g, getLocalBounds().toFloat()))
        paintScreen (g, false);

    if (dropPoint >= 0)
    {
        // Подсказка, куда встанет модуляция
        const bool position = juce::ModifierKeys::currentModifiers.isAltDown();
        g.setColour (Palette::accentBright);
        g.setFont (makeFont (11.0f, true, 0.06f));
        g.drawText ("POINT " + juce::String (dropPoint + 1) + (position ? ": TIME" : ": HEIGHT  (ALT - TIME)"),
                    bounds.reduced (10.0f, 6.0f), juce::Justification::topLeft);
    }
    else if (! isCustom())
    {
        g.setColour (Palette::textFaint);
        g.setFont (makeFont (10.5f, true, 0.1f));
        g.drawText ("CLICK TO EDIT", bounds.reduced (10.0f, 6.0f), juce::Justification::bottomRight);
    }

    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

void LfoEditor::paintScreen (juce::Graphics& g, bool forShader)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Palette::deep);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto plot = plotArea();

    // Сетка
    for (int i = 1; i < 8; ++i)
    {
        g.setColour (Palette::outline.withAlpha (i == 4 ? 0.9f : 0.45f));
        g.drawVerticalLine (juce::roundToInt (plot.getX() + plot.getWidth() * (float) i / 8.0f), plot.getY(), plot.getBottom());
    }

    for (float y : { -0.5f, 0.5f })
    {
        g.setColour (Palette::outline.withAlpha (0.45f));
        g.drawHorizontalLine (juce::roundToInt (toScreen (0.0f, y).y), plot.getX(), plot.getRight());
    }

    g.setColour (Palette::outline);
    g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());

    // Кривая: живая форма звучащего голоса (с модуляцией точек), под ней - форма из точек как есть
    const bool custom = isCustom();
    const auto shape = currentShape();
    const auto points = displayPoints();
    const auto live = livePoints();
    const bool usePoints = custom || isRandomShape (shape);
    const bool modulated = custom && hasPointModulation();

    const auto makeCurve = [&] (const std::vector<LfoPoint>& source)
    {
        juce::Path path;
        const int steps = juce::jmax (64, (int) plot.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const float x = (float) i / (float) steps;
            const float y = usePoints ? LfoMath::evaluatePoints (source, x)
                                      : LfoMath::evaluate (shape, juce::jmin (x, 0.9999f), 0, 0, nullptr);
            const auto point = toScreen (x, y);

            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }
        return path;
    };

    const auto curve = makeCurve (live);

    if (modulated)
    {
        g.setColour (Palette::textFaint.withAlpha (0.6f));
        g.strokePath (makeCurve (points), { 1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }

    auto fill = curve;
    fill.lineTo (plot.getRight(), plot.getCentreY());
    fill.lineTo (plot.getX(), plot.getCentreY());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.22f), 0.0f, plot.getY(),
                                             Palette::accent.withAlpha (0.02f), 0.0f, plot.getCentreY(), false));
    g.fillPath (fill);

    if (! forShader)
    {
        g.setColour (Palette::accent.withAlpha (0.12f));
        g.strokePath (curve, { 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }

    g.setColour (Palette::accentBright);
    g.strokePath (curve, { 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    // Точки и ручки изгиба
    if (custom)
    {
        for (int i = 0; i + 1 < (int) points.size(); ++i)
        {
            const auto& a = points[(size_t) i];
            const auto& b = points[(size_t) i + 1];
            if (b.x - a.x < 0.02f)
                continue;

            const float midX = 0.5f * (a.x + b.x);
            const auto handle = toScreen (midX, LfoMath::evaluatePoints (points, midX));
            const bool highlighted = (hover.target == Target::curve && hover.index == i)
                                  || (drag.target == Target::curve && drag.index == i);

            g.setColour (highlighted ? Palette::accentBright : Palette::accent.withAlpha (0.7f));
            g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (handle), 1.3f);
        }

        // Диапазоны модуляции точек: по высоте - вертикальная полоса, по времени - горизонтальная
        for (int i = 0; i < juce::jmin ((int) points.size(), kMaxModulatedPoints); ++i)
        {
            if (pointSlots (i).empty())
                continue;

            const auto& point = points[(size_t) i];
            const auto [lowY, highY] = pointSpan (i, false);
            const auto [lowX, highX] = pointSpan (i, true);
            g.setColour (Palette::accent.withAlpha (0.55f));

            if (highY - lowY > 0.001f)
            {
                const auto top = toScreen (point.x, juce::jlimit (-1.0f, 1.0f, point.y + highY));
                const auto bottom = toScreen (point.x, juce::jlimit (-1.0f, 1.0f, point.y + lowY));
                g.fillRoundedRectangle (juce::Rectangle<float> (top.x - 1.5f, top.y, 3.0f, bottom.y - top.y), 1.5f);
            }

            if (highX - lowX > 0.001f)
            {
                const auto left = toScreen (juce::jlimit (0.0f, 1.0f, point.x + lowX), point.y);
                const auto right = toScreen (juce::jlimit (0.0f, 1.0f, point.x + highX), point.y);
                g.fillRoundedRectangle (juce::Rectangle<float> (left.x, left.y - 1.5f, right.x - left.x, 3.0f), 1.5f);
            }

            // Кольцо: у точки есть модуляция; полый кружок - где точка сейчас в звучащем голосе
            const auto centre = toScreen (point.x, point.y);
            g.setColour (Palette::accent);
            g.drawEllipse (juce::Rectangle<float> (kPointRadius * 4.0f, kPointRadius * 4.0f).withCentre (centre), 1.2f);

            if (i < (int) live.size())
            {
                g.setColour (Palette::accentBright);
                g.drawEllipse (juce::Rectangle<float> (kPointRadius * 2.4f, kPointRadius * 2.4f)
                                   .withCentre (toScreen (live[(size_t) i].x, live[(size_t) i].y)), 1.2f);
            }
        }

        for (int i = 0; i < (int) points.size(); ++i)
        {
            const auto position = toScreen (points[(size_t) i].x, points[(size_t) i].y);
            const bool highlighted = (hover.target == Target::point && hover.index == i)
                                  || (drag.target == Target::point && drag.index == i);
            const float radius = highlighted ? kPointRadius + 1.5f : kPointRadius;

            if (highlighted)
            {
                g.setColour (Palette::accent.withAlpha (0.3f));
                g.fillEllipse (juce::Rectangle<float> (radius * 4.0f, radius * 4.0f).withCentre (position));
            }

            g.setColour (Palette::deep);
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f + 2.0f, radius * 2.0f + 2.0f).withCentre (position));
            g.setColour (Palette::accentBright);
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (position));
        }
    }

    // Точка, на которую сейчас бросят источник модуляции (видна и у стандартных форм)
    if (dropPoint >= 0 && dropPoint < (int) points.size())
    {
        const auto centre = toScreen (points[(size_t) dropPoint].x, points[(size_t) dropPoint].y);
        g.setColour (Palette::accent.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (kPointRadius * 6.0f, kPointRadius * 6.0f).withCentre (centre));
        g.setColour (Palette::accentBright);
        g.drawEllipse (juce::Rectangle<float> (kPointRadius * 6.0f, kPointRadius * 6.0f).withCentre (centre), 1.6f);
    }

    // Текущая фаза
    const float phase = processor.lfoDisplayPhase[(size_t) lfo].load();
    const auto dot = toScreen (phase, displayValue (phase));

    g.setColour (Palette::accent.withAlpha (0.2f));
    g.fillRect (juce::Rectangle<float> (dot.x - 0.5f, plot.getY(), 1.0f, plot.getHeight()));
    g.setColour (Palette::accent.withAlpha (0.3f));
    g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (dot));
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (dot));

    if (! forShader)
        glass.draw (g, bounds);
}

} // namespace sonder::ui
