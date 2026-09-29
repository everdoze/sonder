#include "WavetableBank.h"

namespace sonder
{

namespace
{
    const juce::String defaultId { "builtin:Basic Shapes" };
    const juce::String builtInPrefix { "builtin:" };
    const juce::String userPrefix { "user:" };
}

WavetableBank::WavetableBank()
{
    resetToDefault();
}

juce::File WavetableBank::getUserDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Sonder")
               .getChildFile ("Wavetables");
}

juce::Array<juce::File> WavetableBank::getUserFiles()
{
    auto files = getUserDirectory().findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
    files.sort();
    return files;
}

std::shared_ptr<const Wavetable> WavetableBank::obtain (const juce::String& id)
{
    if (auto it = cache.find (id); it != cache.end())
        return it->second;

    std::shared_ptr<const Wavetable> table;

    if (id.startsWith (builtInPrefix))
    {
        table = Wavetables::createBuiltIn (id.fromFirstOccurrenceOf (builtInPrefix, false, false));
    }
    else if (id.startsWith (userPrefix))
    {
        const auto name = id.fromFirstOccurrenceOf (userPrefix, false, false);
        for (const auto& file : getUserFiles())
            if (file.getFileNameWithoutExtension() == name)
                table = Wavetables::loadFromFile (file);
    }

    if (table != nullptr)
        cache[id] = table;

    return table;
}

bool WavetableBank::select (int oscillator, const juce::String& id)
{
    auto table = obtain (id);
    if (table == nullptr)
        return false;

    selected[(size_t) oscillator] = id;
    current[(size_t) oscillator].store (table.get(), std::memory_order_release);
    ++version;
    return true;
}

bool WavetableBank::importFile (int oscillator, const juce::File& file)
{
    if (Wavetables::loadFromFile (file) == nullptr)
        return false;

    const auto directory = getUserDirectory();
    directory.createDirectory();

    const auto target = directory.getChildFile (file.getFileName());
    if (target != file && ! file.copyFileTo (target))
        return false;

    // Файл мог быть перезаписан новым содержимым - сбрасываем кэш по этому имени
    const auto id = userPrefix + target.getFileNameWithoutExtension();
    cache.erase (id);
    return select (oscillator, id);
}

juce::String WavetableBank::getSelectedId (int oscillator) const
{
    return selected[(size_t) oscillator];
}

juce::String WavetableBank::getDisplayName (int oscillator) const
{
    if (const auto* table = get (oscillator))
        return table->getName();

    return {};
}

void WavetableBank::resetToDefault()
{
    for (int osc = 0; osc < kNumOscillators; ++osc)
        select (osc, defaultId);
}

std::unique_ptr<juce::XmlElement> WavetableBank::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("Wavetables");
    for (int osc = 0; osc < kNumOscillators; ++osc)
        xml->setAttribute ("osc" + juce::String (osc + 1), selected[(size_t) osc]);

    return xml;
}

void WavetableBank::fromXml (const juce::XmlElement* xml)
{
    for (int osc = 0; osc < kNumOscillators; ++osc)
    {
        const auto id = xml != nullptr ? xml->getStringAttribute ("osc" + juce::String (osc + 1)) : juce::String();

        // Пропавший пользовательский файл - откатываемся на базовую таблицу
        if (id.isEmpty() || ! select (osc, id))
            select (osc, defaultId);
    }
}

} // namespace sonder
