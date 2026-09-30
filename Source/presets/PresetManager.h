#pragma once

#include "dsp/LfoShapes.h"
#include "dsp/WavetableBank.h"
#include "fx/FxRackController.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

namespace sonder
{

// Заводские пресеты (из кода) и пользовательские (XML-файлы в %APPDATA%\Sonder\Presets).
// Все методы вызываются с message thread; об изменениях сообщает через ChangeBroadcaster.
class PresetManager : public juce::ChangeBroadcaster
{
public:
    struct Preset
    {
        juce::String name;
        juce::String category; // для пользовательских "User"
        bool isFactory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    PresetManager (juce::AudioProcessorValueTreeState& state, LfoShapeBank& lfoShapes, WavetableBank& wavetables,
                   FxRackController& fxRack);

    void refresh();

    const std::vector<Preset>& getPresets() const noexcept { return presets; }
    int getNumFactoryPresets() const noexcept;
    int getCurrentIndex() const noexcept { return currentIndex; }
    juce::String getCurrentName() const;

    void load (int index);
    void loadNext();
    void loadPrevious();

    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);

    // После восстановления состояния из проекта DAW находим пресет по сохранённому имени
    void syncWithState();

    // Вызывается перед тем, как пресет поменяет параметры: интерфейс запоминает положение ручек,
    // чтобы плавно довести их до новых значений
    std::function<void()> onBeforeLoad;

    static juce::File getUserPresetDirectory();

private:
    void applyValues (const std::map<juce::String, float>& values);
    void setCurrent (int index);

    juce::AudioProcessorValueTreeState& state;
    LfoShapeBank& lfoShapes;
    WavetableBank& wavetables;
    FxRackController& fxRack;
    std::vector<Preset> presets;
    int currentIndex = 0;
};

} // namespace sonder
