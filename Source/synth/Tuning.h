#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <vector>

namespace sonder
{

// Строй: для каждой MIDI-ноты своя высота в "MIDI-единицах" (69.0 = 440 Гц, дробная часть - доли полутона).
//
// Обычно строй - это гамма: ступени в центах от тоники (первая ступень - 0), период (обычно октава, 1200 центов)
// и клавиша тоники. Тоника звучит там же, где в равномерном строе, остальные клавиши отсчитываются от неё
// по ступеням. Гамму можно загрузить из Scala (.scl), выбрать из готовых или настроить вручную.
// Файл AnaMark (.tun) задаёт высоту каждой клавиши отдельно - это таблица, её ступени не редактируются.
// Частота ля (A4) сдвигает весь строй.
//
// Таблицу высот читает аудиопоток (атомарно, по одной ноте), меняет message thread.
class Tuning
{
public:
    static constexpr int kNumNotes = 128;

    Tuning() { reset(); }

    float pitchFor (int note) const noexcept
    {
        return table[(size_t) juce::jlimit (0, kNumNotes - 1, note)].load (std::memory_order_relaxed);
    }

    // Равномерный 12-тоновый строй, ля 440 Гц
    void reset();

    // Готовые строи
    static const juce::StringArray& getPresetNames();
    void loadPreset (int index);

    // false - файл не разобран, error - почему
    bool loadFile (const juce::File& file, juce::String& error);
    bool loadScala (const juce::String& text, const juce::String& fallbackName, juce::String& error);
    bool loadTun (const juce::String& text, const juce::String& fallbackName, juce::String& error);
    bool saveScala (const juce::File& file) const;
    juce::String toScala() const;

    // Ручная настройка гаммы (только пока строй - гамма, а не таблица из .tun)
    bool isTable() const noexcept { return tableMode; }
    int getNumDegrees() const noexcept { return (int) degrees.size(); }
    double getDegree (int degree) const noexcept { return degrees[(size_t) degree]; }
    void setDegree (int degree, double cents);
    double getPeriod() const noexcept { return period; }
    void setPeriod (double cents);
    int getRootNote() const noexcept { return rootNote; }
    void setRootNote (int note);

    // Частота ля первой октавы: сдвигает и гамму, и таблицу
    double getReference() const noexcept { return referenceHz; }
    void setReference (double hz);

    // Для интерфейса (message thread)
    const juce::String& getName() const noexcept { return name; }
    const juce::String& getDescription() const noexcept { return description; }
    bool isEqualTemperament() const noexcept;
    int getVersion() const noexcept { return version.load(); }

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement* xml);

private:
    void setScale (std::vector<double> newDegrees, double newPeriod, const juce::String& newName, const juce::String& newDescription);
    void markEdited();
    void rebuild();

    std::array<std::atomic<float>, kNumNotes> table {};

    bool tableMode = false;
    std::vector<double> degrees;             // [0] = 0, дальше ступени в центах от тоники
    double period = 1200.0;
    int rootNote = 60;
    double referenceHz = 440.0;
    std::array<float, kNumNotes> fixedTable {}; // высоты из .tun при ля 440

    juce::String name, description;
    std::atomic<int> version { 0 };
};

} // namespace sonder
