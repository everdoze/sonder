#pragma once

#include "dsp/LfoShapes.h"
#include "fx/FxTypes.h"

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

// Ручка эффекта: номер из fxp::<эффект> и значение в реальных единицах
struct PresetFxValue
{
    int param;
    float value;
};

// Эффект в рэке пресета; ручки, которых нет в списке, остаются по умолчанию
struct PresetFx
{
    FxType type;
    std::vector<PresetFxValue> values;
};

struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<PresetValue> values;
    std::vector<PresetFx> effects;    // рэк эффектов в порядке прохождения сигнала
    const char* wavetable1 = nullptr; // встроенная wavetable, nullptr - по умолчанию
    const char* wavetable2 = nullptr;

    // Свои формы LFO (номер LFO с нуля и точки); форма включается параметром lfoNShape = 7 (Custom)
    std::vector<std::pair<int, std::vector<LfoPoint>>> lfoShapes {};
};

const std::vector<FactoryPreset>& getFactoryPresets();

} // namespace sonder
