#pragma once

#include "FxRack.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace sonder
{

// Операции над рэком с message thread: добавить, убрать и переставить эффект, сохранить и загрузить.
// Связывает состояние рэка (FxRack) с параметрами слотов в AudioProcessorValueTreeState.
class FxRackController
{
public:
    FxRackController (FxRack& rack, juce::AudioProcessorValueTreeState& state, juce::AudioProcessor& processor);

    const FxRack& getRack() const noexcept { return rack; }

    // Слоты с эффектами в порядке прохождения сигнала
    std::vector<int> getChain() const;

    // Ставит эффект в свободный слот в конец цепочки со значениями по умолчанию; -1, если рэк заполнен
    int addEffect (FxType type);
    void removeEffect (int slot);

    // Переносит эффект на позицию chainPosition в цепочке (0 - первым, размер цепочки - последним)
    void moveEffect (int slot, int chainPosition);

    // Убранный эффект забирает с собой связи мод-матрицы со своими ручками.
    // clear (false) - для загрузки пресета: его матрица уже на месте
    void clear (bool removeModulation = false);

    juce::RangedAudioParameter* getParameter (int slot, int param) const;
    juce::RangedAudioParameter* getOnParameter (int slot) const;

    // Значения ручек в реальных единицах
    float getRealValue (int slot, int param) const;
    void setRealValue (int slot, int param, float real);

    std::unique_ptr<juce::XmlElement> toXml() const;
    void fromXml (const juce::XmlElement& xml);

private:
    void setDefaults (int slot, FxType type);
    void clearModulation (int slot);
    void notifyHost();

    FxRack& rack;
    juce::AudioProcessorValueTreeState& state;
    juce::AudioProcessor& processor;
};

} // namespace sonder
