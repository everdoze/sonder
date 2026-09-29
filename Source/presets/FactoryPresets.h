#pragma once

#include <vector>

namespace sonder
{

// Заводской пресет: только отличия от значений по умолчанию.
// Значения в "человеческих" единицах: Гц, секунды, индексы вариантов для choice-параметров.
struct PresetValue
{
    const char* id;
    float value;
};

struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<PresetValue> values;
    const char* wavetable1 = nullptr; // встроенная wavetable, nullptr - по умолчанию
    const char* wavetable2 = nullptr;
};

const std::vector<FactoryPreset>& getFactoryPresets();

} // namespace sonder
