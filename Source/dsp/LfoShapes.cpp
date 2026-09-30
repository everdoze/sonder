#include "LfoShapes.h"
#include "Saturation.h"

namespace sonder
{

float LfoMath::curveShape (float t, float curve) noexcept
{
    if (std::abs (curve) < 1.0e-3f)
        return t;

    // Экспоненциальный изгиб: curve > 0 - медленный старт, curve < 0 - быстрый
    const float k = curve * 8.0f;
    return (std::exp (k * t) - 1.0f) / (std::exp (k) - 1.0f);
}

float LfoMath::randomForCycle (uint32_t seed, uint32_t cycle) noexcept
{
    uint32_t h = cycle * 0x9e3779b1u ^ (seed * 0x85ebca77u + 0x165667b1u);
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    return (float) (h >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

float LfoMath::evaluate (LfoShape shape, float phase, uint32_t cycle, uint32_t seed, const float* customTable) noexcept
{
    switch (shape)
    {
        case LfoShape::sine:          return fastSinCycles (phase);
        case LfoShape::triangle:      return 1.0f - 4.0f * std::abs (phase - 0.5f);
        case LfoShape::sawUp:         return 2.0f * phase - 1.0f;
        case LfoShape::sawDown:       return 1.0f - 2.0f * phase;
        case LfoShape::square:        return phase < 0.5f ? 1.0f : -1.0f;
        case LfoShape::sampleAndHold: return randomForCycle (seed, cycle);

        case LfoShape::smoothRandom:
        {
            // Косинусная интерполяция между случайными точками соседних циклов
            const float t = 0.5f - 0.5f * fastSinCycles (0.5f * phase + 0.25f);
            const float a = randomForCycle (seed, cycle);
            const float b = randomForCycle (seed, cycle + 1);
            return a + (b - a) * t;
        }

        case LfoShape::custom:
        {
            const float position = phase * (float) (LfoShapeBank::kTableSize - 1);
            const int index = juce::jlimit (0, LfoShapeBank::kTableSize - 2, (int) position);
            const float frac = position - (float) index;
            return customTable[index] + (customTable[index + 1] - customTable[index]) * frac;
        }
    }

    return 0.0f;
}

float LfoMath::evaluatePoints (const std::vector<LfoPoint>& points, float x) noexcept
{
    return evaluatePoints (points.data(), (int) points.size(), x);
}

float LfoMath::evaluatePoints (const LfoPoint* points, int count, float x) noexcept
{
    if (count <= 0)
        return 0.0f;

    if (x <= points[0].x)
        return points[0].y;

    for (int i = 0; i + 1 < count; ++i)
    {
        const auto& a = points[i];
        const auto& b = points[i + 1];

        if (x <= b.x)
        {
            const float width = b.x - a.x;
            if (width <= 1.0e-6f)
                return b.y;

            const float t = (x - a.x) / width;
            return a.y + (b.y - a.y) * curveShape (t, a.curve);
        }
    }

    return points[count - 1].y;
}

std::vector<LfoPoint> LfoMath::pointsForShape (LfoShape shape)
{
    switch (shape)
    {
        case LfoShape::sine:
            return { { 0.0f, 0.0f, -0.35f }, { 0.25f, 1.0f, 0.35f }, { 0.5f, 0.0f, -0.35f }, { 0.75f, -1.0f, 0.35f }, { 1.0f, 0.0f, 0.0f } };
        case LfoShape::triangle:
            return { { 0.0f, -1.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } };
        case LfoShape::sawUp:
            return { { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
        case LfoShape::sawDown:
            return { { 0.0f, 1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } };
        case LfoShape::square:
            return { { 0.0f, 1.0f, 0.0f }, { 0.5f, 1.0f, 0.0f }, { 0.5f, -1.0f, 0.0f }, { 1.0f, -1.0f, 0.0f } };

        case LfoShape::sampleAndHold:
        case LfoShape::smoothRandom:
        {
            // Ступеньки из фиксированного "случайного" узора
            std::vector<LfoPoint> result;
            constexpr int steps = 8;
            for (int i = 0; i < steps; ++i)
            {
                const float y = randomForCycle (0x5eedu, (uint32_t) i);
                result.push_back ({ (float) i / steps, y, 0.0f });
                result.push_back ({ (float) (i + 1) / steps, y, 0.0f });
            }
            return result;
        }

        case LfoShape::custom:
            break;
    }

    return pointsForShape (LfoShape::sine);
}

//==============================================================================
LfoShapeBank::LfoShapeBank()
{
    resetAll();
}

std::vector<LfoPoint> LfoShapeBank::getPoints (int lfo) const
{
    const juce::ScopedLock sl (lock);
    return points[(size_t) lfo];
}

void LfoShapeBank::setPoints (int lfo, std::vector<LfoPoint> newPoints)
{
    if (newPoints.size() < 2)
        return;

    if (newPoints.size() > (size_t) LfoPointSet::kMaxPoints)
        newPoints.resize ((size_t) LfoPointSet::kMaxPoints);

    // Концы всегда на краях цикла, точки упорядочены по x
    std::stable_sort (newPoints.begin(), newPoints.end(), [] (const LfoPoint& a, const LfoPoint& b) { return a.x < b.x; });
    newPoints.front().x = 0.0f;
    newPoints.back().x = 1.0f;

    for (auto& p : newPoints)
    {
        p.x = juce::jlimit (0.0f, 1.0f, p.x);
        p.y = juce::jlimit (-1.0f, 1.0f, p.y);
        p.curve = juce::jlimit (-1.0f, 1.0f, p.curve);
    }

    {
        const juce::ScopedLock sl (lock);
        points[(size_t) lfo] = std::move (newPoints);
    }

    rebuildTable (lfo);
}

void LfoShapeBank::resetAll()
{
    for (int lfo = 0; lfo < (int) points.size(); ++lfo)
        setPoints (lfo, LfoMath::pointsForShape (LfoShape::sine));
}

void LfoShapeBank::rebuildTable (int lfo)
{
    const auto current = getPoints (lfo);
    const int writeIndex = 1 - activeTable[(size_t) lfo].load();
    auto& table = tables[(size_t) lfo][(size_t) writeIndex];

    for (int i = 0; i < kTableSize; ++i)
        table[(size_t) i] = LfoMath::evaluatePoints (current, (float) i / (float) (kTableSize - 1));

    auto& set = pointSets[(size_t) lfo][(size_t) writeIndex];
    set.count = (int) current.size();
    std::copy (current.begin(), current.end(), set.points.begin());

    activeTable[(size_t) lfo].store (writeIndex, std::memory_order_release);
    ++version;
}

std::unique_ptr<juce::XmlElement> LfoShapeBank::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("LfoShapes");

    for (int lfo = 0; lfo < (int) points.size(); ++lfo)
    {
        auto* shape = xml->createNewChildElement ("Lfo");
        shape->setAttribute ("index", lfo);

        for (const auto& p : getPoints (lfo))
        {
            auto* point = shape->createNewChildElement ("Point");
            point->setAttribute ("x", p.x);
            point->setAttribute ("y", p.y);
            point->setAttribute ("curve", p.curve);
        }
    }

    return xml;
}

void LfoShapeBank::fromXml (const juce::XmlElement* xml)
{
    resetAll();

    if (xml == nullptr)
        return;

    for (auto* shape : xml->getChildWithTagNameIterator ("Lfo"))
    {
        const int lfo = shape->getIntAttribute ("index", -1);
        if (! juce::isPositiveAndBelow (lfo, (int) points.size()))
            continue;

        std::vector<LfoPoint> loaded;
        for (auto* point : shape->getChildWithTagNameIterator ("Point"))
            loaded.push_back ({ (float) point->getDoubleAttribute ("x"),
                                (float) point->getDoubleAttribute ("y"),
                                (float) point->getDoubleAttribute ("curve") });

        setPoints (lfo, std::move (loaded));
    }
}

} // namespace sonder
