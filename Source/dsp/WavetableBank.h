#pragma once

#include "Parameters.h"
#include "Wavetable.h"

#include <atomic>
#include <map>

namespace sonder
{

// Выбор wavetable для каждого осциллятора. Выбор меняет UI (message thread);
// аудиопоток читает сырой указатель. Загруженные таблицы живут в кэше до конца работы,
// поэтому указатель в аудиопотоке никогда не становится висячим.
class WavetableBank
{
public:
    static constexpr int kNumOscillators = kNumOscs;

    WavetableBank();

    // Идентификаторы вида "builtin:Vocal" или "user:MyTable"
    bool select (int oscillator, const juce::String& id);

    // Копирует WAV в папку пользовательских таблиц и выбирает его
    bool importFile (int oscillator, const juce::File& file);

    juce::String getSelectedId (int oscillator) const;
    juce::String getDisplayName (int oscillator) const;
    int getVersion() const noexcept { return version.load(); }

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement* xml);
    void resetToDefault();

    static juce::File getUserDirectory();
    static juce::Array<juce::File> getUserFiles();

    // Аудиопоток и отрисовка
    const Wavetable* get (int oscillator) const noexcept { return current[(size_t) oscillator].load (std::memory_order_acquire); }

private:
    std::shared_ptr<const Wavetable> obtain (const juce::String& id);

    std::map<juce::String, std::shared_ptr<const Wavetable>> cache;
    std::array<std::atomic<const Wavetable*>, kNumOscillators> current {};
    std::array<juce::String, kNumOscillators> selected;
    std::atomic<int> version { 0 };
};

} // namespace sonder
