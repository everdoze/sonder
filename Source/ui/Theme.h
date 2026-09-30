#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace sonder::ui
{

// Настройки вида. Общие для всех экземпляров плагина, лежат в %APPDATA%\Sonder\Settings.xml.
struct Settings
{
    int theme = 0;
    int accent = 0;             // 0 - цвет, заданный темой

    bool timbreColour = true;   // цвет волны следует за яркостью тембра
    bool crtScreen = true;      // послесвечение и строки развёртки на экранах
    bool spectrum = true;       // спектр за кривой фильтра
    bool motion = true;         // покачивание wavetable, "дыхание" ручек, плавная смена пресета
    bool analogGlow = true;     // прогрев и просадка питания видны в свечении
    bool shaderFx = true;       // шейдеры на экранах: размытое свечение, выпуклое стекло, расслоение цвета
    bool particles = true;      // искры от волны на осциллографе

    int scopeMode = 0;          // 0 Wave, 1 Roll, 2 XY
    int scopeWindow = 2;        // окно бегущей волны, индекс в ScopeView

    // Открытая страница и вкладка осциллятора: помнятся до закрытия программы, в файл не пишутся
    int page = 0;
    int oscTab = 0;
    int filterTab = 0;

    static Settings& get();
    void save() const;
    static juce::File getFile();
};

struct ThemeInfo
{
    const char* name;
    float hue;          // оттенок фона и панелей, < 0 - как в исходной палитре
    float saturation;   // множитель насыщенности фона
    juce::uint32 accent;
};

struct AccentInfo
{
    const char* name;
    juce::uint32 colour;
};

const std::vector<ThemeInfo>& getThemes();
const std::vector<AccentInfo>& getAccents(); // первый пункт - "цвет темы"

// Заполняет Palette по настройкам. Компоненты, которые запомнили цвета при создании,
// после этого нужно пересоздать.
void applyTheme (const Settings& settings);

// Цвет из исходной (бирюзовой) палитры, перенесённый в текущую тему
juce::Colour themed (juce::uint32 argb);

// Сдвиг цвета по "температуре": t = -1 темнее и глубже (в сторону синего), +1 светлее и белее
juce::Colour shiftColour (juce::Colour base, float t);

} // namespace sonder::ui
