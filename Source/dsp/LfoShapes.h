#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <vector>

namespace sonder
{

inline constexpr int kLfoShapeCount = 8;

// Порядок совпадает с Choices::lfoShapes()
enum class LfoShape { sine, triangle, sawUp, sawDown, square, sampleAndHold, smoothRandom, custom };
enum class LfoMode { free, retrigger, envelope };

// Точка пользовательской формы: x в [0, 1], y в [-1, 1],
// curve в [-1, 1] - изгиб сегмента от этой точки до следующей
struct LfoPoint
{
    float x = 0.0f, y = 0.0f, curve = 0.0f;
};

// Точки формы без выделения памяти: для аудиопотока
struct LfoPointSet
{
    static constexpr int kMaxPoints = 64;

    int count = 0;
    std::array<LfoPoint, kMaxPoints> points {};
};

namespace LfoMath
{
    // Значение изогнутого сегмента для t в [0, 1]
    float curveShape (float t, float curve) noexcept;

    // Детерминированное "случайное" число в [-1, 1] для цикла LFO: одинаковое во всех голосах с одним seed
    float randomForCycle (uint32_t seed, uint32_t cycle) noexcept;

    // Значение LFO в [-1, 1]. customTable - таблица пользовательской формы (для LfoShape::custom)
    float evaluate (LfoShape shape, float phase, uint32_t cycle, uint32_t seed, const float* customTable) noexcept;

    // Значение ломаной с изгибами в точке x
    float evaluatePoints (const std::vector<LfoPoint>& points, float x) noexcept;
    float evaluatePoints (const LfoPoint* points, int count, float x) noexcept;

    // Стандартные формы в виде точек: с них начинается редактирование
    std::vector<LfoPoint> pointsForShape (LfoShape shape);
}

// Пользовательские формы всех LFO. Точки правит UI (message thread), аудиопоток читает
// заранее посчитанные таблицы через двойную буферизацию без блокировок.
class LfoShapeBank
{
public:
    static constexpr int kTableSize = 1024;

    LfoShapeBank();

    std::vector<LfoPoint> getPoints (int lfo) const;
    void setPoints (int lfo, std::vector<LfoPoint> newPoints);
    void resetAll();

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement* xml);

    // Счётчик изменений: UI перерисовывается, когда он меняется
    int getVersion() const noexcept { return version.load(); }

    // Аудиопоток
    const float* getTable (int lfo) const noexcept
    {
        return tables[(size_t) lfo][(size_t) activeTable[(size_t) lfo].load (std::memory_order_acquire)].data();
    }

    const LfoPointSet* getPointSet (int lfo) const noexcept
    {
        return &pointSets[(size_t) lfo][(size_t) activeTable[(size_t) lfo].load (std::memory_order_acquire)];
    }

private:
    void rebuildTable (int lfo);

    mutable juce::CriticalSection lock;
    std::array<std::vector<LfoPoint>, 8> points;
    std::array<std::array<std::array<float, kTableSize>, 2>, 8> tables {};
    std::array<std::array<LfoPointSet, 2>, 8> pointSets {};
    std::array<std::atomic<int>, 8> activeTable {};
    std::atomic<int> version { 0 };
};

} // namespace sonder
