#include "PresetManager.h"
#include "FactoryPresets.h"

namespace sonder
{

namespace
{
    const juce::Identifier presetNameProperty { "presetName" };
    const juce::String presetExtension { ".sonderpreset" };
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, LfoShapeBank& shapes, WavetableBank& tables,
                              FxRackController& rack)
    : state (s), lfoShapes (shapes), wavetables (tables), fxRack (rack)
{
    refresh();
}

juce::File PresetManager::getUserPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Sonder")
               .getChildFile ("Presets");
}

void PresetManager::refresh()
{
    const auto previousName = getCurrentName();
    presets.clear();

    const auto& factory = getFactoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        presets.push_back ({ factory[(size_t) i].name, factory[(size_t) i].category, true, i, {} });

    auto files = getUserPresetDirectory().findChildFiles (juce::File::findFiles, false, "*" + presetExtension);
    files.sort();

    for (const auto& file : files)
        presets.push_back ({ file.getFileNameWithoutExtension(), "User", false, -1, file });

    currentIndex = 0;
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].name == previousName)
            currentIndex = i;

    sendChangeMessage();
}

int PresetManager::getNumFactoryPresets() const noexcept
{
    return (int) getFactoryPresets().size();
}

juce::String PresetManager::getCurrentName() const
{
    return state.state.getProperty (presetNameProperty, "Init").toString();
}

void PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    if (onBeforeLoad != nullptr)
        onBeforeLoad();

    const auto& preset = presets[(size_t) index];
    std::map<juce::String, float> values;
    std::unique_ptr<juce::XmlElement> xml;

    if (preset.isFactory)
    {
        for (const auto& [id, value] : getFactoryPresets()[(size_t) preset.factoryIndex].values)
            values[id] = value;
    }
    else
    {
        xml = juce::parseXML (preset.file);
        if (xml == nullptr)
            return;

        for (auto* param : xml->getChildWithTagNameIterator ("Param"))
            values[param->getStringAttribute ("id")] = (float) param->getDoubleAttribute ("value");
    }

    applyValues (values);

    if (preset.isFactory)
    {
        const auto& factory = getFactoryPresets()[(size_t) preset.factoryIndex];

        lfoShapes.resetAll();
        wavetables.resetToDefault();

        if (factory.wavetable1 != nullptr)
            wavetables.select (0, juce::String ("builtin:") + factory.wavetable1);
        if (factory.wavetable2 != nullptr)
            wavetables.select (1, juce::String ("builtin:") + factory.wavetable2);

        for (const auto& [lfo, points] : factory.lfoShapes)
            lfoShapes.setPoints (lfo, points);

        // Рэк эффектов: ручки, не указанные в пресете, остаются по умолчанию
        fxRack.clear();

        for (const auto& effect : factory.effects)
        {
            const int slot = fxRack.addEffect (effect.type);
            for (const auto& [param, value] : effect.values)
                fxRack.setRealValue (slot, param, value);
        }
    }
    else
    {
        lfoShapes.fromXml (xml->getChildByName ("LfoShapes"));
        wavetables.fromXml (xml->getChildByName ("Wavetables"));

        if (const auto* rack = xml->getChildByName ("FxRack"))
            fxRack.fromXml (*rack);
        else
            fxRack.clear();
    }

    setCurrent (index);
}

void PresetManager::loadNext()
{
    if (! presets.empty())
        load ((currentIndex + 1) % (int) presets.size());
}

void PresetManager::loadPrevious()
{
    if (! presets.empty())
        load ((currentIndex + (int) presets.size() - 1) % (int) presets.size());
}

void PresetManager::applyValues (const std::map<juce::String, float>& values)
{
    // Параметры, которых нет в пресете, возвращаются к значениям по умолчанию
    for (auto* parameter : state.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
        if (ranged == nullptr)
            continue;

        const auto it = values.find (ranged->getParameterID());
        const float normalised = it != values.end() ? ranged->convertTo0to1 (it->second)
                                                    : ranged->getDefaultValue();

        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost (normalised);
        ranged->endChangeGesture();
    }
}

void PresetManager::setCurrent (int index)
{
    currentIndex = index;
    state.state.setProperty (presetNameProperty, presets[(size_t) index].name, nullptr);
    sendChangeMessage();
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty())
        return false;

    const auto directory = getUserPresetDirectory();
    if (! directory.createDirectory())
        return false;

    juce::XmlElement xml ("SonderPreset");
    xml.setAttribute ("name", trimmed);
    xml.setAttribute ("version", 3);

    for (auto* parameter : state.processor.getParameters())
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            auto* child = xml.createNewChildElement ("Param");
            child->setAttribute ("id", ranged->getParameterID());
            child->setAttribute ("value", ranged->convertFrom0to1 (ranged->getValue()));
        }
    }

    xml.addChildElement (fxRack.toXml().release());
    xml.addChildElement (lfoShapes.toXml().release());
    xml.addChildElement (wavetables.toXml().release());

    const auto file = directory.getChildFile (juce::File::createLegalFileName (trimmed) + presetExtension);
    if (! xml.writeTo (file))
        return false;

    state.state.setProperty (presetNameProperty, file.getFileNameWithoutExtension(), nullptr);
    refresh();
    return true;
}

bool PresetManager::deleteUserPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size()) || presets[(size_t) index].isFactory)
        return false;

    if (! presets[(size_t) index].file.deleteFile())
        return false;

    refresh();
    return true;
}

void PresetManager::syncWithState()
{
    const auto name = getCurrentName();

    for (int i = 0; i < (int) presets.size(); ++i)
    {
        if (presets[(size_t) i].name == name)
        {
            currentIndex = i;
            break;
        }
    }

    sendChangeMessage();
}

} // namespace sonder
