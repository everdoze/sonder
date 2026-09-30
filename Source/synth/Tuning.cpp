#include "Tuning.h"

namespace sonder
{

namespace
{
    // Строка без комментария и пробелов по краям
    juce::String cleanLine (const juce::String& line, const char* commentStart)
    {
        return line.upToFirstOccurrenceOf (commentStart, false, false).trim();
    }

    double ratioToCents (double numerator, double denominator)
    {
        return 1200.0 * std::log2 (numerator / denominator);
    }

    // Высота ступени Scala в центах: "701.955" - центы, "3/2" или "2" - отношение частот
    bool parseScalaPitch (const juce::String& text, double& cents)
    {
        const auto token = text.trim().upToFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf ("\t", false, false);
        if (token.isEmpty())
            return false;

        if (token.containsChar ('.'))
        {
            cents = token.getDoubleValue();
            return true;
        }

        const double numerator = token.upToFirstOccurrenceOf ("/", false, false).getDoubleValue();
        const double denominator = token.containsChar ('/') ? token.fromFirstOccurrenceOf ("/", false, false).getDoubleValue() : 1.0;

        if (numerator <= 0.0 || denominator <= 0.0)
            return false;

        cents = ratioToCents (numerator, denominator);
        return true;
    }

    struct Preset
    {
        const char* name;
        const char* description;
        std::vector<double> degrees; // без первой ступени (0) и без периода
        double period = 1200.0;
    };

    std::vector<double> ratios (std::initializer_list<std::pair<int, int>> list)
    {
        std::vector<double> result;
        for (const auto& [numerator, denominator] : list)
            result.push_back (ratioToCents (numerator, denominator));
        return result;
    }

    std::vector<double> equalSteps (int count, double period = 1200.0)
    {
        std::vector<double> result;
        for (int i = 1; i < count; ++i)
            result.push_back (period * i / count);
        return result;
    }

    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> list {
            { "12-TET", "12-tone equal temperament", equalSteps (12) },
            { "Just intonation", "5-limit just intonation: pure thirds and fifths in the key of the root",
              ratios ({ { 16, 15 }, { 9, 8 }, { 6, 5 }, { 5, 4 }, { 4, 3 }, { 45, 32 }, { 3, 2 }, { 8, 5 }, { 5, 3 }, { 9, 5 }, { 15, 8 } }) },
            { "Pythagorean", "Pure fifths stacked from the root; bright wide thirds",
              ratios ({ { 256, 243 }, { 9, 8 }, { 32, 27 }, { 81, 64 }, { 4, 3 }, { 729, 512 }, { 3, 2 }, { 128, 81 }, { 27, 16 },
                        { 16, 9 }, { 243, 128 } }) },
            { "Quarter-comma meantone", "Renaissance keyboard tuning: pure major thirds, one 'wolf' fifth",
              { 76.049, 193.157, 310.264, 386.314, 503.422, 579.471, 696.578, 772.627, 889.735, 1006.843, 1082.892 } },
            { "Werckmeister III", "Baroque well temperament: every key playable, each with its own colour",
              { 90.225, 192.180, 294.135, 390.225, 498.045, 588.270, 696.090, 792.180, 888.270, 996.090, 1092.180 } },
            { "Kirnberger III", "Well temperament with a pure major third on the root",
              { 90.225, 193.157, 294.135, 386.314, 498.045, 590.224, 696.578, 792.180, 889.735, 996.090, 1088.269 } },
            { "24-TET quarter tones", "24 equal steps per octave: every key is a quarter tone", equalSteps (24) },
            { "19-TET", "19 equal steps per octave: close to meantone, very smooth thirds", equalSteps (19) },
            { "Bohlen-Pierce", "13 equal steps of a 'tritave' (3:1) instead of an octave", equalSteps (13, ratioToCents (3, 1)),
              ratioToCents (3, 1) },
        };

        return list;
    }
}

//==============================================================================
void Tuning::reset()
{
    referenceHz = 440.0;
    rootNote = 60;
    loadPreset (0);
}

const juce::StringArray& Tuning::getPresetNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray result;
        for (const auto& preset : presets())
            result.add (preset.name);
        return result;
    }();

    return names;
}

void Tuning::loadPreset (int index)
{
    const auto& preset = presets()[(size_t) juce::jlimit (0, (int) presets().size() - 1, index)];

    std::vector<double> newDegrees { 0.0 };
    newDegrees.insert (newDegrees.end(), preset.degrees.begin(), preset.degrees.end());
    setScale (std::move (newDegrees), preset.period, preset.name, preset.description);
}

void Tuning::setScale (std::vector<double> newDegrees, double newPeriod, const juce::String& newName, const juce::String& newDescription)
{
    tableMode = false;
    degrees = std::move (newDegrees);
    period = newPeriod;
    name = newName;
    description = newDescription;
    rebuild();
}

void Tuning::rebuild()
{
    const double shift = 12.0 * std::log2 (referenceHz / 440.0);

    for (int note = 0; note < kNumNotes; ++note)
    {
        double pitch = 0.0;

        if (tableMode)
        {
            pitch = fixedTable[(size_t) note] + shift;
        }
        else
        {
            const int count = (int) degrees.size();
            const int offset = note - rootNote;
            const int octave = (int) std::floor ((double) offset / count);
            const int degree = offset - octave * count;
            pitch = rootNote + (octave * period + degrees[(size_t) degree]) / 100.0 + shift;
        }

        table[(size_t) note].store ((float) pitch, std::memory_order_relaxed);
    }

    ++version;
}

bool Tuning::isEqualTemperament() const noexcept
{
    if (tableMode || degrees.size() != 12 || std::abs (period - 1200.0) > 0.001 || std::abs (referenceHz - 440.0) > 0.001)
        return false;

    for (size_t i = 0; i < degrees.size(); ++i)
        if (std::abs (degrees[i] - 100.0 * (double) i) > 0.001)
            return false;

    return true;
}

void Tuning::markEdited()
{
    if (isEqualTemperament())
    {
        name = "12-TET";
        description = "12-tone equal temperament";
        return;
    }

    if (! name.endsWith (" (edited)"))
        name = (name.isEmpty() ? juce::String ("Custom") : name) + " (edited)";
}

void Tuning::setDegree (int degree, double cents)
{
    if (tableMode || ! juce::isPositiveAndBelow (degree, (int) degrees.size()) || degree == 0)
        return;

    // Ступени не перескакивают через соседние
    const double lower = degrees[(size_t) degree - 1] + 1.0;
    const double upper = (degree + 1 < (int) degrees.size() ? degrees[(size_t) degree + 1] : period) - 1.0;
    degrees[(size_t) degree] = juce::jlimit (lower, upper, cents);
    markEdited();
    rebuild();
}

void Tuning::setPeriod (double cents)
{
    if (tableMode)
        return;

    period = juce::jmax (degrees.back() + 1.0, juce::jlimit (600.0, 2400.0, cents));
    markEdited();
    rebuild();
}

void Tuning::setRootNote (int note)
{
    rootNote = juce::jlimit (0, kNumNotes - 1, note);
    rebuild();
}

void Tuning::setReference (double hz)
{
    referenceHz = juce::jlimit (380.0, 500.0, hz);
    rebuild();
}

//==============================================================================
bool Tuning::loadFile (const juce::File& file, juce::String& error)
{
    const auto text = file.loadFileAsString();
    if (text.isEmpty())
    {
        error = "The file is empty or cannot be read";
        return false;
    }

    if (file.hasFileExtension ("tun"))
        return loadTun (text, file.getFileNameWithoutExtension(), error);

    return loadScala (text, file.getFileNameWithoutExtension(), error);
}

bool Tuning::loadScala (const juce::String& text, const juce::String& fallbackName, juce::String& error)
{
    // Формат Scala: строки с "!" - комментарии; первая строка - описание, вторая - число ступеней,
    // дальше ступени. Последняя ступень - период (обычно октава, 1200 центов).
    juce::StringArray lines;
    lines.addLines (text);

    juce::StringArray content;
    for (const auto& line : lines)
        if (! line.trimStart().startsWithChar ('!'))
            content.add (line);

    if (content.size() < 2)
    {
        error = "Not a Scala file: no description and note count";
        return false;
    }

    const auto scaleDescription = content[0].trim();
    const int count = content[1].trim().getIntValue();

    if (count < 1 || count > 1024 || content.size() < 2 + count)
    {
        error = "Not a Scala file: wrong number of notes";
        return false;
    }

    std::vector<double> newDegrees { 0.0 };
    double newPeriod = 1200.0;

    for (int i = 0; i < count; ++i)
    {
        double cents = 0.0;
        if (! parseScalaPitch (content[2 + i], cents))
        {
            error = "Cannot read note " + juce::String (i + 1) + ": \"" + content[2 + i].trim() + "\"";
            return false;
        }

        if (i + 1 < count)
            newDegrees.push_back (cents);
        else
            newPeriod = cents;
    }

    for (size_t i = 1; i < newDegrees.size(); ++i)
    {
        if (newDegrees[i] <= newDegrees[i - 1] || newDegrees[i] >= newPeriod)
        {
            error = "The notes of the scale must go up and stay below the period";
            return false;
        }
    }

    if (newPeriod <= 0.0)
    {
        error = "The scale period must be above the first note";
        return false;
    }

    setScale (std::move (newDegrees), newPeriod, fallbackName, scaleDescription);
    return true;
}

bool Tuning::loadTun (const juce::String& text, const juce::String& fallbackName, juce::String& error)
{
    // AnaMark TUN: [Tuning] "note N = центы от 8.1758 Гц" или [Exact Tuning] с BaseFreq и "note N = центы от BaseFreq"
    juce::StringArray lines;
    lines.addLines (text);

    std::array<float, kNumNotes> tuning {}, exact {};
    for (int note = 0; note < kNumNotes; ++note)
        tuning[(size_t) note] = exact[(size_t) note] = (float) note;

    juce::String section, tunName;
    double baseFrequency = 8.1757989156;
    bool anyTuning = false, anyExact = false;

    for (const auto& raw : lines)
    {
        const auto line = cleanLine (raw, ";");
        if (line.isEmpty())
            continue;

        if (line.startsWithChar ('['))
        {
            section = line.removeCharacters ("[]").trim().toLowerCase();
            continue;
        }

        const auto key = line.upToFirstOccurrenceOf ("=", false, false).trim().toLowerCase();
        const auto value = line.fromFirstOccurrenceOf ("=", false, false).trim().unquoted();

        if (section == "info" && key == "name")
            tunName = value;

        if (key.startsWith ("note"))
        {
            const int note = key.fromFirstOccurrenceOf ("note", false, false).trim().getIntValue();
            if (! juce::isPositiveAndBelow (note, kNumNotes))
                continue;

            const auto cents = (float) value.getDoubleValue();

            if (section == "tuning")
            {
                tuning[(size_t) note] = cents / 100.0f;
                anyTuning = true;
            }
            else if (section == "exact tuning")
            {
                exact[(size_t) note] = cents / 100.0f;
                anyExact = true;
            }
        }
        else if (section == "exact tuning" && key == "basefreq")
        {
            baseFrequency = value.getDoubleValue();
        }
    }

    if (! anyTuning && ! anyExact)
    {
        error = "Not a TUN file: no [Tuning] or [Exact Tuning] notes";
        return false;
    }

    if (baseFrequency <= 0.0)
    {
        error = "BaseFreq must be above zero";
        return false;
    }

    // Точная секция важнее: центы в ней считаются от BaseFreq
    if (anyExact)
    {
        const float basePitch = (float) (69.0 + 12.0 * std::log2 (baseFrequency / 440.0));
        for (auto& pitch : exact)
            pitch += basePitch;
    }

    fixedTable = anyExact ? exact : tuning;
    tableMode = true;
    name = tunName.isNotEmpty() ? tunName : fallbackName;
    description = "Per-key table from a .tun file";
    rebuild();
    return true;
}

juce::String Tuning::toScala() const
{
    juce::String text;
    text << "! " << (name.isNotEmpty() ? name : juce::String ("Sonder")) << ".scl\n!\n"
         << (description.isNotEmpty() ? description : name) << "\n"
         << " " << (int) degrees.size() << "\n!\n";

    for (size_t i = 1; i < degrees.size(); ++i)
        text << " " << juce::String (degrees[i], 5) << "\n";

    text << " " << juce::String (period, 5) << "\n";
    return text;
}

bool Tuning::saveScala (const juce::File& file) const
{
    return ! tableMode && file.replaceWithText (toScala());
}

//==============================================================================
std::unique_ptr<juce::XmlElement> Tuning::toXml() const
{
    auto xml = std::make_unique<juce::XmlElement> ("Tuning");
    xml->setAttribute ("name", name);
    xml->setAttribute ("description", description);
    xml->setAttribute ("root", rootNote);
    xml->setAttribute ("reference", referenceHz);

    juce::StringArray values;

    if (tableMode)
    {
        for (const auto pitch : fixedTable)
            values.add (juce::String (pitch, 5));

        xml->setAttribute ("table", values.joinIntoString (" "));
    }
    else
    {
        for (size_t i = 1; i < degrees.size(); ++i)
            values.add (juce::String (degrees[i], 5));

        xml->setAttribute ("degrees", values.joinIntoString (" "));
        xml->setAttribute ("period", period);
    }

    return xml;
}

void Tuning::fromXml (const juce::XmlElement* xml)
{
    reset();

    if (xml == nullptr)
        return;

    rootNote = juce::jlimit (0, kNumNotes - 1, xml->getIntAttribute ("root", 60));
    referenceHz = juce::jlimit (380.0, 500.0, xml->getDoubleAttribute ("reference", 440.0));

    if (xml->hasAttribute ("table"))
    {
        const auto values = juce::StringArray::fromTokens (xml->getStringAttribute ("table"), " ", {});
        if (values.size() == kNumNotes)
        {
            for (int note = 0; note < kNumNotes; ++note)
                fixedTable[(size_t) note] = values[note].getFloatValue();

            tableMode = true;
            name = xml->getStringAttribute ("name");
            description = xml->getStringAttribute ("description");
        }
    }
    else if (xml->hasAttribute ("degrees"))
    {
        std::vector<double> newDegrees { 0.0 };
        for (const auto& value : juce::StringArray::fromTokens (xml->getStringAttribute ("degrees"), " ", {}))
            newDegrees.push_back (value.getDoubleValue());

        const double newPeriod = xml->getDoubleAttribute ("period", 1200.0);
        if (newPeriod > newDegrees.back())
        {
            degrees = std::move (newDegrees);
            period = newPeriod;
            name = xml->getStringAttribute ("name");
            description = xml->getStringAttribute ("description");
        }
    }

    rebuild();
}

} // namespace sonder
