#include "Parameters.h"
#include "fx/FxRack.h"

namespace sonder
{

namespace
{
    juce::String numbered (const char* prefix, int index, const char* suffix)
    {
        return prefix + juce::String (index + 1) + suffix;
    }
}

juce::String ParamIDs::oscOn (int osc)      { return numbered ("osc", osc, "On"); }
juce::String ParamIDs::oscShape (int osc)   { return numbered ("osc", osc, "Shape"); }
juce::String ParamIDs::oscWtPos (int osc)   { return numbered ("osc", osc, "WtPos"); }
juce::String ParamIDs::oscSemi (int osc)    { return numbered ("osc", osc, "Semi"); }
juce::String ParamIDs::oscFine (int osc)    { return numbered ("osc", osc, "Fine"); }
juce::String ParamIDs::oscPw (int osc)      { return numbered ("osc", osc, "Pw"); }
juce::String ParamIDs::oscLevel (int osc)   { return numbered ("osc", osc, "Level"); }
juce::String ParamIDs::oscSync (int osc)    { return numbered ("osc", osc, "Sync"); }
juce::String ParamIDs::filterParam (int filter, FilterParam param)
{
    // Первый фильтр сохраняет старые идентификаторы, чтобы пресеты с ним не сломались
    static const char* const first[] { "filter1On", filterMode, cutoff, resonance, drive, vowel, filterEnvAmt, keyTrack, velToCutoff };
    static const char* const suffixes[] { "On", "Mode", "Cutoff", "Resonance", "Drive", "Vowel", "EnvAmt", "KeyTrack", "Vel" };

    if (filter == 0)
        return first[(size_t) param];

    return numbered ("filter", filter, suffixes[(size_t) param]);
}

juce::String ParamIDs::lfoShape (int lfo)   { return numbered ("lfo", lfo, "Shape"); }
juce::String ParamIDs::lfoRate (int lfo)    { return numbered ("lfo", lfo, "Rate"); }
juce::String ParamIDs::lfoSync (int lfo)    { return numbered ("lfo", lfo, "Sync"); }
juce::String ParamIDs::lfoMode (int lfo)    { return numbered ("lfo", lfo, "Mode"); }
juce::String ParamIDs::modSource (int slot) { return numbered ("mod", slot, "Source"); }
juce::String ParamIDs::modDest (int slot)   { return numbered ("mod", slot, "Dest"); }
juce::String ParamIDs::modAmount (int slot) { return numbered ("mod", slot, "Amount"); }
juce::String ParamIDs::modPolarity (int slot) { return numbered ("mod", slot, "Polarity"); }
juce::String ParamIDs::fxParam (int slot, int param) { return "fx" + juce::String (slot + 1) + "p" + juce::String (param + 1); }
juce::String ParamIDs::fxOn (int slot)      { return numbered ("fx", slot, "On"); }

const juce::StringArray& Choices::oscShapes()
{
    static const juce::StringArray list { "Saw", "Pulse", "Triangle", "Sine", "Wavetable" };
    return list;
}

const juce::StringArray& Choices::noiseTypes()
{
    static const juce::StringArray list { "White", "Pink", "Brown", "Crackle", "Hiss", "Digital" };
    return list;
}

const juce::StringArray& Choices::filterModes()
{
    // Новые режимы дописаны в конец, поэтому Vowel остался на своём месте
    static const juce::StringArray list { "LP 24", "LP 12", "BP 12", "HP 24", "Vowel", "HP 12", "BP 24", "Notch" };
    return list;
}

const juce::StringArray& Choices::distTypes()
{
    static const juce::StringArray list { "Off", "Tube", "Hard", "Fold", "Crush" };
    return list;
}

const juce::StringArray& Choices::distPositions()
{
    static const juce::StringArray list { "Post", "Pre" };
    return list;
}

const juce::StringArray& Choices::filterRoutings()
{
    static const juce::StringArray list { "Serial", "Parallel" };
    return list;
}

const juce::StringArray& Choices::glideModes()
{
    static const juce::StringArray list { "Auto", "Always", "Legato" };
    return list;
}

const juce::StringArray& Choices::lfoShapes()
{
    static const juce::StringArray list { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth", "Custom" };
    return list;
}

const juce::StringArray& Choices::lfoModes()
{
    static const juce::StringArray list { "Free", "Retrig", "Env" };
    return list;
}

const juce::StringArray& Choices::syncDivisions()
{
    static const juce::StringArray list { "Free", "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T",
                                          "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/1", "2/1", "4/1" };
    return list;
}

double syncDivisionInBeats (int index)
{
    static constexpr double beats[] { 0.0, 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0,
                                      0.75, 1.0, 4.0 / 3.0, 1.5, 2.0, 4.0, 8.0, 16.0 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

const juce::StringArray& Choices::modSources()
{
    static const juce::StringArray list { "Off", "LFO 1", "LFO 2", "Filter Env", "Amp Env", "Velocity",
                                          "Mod Wheel", "Aftertouch", "Key", "Random",
                                          "LFO 3", "LFO 4", "LFO 5", "LFO 6", "LFO 7", "LFO 8", "Slide" };
    return list;
}

const juce::StringArray& Choices::modDestinations()
{
    static const juce::StringArray list { "Off", "Pitch", "Osc 1 Pitch", "Osc 2 Pitch", "Pulse Width", "Osc 1 Level",
                                          "FM", "Fold", "Sub", "Noise", "F1 Cutoff", "F1 Resonance", "F1 Drive", "Amp", "Pan",
                                          "F1 Vowel", "Dist Drive", "Dist Mix", "Osc 1 WT Pos", "Osc 2 WT Pos",
                                          "LFO 1 Rate", "LFO 2 Rate", "LFO 3 Rate", "LFO 4 Rate",
                                          "LFO 5 Rate", "LFO 6 Rate", "LFO 7 Rate", "LFO 8 Rate",
                                          "Noise Color", "Osc 3 Pitch", "Osc 4 Pitch", "Osc 3 WT Pos", "Osc 4 WT Pos",
                                          "Osc 2 Level", "Osc 3 Level", "Osc 4 Level",
                                          "F2 Cutoff", "F2 Resonance", "F2 Drive", "F2 Vowel",
                                          // дальше модуляция считается в долях хода ручки
                                          "Amp Attack", "Amp Decay", "Amp Sustain", "Amp Release", "Amp Velocity",
                                          "FEnv Attack", "FEnv Decay", "FEnv Sustain", "FEnv Release",
                                          "F1 Env Amt", "F1 Key Track", "F1 Velocity", "F2 Env Amt", "F2 Key Track", "F2 Velocity",
                                          "Dist Tone", "Ring", "Osc 1 Fine", "Osc 2 Fine", "Osc 3 Fine", "Osc 4 Fine",
                                          "Unison Detune", "Unison Width", "Glide", "Glide Curve", "Vibrato",
                                          "Drift", "Jitter", "Spread",
                                          // общие для всего синта
                                          "Master", "Sag", "Warm-up" };

    // Ручки слотов рэка: "FX 3 Knob 5" (интерфейс подписывает их по эффекту, который стоит в слоте)
    static const juce::StringArray full = []
    {
        auto result = list;
        for (int slot = 0; slot < 12; ++slot)
            for (int param = 0; param < 16; ++param)
                result.add ("FX " + juce::String (slot + 1) + " Knob " + juce::String (param + 1));

        // Точки форм LFO: высота (Y) и положение (X) первых 16 точек каждого LFO
        for (int lfo = 0; lfo < kNumLfos; ++lfo)
            for (int point = 0; point < 16; ++point)
                for (const char* axis : { " Y", " X" })
                    result.add ("LFO " + juce::String (lfo + 1) + " Point " + juce::String (point + 1) + axis);
        return result;
    }();

    return full;
}

const juce::StringArray& Choices::voiceModes()
{
    static const juce::StringArray list { "Poly", "Mono", "Legato" };
    return list;
}

const juce::StringArray& Choices::modPolarities()
{
    // Both - в обе стороны от положения ручки (так LFO работали всегда), Up - только вверх, Down - только вниз
    static const juce::StringArray list { "Both", "Up", "Down" };
    return list;
}

const juce::StringArray& Choices::arpModes()
{
    static const juce::StringArray list { "Up", "Down", "Up-Down", "Random", "As Played" };
    return list;
}

const juce::StringArray& Choices::arpRates()
{
    static const juce::StringArray list { "1/4", "1/8.", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    return list;
}

double arpRateInBeats (int index)
{
    static constexpr double beats[] { 1.0, 0.75, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, index)];
}

namespace
{
    using Formatter = std::function<juce::String (float, int)>;

    juce::String formatTime (float seconds, int)
    {
        return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms"
                              : juce::String (seconds, 2) + " s";
    }

    juce::String formatHz (float hz, int)
    {
        if (hz < 10.0f)
            return juce::String (hz, 2) + " Hz";

        return hz < 1000.0f ? juce::String (juce::roundToInt (hz)) + " Hz"
                            : juce::String (hz / 1000.0f, 2) + " kHz";
    }

    juce::String formatPercent (float value, int) { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; }
    juce::String formatCents (float cents, int)   { return juce::String (cents, 1) + " ct"; }
    juce::String formatDb (float db, int)         { return juce::String (db, 1) + " dB"; }

    juce::String formatSignedDb (float db, int)   { return (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB"; }
    juce::String formatRatio (float ratio, int)   { return juce::String (ratio, 1) + ":1"; }

    juce::String formatBipolarPercent (float value, int)
    {
        const auto percent = juce::roundToInt (value * 100.0f);
        return (percent > 0 ? "+" : "") + juce::String (percent) + "%";
    }

    // Положение между соседними видами шума: "Pink" или "Pink>Brown"
    juce::String formatNoiseColor (float value, int)
    {
        const auto& names = Choices::noiseTypes();
        const int last = names.size() - 1;
        const int index = juce::jlimit (0, last, juce::roundToInt (value));

        if (std::abs (value - (float) index) < 0.05f)
            return names[index];

        const int lower = juce::jlimit (0, last - 1, (int) value);
        return names[lower] + ">" + names[lower + 1];
    }

    juce::String formatVowel (float value, int)
    {
        static const char* vowels[] { "A", "E", "I", "O", "U" };
        const float position = value * 4.0f;
        const int index = juce::jlimit (0, 4, juce::roundToInt (position));
        return std::abs (position - (float) index) < 0.1f ? juce::String (vowels[index])
                                                          : juce::String (vowels[(int) position]) + ">" + vowels[juce::jmin (4, (int) position + 1)];
    }

    std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name,
                                                          juce::NormalisableRange<float> range,
                                                          float defaultValue, Formatter formatter)
    {
        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, defaultValue,
                                                            juce::AudioParameterFloatAttributes()
                                                                .withStringFromValueFunction (std::move (formatter)));
    }

    std::unique_ptr<juce::AudioParameterChoice> makeChoice (const juce::String& id, const juce::String& name,
                                                            const juce::StringArray& choices, int defaultIndex)
    {
        return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, defaultIndex);
    }

    std::unique_ptr<juce::AudioParameterInt> makeInt (const juce::String& id, const juce::String& name,
                                                      int minValue, int maxValue, int defaultValue,
                                                      std::function<juce::String (int, int)> formatter)
    {
        return std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, minValue, maxValue, defaultValue,
                                                          juce::AudioParameterIntAttributes()
                                                              .withStringFromValueFunction (std::move (formatter)));
    }

    juce::NormalisableRange<float> skewed (float minValue, float maxValue, float centre)
    {
        juce::NormalisableRange<float> range (minValue, maxValue);
        range.setSkewForCentre (centre);
        return range;
    }

    juce::NormalisableRange<float> timeRange() { return skewed (0.001f, 10.0f, 0.5f); }
    juce::NormalisableRange<float> unitRange() { return { 0.0f, 1.0f }; }
    juce::NormalisableRange<float> bipolarRange() { return { -1.0f, 1.0f }; }
}

namespace
{
    // Параметр ручки слота рэка. Хранит нормированное значение 0..1; имя, текст значения и значение
    // по умолчанию берёт из описания эффекта, который сейчас стоит в слоте.
    class FxSlotParameter final : public juce::AudioParameterFloat
    {
    public:
        FxSlotParameter (const FxRack& rackToUse, int slotIndex, int paramIndex)
            : juce::AudioParameterFloat (juce::ParameterID { ParamIDs::fxParam (slotIndex, paramIndex), 1 },
                                         "FX " + juce::String (slotIndex + 1) + " Param " + juce::String (paramIndex + 1),
                                         juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
              rack (rackToUse), slot (slotIndex), param (paramIndex)
        {
        }

        juce::String getName (int maximumStringLength) const override
        {
            const auto type = rack.getType (slot);
            const auto prefix = "FX " + juce::String (slot + 1) + " ";

            if (const auto* info = paramInfo())
                return (prefix + getFxTypeInfo (type).name + " " + info->label).substring (0, maximumStringLength);

            return (prefix + "Param " + juce::String (param + 1)).substring (0, maximumStringLength);
        }

        juce::String getText (float normalised, int) const override
        {
            if (const auto* info = paramInfo())
                return info->text (info->toReal (normalised));

            return juce::String (normalised, 2);
        }

        float getValueForText (const juce::String& text) const override
        {
            const auto* info = paramInfo();
            if (info == nullptr)
                return juce::jlimit (0.0f, 1.0f, text.getFloatValue());

            if (info->isChoice())
            {
                const int index = info->choices.indexOf (text.trim(), true);
                return info->toNormalised ((float) juce::jmax (0, index));
            }

            // Проценты вводятся как "35", а хранятся как 0.35
            float entered = text.retainCharacters ("-+.0123456789").getFloatValue();
            if (info->format == FxParamInfo::Format::percent || info->format == FxParamInfo::Format::bipolarPercent)
                entered *= 0.01f;

            return info->toNormalised (entered);
        }

        float getDefaultValue() const override
        {
            if (const auto* info = paramInfo())
                return info->toNormalised (info->defaultValue);

            return 0.0f;
        }

        int getNumSteps() const override
        {
            const auto* info = paramInfo();
            return info != nullptr && info->isChoice() ? info->choices.size() : juce::AudioProcessor::getDefaultNumParameterSteps();
        }

    private:
        const FxParamInfo* paramInfo() const
        {
            const auto& info = getFxTypeInfo (rack.getType (slot));
            return param < (int) info.params.size() ? &info.params[(size_t) param] : nullptr;
        }

        const FxRack& rack;
        const int slot, param;
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (const FxRack& rack)
{
    using namespace ParamIDs;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Осцилляторы: четыре одинаковых слота, по умолчанию включены первые два.
    // Расстройка по умолчанию у каждого своя, чтобы включённый осциллятор сразу давал биения.
    static constexpr float defaultFine[kNumOscs] { 0.0f, 7.0f, -7.0f, 12.0f };

    for (int osc = 0; osc < kNumOscs; ++osc)
    {
        const auto prefix = "Osc " + juce::String (osc + 1) + " ";

        layout.add (makeChoice (oscOn (osc), prefix + "On", { "Off", "On" }, osc < 2 ? 1 : 0),
                    makeChoice (oscShape (osc), prefix + "Shape", Choices::oscShapes(), 0),
                    makeFloat (oscWtPos (osc), prefix + "WT Position", unitRange(), 0.0f, formatPercent),
                    makeInt (oscSemi (osc), prefix + "Semitones", -24, 24, 0,
                             [] (int v, int) { return (v > 0 ? "+" : "") + juce::String (v) + " st"; }),
                    makeFloat (oscFine (osc), prefix + "Fine", { -50.0f, 50.0f }, defaultFine[osc], formatCents),
                    makeFloat (oscPw (osc), prefix + "Pulse Width", { 0.05f, 0.95f }, 0.5f, formatPercent),
                    makeFloat (oscLevel (osc), prefix + "Level", unitRange(), 0.5f, formatPercent));

        if (osc > 0)
            layout.add (makeChoice (oscSync (osc), prefix + "Sync", { "Off", "On" }, 0));
    }

    layout.add (makeFloat (subLevel,   "Sub",         unitRange(), 0.0f, formatPercent),
                makeFloat (noiseLevel, "Noise",       unitRange(), 0.0f, formatPercent),
                makeFloat (noiseColor, "Noise Color", { 0.0f, (float) (Choices::noiseTypes().size() - 1) }, 0.0f, formatNoiseColor),
                makeFloat (fmAmount,   "FM",          unitRange(), 0.0f, formatPercent),
                makeFloat (ringLevel,  "Ring Mod",    unitRange(), 0.0f, formatPercent),
                makeFloat (foldAmount, "Fold",        unitRange(), 0.0f, formatPercent));

    // Фильтры. Второй по умолчанию выключен; включённый он сразу полезен как срез низа (HP 12, 120 Гц).
    for (int f = 0; f < kNumFilters; ++f)
    {
        const bool first = f == 0;
        const auto prefix = "Filter " + juce::String (f + 1) + " ";
        const auto id = [f] (FilterParam param) { return filterParam (f, param); };

        layout.add (makeChoice (id (FilterParam::on), prefix + "On", { "Off", "On" }, first ? 1 : 0),
                    makeChoice (id (FilterParam::mode), prefix + "Mode", Choices::filterModes(), first ? 0 : 5),
                    makeFloat (id (FilterParam::cutoff), prefix + "Cutoff", skewed (20.0f, 20000.0f, 1000.0f), first ? 2000.0f : 120.0f, formatHz),
                    makeFloat (id (FilterParam::resonance), prefix + "Resonance", unitRange(), first ? 0.25f : 0.0f, formatPercent),
                    makeFloat (id (FilterParam::drive), prefix + "Drive", unitRange(), 0.3f, formatPercent),
                    makeFloat (id (FilterParam::vowel), prefix + "Vowel", unitRange(), 0.0f, formatVowel),
                    makeFloat (id (FilterParam::envAmount), prefix + "Env Amount", bipolarRange(), first ? 0.35f : 0.0f, formatBipolarPercent),
                    makeFloat (id (FilterParam::keyTrack), prefix + "Key Tracking", unitRange(), first ? 0.5f : 0.0f, formatPercent),
                    makeFloat (id (FilterParam::velocity), prefix + "Velocity to Cutoff", unitRange(), first ? 0.2f : 0.0f, formatPercent));
    }

    layout.add (makeChoice (filterRouting, "Filter Routing", Choices::filterRoutings(), 0));

    // Дисторшн после фильтра
    layout.add (makeChoice (distType, "Distortion Type", Choices::distTypes(), 0),
                makeFloat (distDrive, "Distortion Drive", unitRange(), 0.4f, formatPercent),
                makeFloat (distMix,   "Distortion Mix",   unitRange(), 1.0f, formatPercent),
                makeFloat (distTone,  "Distortion Tone",  unitRange(), 1.0f, formatPercent),
                makeChoice (distPosition, "Distortion Position", Choices::distPositions(), 0));

    // Огибающие
    layout.add (makeFloat (filterAttack,  "Filter Attack",  timeRange(), 0.005f, formatTime),
                makeFloat (filterDecay,   "Filter Decay",   timeRange(), 0.6f,   formatTime),
                makeFloat (filterSustain, "Filter Sustain", unitRange(), 0.2f,   formatPercent),
                makeFloat (filterRelease, "Filter Release", timeRange(), 0.5f,   formatTime),
                makeFloat (ampAttack,     "Amp Attack",     timeRange(), 0.005f, formatTime),
                makeFloat (ampDecay,      "Amp Decay",      timeRange(), 0.4f,   formatTime),
                makeFloat (ampSustain,    "Amp Sustain",    unitRange(), 0.8f,   formatPercent),
                makeFloat (ampRelease,    "Amp Release",    timeRange(), 0.4f,   formatTime),
                makeFloat (ampVelocity,   "Amp Velocity",   unitRange(), 0.6f,   formatPercent));

    // LFO
    for (int lfo = 0; lfo < kNumLfos; ++lfo)
    {
        const auto prefix = "LFO " + juce::String (lfo + 1) + " ";
        const float defaultRate = lfo == 0 ? 3.0f : (lfo == 1 ? 0.4f : 1.0f);

        layout.add (makeChoice (lfoShape (lfo), prefix + "Shape", Choices::lfoShapes(), lfo == 1 ? 1 : 0),
                    makeFloat (lfoRate (lfo), prefix + "Rate", skewed (0.02f, 40.0f, 2.0f), defaultRate, formatHz),
                    makeChoice (lfoSync (lfo), prefix + "Sync", Choices::syncDivisions(), 0),
                    makeChoice (lfoMode (lfo), prefix + "Mode", Choices::lfoModes(), 0));
    }

    // Мод-матрица
    for (int slot = 0; slot < kNumModSlots; ++slot)
    {
        const auto prefix = "Mod " + juce::String (slot + 1) + " ";
        layout.add (makeChoice (modSource (slot), prefix + "Source", Choices::modSources(), 0),
                    makeChoice (modDest (slot), prefix + "Destination", Choices::modDestinations(), 0),
                    makeFloat (modAmount (slot), prefix + "Amount", bipolarRange(), 0.0f, formatBipolarPercent),
                    makeChoice (modPolarity (slot), prefix + "Direction", Choices::modPolarities(), 0));
    }

    // Аналоговое поведение
    layout.add (makeFloat (drift,  "Drift",        unitRange(), 0.2f, formatPercent),
                makeFloat (jitter, "Jitter",       unitRange(), 0.1f, formatPercent),
                makeFloat (spread, "Voice Spread", unitRange(), 0.3f, formatPercent),
                makeFloat (sag,    "Power Sag",    unitRange(), 0.2f, formatPercent),
                makeFloat (warmup, "Warm-up",      unitRange(), 0.0f, formatPercent),
                makeInt (unit, "Unit", 1, 16, 1, [] (int v, int) { return "#" + juce::String (v); }));

    // Голоса
    layout.add (makeChoice (voiceMode, "Voice Mode", Choices::voiceModes(), 0),
                makeInt (unisonVoices, "Unison", 1, 4, 1, [] (int v, int) { return v == 1 ? juce::String ("Off") : "x" + juce::String (v); }),
                makeFloat (unisonDetune, "Unison Detune", unitRange(), 0.25f, formatPercent),
                makeFloat (unisonWidth,  "Unison Width",  unitRange(), 0.6f,  formatPercent),
                makeFloat (glide,        "Glide",         skewed (0.0f, 2.0f, 0.25f), 0.0f, formatTime),
                makeChoice (glideMode, "Glide Mode", Choices::glideModes(), 0),
                makeFloat (glideCurve,   "Glide Curve",   bipolarRange(), 0.7f, formatBipolarPercent),
                makeInt (bendRange, "Bend Range", 1, 24, 2, [] (int v, int) { return juce::String (v) + " st"; }),
                makeFloat (vibrato, "Mod Wheel Vibrato", unitRange(), 0.3f, formatPercent));

    // Выход
    layout.add (makeFloat (masterGain, "Master", { -36.0f, 6.0f }, -9.0f, formatDb));

    // Арпеджиатор и выразительность
    layout.add (makeChoice (arpOn, "Arp", { "Off", "On" }, 0),
                makeChoice (arpMode, "Arp Mode", Choices::arpModes(), 0),
                makeInt (arpOctaves, "Arp Octaves", 1, 4, 1, [] (int v, int) { return juce::String (v) + " oct"; }),
                makeChoice (arpRate, "Arp Rate", Choices::arpRates(), 4),
                makeFloat (arpGate, "Arp Gate", { 0.05f, 1.0f }, 0.6f, formatPercent),
                makeFloat (arpSwing, "Arp Swing", { 0.0f, 0.75f }, 0.0f, formatPercent),
                makeChoice (arpHold, "Arp Hold", { "Off", "On" }, 0),
                makeChoice (mpeOn, "MPE", { "Off", "On" }, 0),
                makeInt (mpeBendRange, "MPE Bend Range", 1, 96, 48, [] (int v, int) { return juce::String (v) + " st"; }));

    // Рэк эффектов: у каждого слота 16 ручек и флаг включения. Что именно делает ручка,
    // зависит от эффекта в слоте, поэтому имя и единицы параметр берёт у рэка на лету.
    for (int slot = 0; slot < kNumFxSlots; ++slot)
    {
        for (int param = 0; param < kNumFxParams; ++param)
            layout.add (std::make_unique<FxSlotParameter> (rack, slot, param));

        // Двухпозиционный выбор, а не AudioParameterBool: тот отдаёт хосту "сырое" значение между 0 и 1,
        // и после восстановления состояния оно может остаться не тем, что было сохранено
        layout.add (makeChoice (fxOn (slot), "FX " + juce::String (slot + 1) + " On", { "Off", "On" }, 1));
    }

    return layout;
}

ParameterRefs::ParameterRefs (juce::AudioProcessorValueTreeState& state)
{
    const auto get = [&state] (const juce::String& id)
    {
        auto* value = state.getRawParameterValue (id);
        jassert (value != nullptr);
        return value;
    };

    for (int osc = 0; osc < kNumOscs; ++osc)
    {
        oscOn[(size_t) osc] = get (ParamIDs::oscOn (osc));
        oscShape[(size_t) osc] = get (ParamIDs::oscShape (osc));
        oscWtPos[(size_t) osc] = get (ParamIDs::oscWtPos (osc));
        oscSemi[(size_t) osc] = get (ParamIDs::oscSemi (osc));
        oscFine[(size_t) osc] = get (ParamIDs::oscFine (osc));
        oscPw[(size_t) osc] = get (ParamIDs::oscPw (osc));
        oscLevel[(size_t) osc] = get (ParamIDs::oscLevel (osc));
        oscSync[(size_t) osc] = osc > 0 ? get (ParamIDs::oscSync (osc)) : nullptr;
    }

    subLevel = get (ParamIDs::subLevel);
    noiseLevel = get (ParamIDs::noiseLevel);
    noiseColor = get (ParamIDs::noiseColor);
    fmAmount = get (ParamIDs::fmAmount);
    ringLevel = get (ParamIDs::ringLevel);
    foldAmount = get (ParamIDs::foldAmount);

    for (int f = 0; f < kNumFilters; ++f)
        for (int param = 0; param < (int) FilterParam::count; ++param)
            filters[(size_t) f][(size_t) param] = get (ParamIDs::filterParam (f, static_cast<FilterParam> (param)));

    filterRouting = get (ParamIDs::filterRouting);

    distType = get (ParamIDs::distType);
    distDrive = get (ParamIDs::distDrive);
    distMix = get (ParamIDs::distMix);
    distTone = get (ParamIDs::distTone);
    distPosition = get (ParamIDs::distPosition);

    filterAttack = get (ParamIDs::filterAttack);
    filterDecay = get (ParamIDs::filterDecay);
    filterSustain = get (ParamIDs::filterSustain);
    filterRelease = get (ParamIDs::filterRelease);

    ampAttack = get (ParamIDs::ampAttack);
    ampDecay = get (ParamIDs::ampDecay);
    ampSustain = get (ParamIDs::ampSustain);
    ampRelease = get (ParamIDs::ampRelease);
    ampVelocity = get (ParamIDs::ampVelocity);

    for (int lfo = 0; lfo < kNumLfos; ++lfo)
    {
        lfoShape[(size_t) lfo] = get (ParamIDs::lfoShape (lfo));
        lfoRate[(size_t) lfo] = get (ParamIDs::lfoRate (lfo));
        lfoSync[(size_t) lfo] = get (ParamIDs::lfoSync (lfo));
        lfoMode[(size_t) lfo] = get (ParamIDs::lfoMode (lfo));
    }

    for (int slot = 0; slot < kNumModSlots; ++slot)
    {
        modSource[(size_t) slot] = get (ParamIDs::modSource (slot));
        modDest[(size_t) slot] = get (ParamIDs::modDest (slot));
        modAmount[(size_t) slot] = get (ParamIDs::modAmount (slot));
        modPolarity[(size_t) slot] = get (ParamIDs::modPolarity (slot));
    }

    drift = get (ParamIDs::drift);
    jitter = get (ParamIDs::jitter);
    spread = get (ParamIDs::spread);
    sag = get (ParamIDs::sag);
    warmup = get (ParamIDs::warmup);
    unit = get (ParamIDs::unit);

    voiceMode = get (ParamIDs::voiceMode);
    unisonVoices = get (ParamIDs::unisonVoices);
    unisonDetune = get (ParamIDs::unisonDetune);
    unisonWidth = get (ParamIDs::unisonWidth);
    glide = get (ParamIDs::glide);
    glideMode = get (ParamIDs::glideMode);
    glideCurve = get (ParamIDs::glideCurve);
    bendRange = get (ParamIDs::bendRange);
    vibrato = get (ParamIDs::vibrato);

    for (int slot = 0; slot < kNumFxSlots; ++slot)
    {
        for (int param = 0; param < kNumFxParams; ++param)
            fxParams[(size_t) slot][(size_t) param] = get (ParamIDs::fxParam (slot, param));

        fxOn[(size_t) slot] = get (ParamIDs::fxOn (slot));
    }

    masterGain = get (ParamIDs::masterGain);

    arpOn = get (ParamIDs::arpOn);
    arpMode = get (ParamIDs::arpMode);
    arpOctaves = get (ParamIDs::arpOctaves);
    arpRate = get (ParamIDs::arpRate);
    arpGate = get (ParamIDs::arpGate);
    arpSwing = get (ParamIDs::arpSwing);
    arpHold = get (ParamIDs::arpHold);
    mpeOn = get (ParamIDs::mpeOn);
    mpeBendRange = get (ParamIDs::mpeBendRange);
}

} // namespace sonder
