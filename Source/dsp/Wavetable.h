#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace sonder
{

// Wavetable из кадров по 2048 сэмплов. Для каждого кадра хранятся mip-уровни с ограниченным
// числом гармоник: высокие ноты читают "обрезанные" версии и не дают алиасинга.
class Wavetable
{
public:
    static constexpr int kFrameSize = 2048;
    static constexpr int kNumLevels = 11;   // 1024, 512, ... 1 гармоника
    static constexpr int kMaxFrames = 256;

    // Какой mip-уровень читать. blend - доля следующего, более "тёмного" уровня:
    // к границе октавы он плавно берёт верх, поэтому яркость не скачет при глайде.
    struct MipSelection
    {
        int level = 0;
        float blend = 0.0f;
    };

    // frames: по kFrameSize сэмплов каждый
    static std::shared_ptr<const Wavetable> create (const juce::String& name, const std::vector<std::vector<float>>& frames);

    // Один кадр, заданный гармониками: amplitude (k) возвращает амплитуды синуса и косинуса k-й гармоники.
    // Каждый уровень собирается точно (без БПФ исходного кадра) и хранится в kClassicSize отсчётах,
    // чтобы и линейная интерполяция не добавляла грязи. Без нормировки: амплитуды ровно такие, как заданы.
    static constexpr int kClassicSize = 4096;
    static std::shared_ptr<const Wavetable> createHarmonic (const juce::String& name,
                                                            const std::function<std::pair<double, double> (int)>& amplitude);

    const juce::String& getName() const noexcept { return name; }
    int getNumFrames() const noexcept { return numFrames; }

    // Выбор mip-уровня для отношения частоты к частоте дискретизации
    static MipSelection selectMip (float frequencyOverSampleRate) noexcept;

    // phase в [0, 1), position в [0, 1] - позиция морфа по кадрам
    float sample (float phase, float position, MipSelection mip) const noexcept;

    // Быстрое чтение однокадровой таблицы (классические формы): без смешивания кадров
    float sampleSingle (float phase, MipSelection mip) const noexcept
    {
        const auto readLevel = [phase] (const Level& level)
        {
            const float position = phase * (float) level.size;
            const int i0 = (int) position;
            const float frac = position - (float) i0;
            const int mask = level.size - 1;
            const float a = level.data[(size_t) (i0 & mask)];
            return a + (level.data[(size_t) ((i0 + 1) & mask)] - a) * frac;
        };

        const float a = readLevel (levels[(size_t) mip.level]);
        if (mip.blend <= 0.001f || mip.level + 1 >= kNumLevels)
            return a;

        return a + (readLevel (levels[(size_t) mip.level + 1]) - a) * mip.blend;
    }

    // Полноразрешающий кадр для отрисовки
    const float* getFrame (int frame) const noexcept;

private:
    struct Level
    {
        int size = 0;
        std::vector<float> data; // numFrames * size
    };

    float read (const Level& level, int frame, float phase) const noexcept;
    float sampleLevel (const Level& level, float phase, float position) const noexcept;

    juce::String name;
    int numFrames = 0;
    std::array<Level, kNumLevels> levels;
};

namespace Wavetables
{
    // Классические формы с ограниченным спектром для обычных осцилляторов: пила и треугольник
    // с точными амплитудами гармоник, чистый синус. Создаются при первом вызове (не из аудиопотока:
    // Voice::prepare вызывает их заранее).
    const Wavetable& classicSaw();
    const Wavetable& classicTriangle();
    const Wavetable& classicSine();

    // Встроенные таблицы генерируются в коде
    const juce::StringArray& builtInNames();
    std::shared_ptr<const Wavetable> createBuiltIn (const juce::String& name);

    // WAV в формате Serum (кадры по 2048), одиночный цикл или произвольный файл
    std::shared_ptr<const Wavetable> loadFromFile (const juce::File& file);
}

} // namespace sonder
