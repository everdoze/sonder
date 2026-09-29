#include "LfoEditor.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"

namespace sonder::ui
{

namespace
{
    constexpr float kPointRadius = 4.5f;
    constexpr float kHitRadius = 9.0f;

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
    repaint();
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

float LfoEditor::displayValue (float phase) const
{
    const auto shape = currentShape();
    if (shape == LfoShape::custom || isRandomShape (shape))
        return LfoMath::evaluatePoints (displayPoints(), phase);

    return LfoMath::evaluate (shape, juce::jmin (phase, 0.9999f), 0, 0, nullptr);
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
        repaint();
    }

    setMouseCursor (hit.target == Target::point ? juce::MouseCursor::DraggingHandCursor
                    : hit.target == Target::curve ? juce::MouseCursor::UpDownResizeCursor
                                                  : juce::MouseCursor::NormalCursor);
}

void LfoEditor::mouseExit (const juce::MouseEvent&)
{
    hover = {};
    repaint();
}

void LfoEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showMenu();
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
    repaint();
}

void LfoEditor::mouseUp (const juce::MouseEvent&)
{
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
            points.erase (points.begin() + hit.index);
    }
    else if (hit.target == Target::curve)
    {
        points[(size_t) hit.index].curve = 0.0f;
    }
    else
    {
        const auto position = fromScreen (e.position);
        if (position.x > 0.0f && position.x < 1.0f)
            points.push_back ({ position.x, position.y, 0.0f });
    }

    processor.lfoShapes.setPoints (lfo, std::move (points));
    hover = {};
    repaint();
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
                // Разворот во времени: изгиб сегмента меняет знак
                std::vector<LfoPoint> flipped;
                for (int i = (int) points.size() - 1; i >= 0; --i)
                {
                    const float curve = i > 0 ? -points[(size_t) i - 1].curve : 0.0f;
                    flipped.push_back ({ 1.0f - points[(size_t) i].x, points[(size_t) i].y, curve });
                }
                points = flipped;
                break;
            }

            default:
                return;
        }

        editor.processor.lfoShapes.setPoints (editor.lfo, std::move (points));
        editor.repaint();
    });
}

void LfoEditor::paint (juce::Graphics& g)
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

    // Кривая
    const bool custom = isCustom();
    const auto shape = currentShape();
    const auto points = displayPoints();
    const bool usePoints = custom || isRandomShape (shape);

    juce::Path curve;
    const int steps = juce::jmax (64, (int) plot.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) i / (float) steps;
        const float y = usePoints ? LfoMath::evaluatePoints (points, x)
                                  : LfoMath::evaluate (shape, juce::jmin (x, 0.9999f), 0, 0, nullptr);
        const auto point = toScreen (x, y);

        if (i == 0)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    auto fill = curve;
    fill.lineTo (plot.getRight(), plot.getCentreY());
    fill.lineTo (plot.getX(), plot.getCentreY());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.22f), 0.0f, plot.getY(),
                                             Palette::accent.withAlpha (0.02f), 0.0f, plot.getCentreY(), false));
    g.fillPath (fill);

    g.setColour (Palette::accent.withAlpha (0.12f));
    g.strokePath (curve, { 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
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
    else
    {
        g.setColour (Palette::textFaint);
        g.setFont (makeFont (10.5f, true, 0.1f));
        g.drawText ("CLICK TO EDIT", bounds.reduced (10.0f, 6.0f), juce::Justification::bottomRight);
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

    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

} // namespace sonder::ui
