#pragma once

#include <juce_core/juce_core.h>

#include <array>
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

    // frames: по kFrameSize сэмплов каждый
    static std::shared_ptr<const Wavetable> create (const juce::String& name, const std::vector<std::vector<float>>& frames);

    const juce::String& getName() const noexcept { return name; }
    int getNumFrames() const noexcept { return numFrames; }

    // Индекс mip-уровня для отношения частоты к частоте дискретизации
    static int levelForFrequency (float frequencyOverSampleRate) noexcept;

    // phase в [0, 1), position в [0, 1] - позиция морфа по кадрам
    float sample (float phase, float position, int level) const noexcept;

    // Полноразрешающий кадр для отрисовки
    const float* getFrame (int frame) const noexcept;

private:
    struct Level
    {
        int size = 0;
        std::vector<float> data; // numFrames * size
    };

    float read (const Level& level, int frame, float phase) const noexcept;

    juce::String name;
    int numFrames = 0;
    std::array<Level, kNumLevels> levels;
};

namespace Wavetables
{
    // Встроенные таблицы генерируются в коде
    const juce::StringArray& builtInNames();
    std::shared_ptr<const Wavetable> createBuiltIn (const juce::String& name);

    // WAV в формате Serum (кадры по 2048), одиночный цикл или произвольный файл
    std::shared_ptr<const Wavetable> loadFromFile (const juce::File& file);
}

} // namespace sonder
