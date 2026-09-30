#include "FxTypes.h"
#include "Parameters.h"

namespace sonder
{

juce::NormalisableRange<float> FxParamInfo::range() const
{
    juce::NormalisableRange<float> r (minimum, maximum);
    if (centre > minimum && centre < maximum)
        r.setSkewForCentre (centre);

    return r;
}

// Показатель кривой ручки: те же формулы, что у juce::NormalisableRange, но без создания объекта
// (перевод вызывается из аудиопотока на каждый блок)
static float skewFor (const FxParamInfo& info) noexcept
{
    if (info.centre > info.minimum && info.centre < info.maximum)
        return std::log (0.5f) / std::log ((info.centre - info.minimum) / (info.maximum - info.minimum));

    return 1.0f;
}

float FxParamInfo::toReal (float normalised) const
{
    normalised = juce::jlimit (0.0f, 1.0f, normalised);

    if (isChoice())
        return (float) juce::roundToInt (normalised * (float) juce::jmax (1, choices.size() - 1));

    const float skew = skewFor (*this);
    const float proportion = skew != 1.0f && normalised > 0.0f ? std::exp (std::log (normalised) / skew) : normalised;
    return minimum + (maximum - minimum) * proportion;
}

float FxParamInfo::toNormalised (float real) const
{
    if (isChoice())
        return juce::jlimit (0.0f, 1.0f, real / (float) juce::jmax (1, choices.size() - 1));

    const float proportion = (juce::jlimit (minimum, maximum, real) - minimum) / (maximum - minimum);
    const float skew = skewFor (*this);
    return skew != 1.0f ? std::pow (proportion, skew) : proportion;
}

juce::String FxParamInfo::text (float real) const
{
    switch (format)
    {
        case Format::percent:
            return juce::String (juce::roundToInt (real * 100.0f)) + "%";

        case Format::bipolarPercent:
        {
            const int percent = juce::roundToInt (real * 100.0f);
            return (percent > 0 ? "+" : "") + juce::String (percent) + "%";
        }

        case Format::hertz:
            if (real < 10.0f)    return juce::String (real, 2) + " Hz";
            if (real < 1000.0f)  return juce::String (juce::roundToInt (real)) + " Hz";
            return juce::String (real / 1000.0f, 2) + " kHz";

        case Format::seconds:
            return real < 1.0f ? juce::String (juce::roundToInt (real * 1000.0f)) + " ms"
                               : juce::String (real, 2) + " s";

        case Format::milliseconds:   return juce::String (real, 1) + " ms";
        case Format::decibels:       return juce::String (real, 1) + " dB";
        case Format::signedDecibels: return (real > 0.05f ? "+" : "") + juce::String (real, 1) + " dB";
        case Format::ratio:          return juce::String (real, 1) + ":1";
        case Format::plain:          return juce::String (real, 2);
        case Format::octaves:        return juce::String (real, 2) + " oct";
        case Format::multiplier:     return "x" + juce::String (real, 2);

        case Format::signedHertz:
        {
            const float size = std::abs (real);
            const auto sign = real > 0.005f ? juce::String ("+") : (real < -0.005f ? juce::String ("-") : juce::String());

            if (size < 10.0f)   return sign + juce::String (size, 2) + " Hz";
            if (size < 1000.0f) return sign + juce::String (juce::roundToInt (size)) + " Hz";
            return sign + juce::String (size / 1000.0f, 2) + " kHz";
        }
        case Format::choice:         return choices[juce::jlimit (0, juce::jmax (0, choices.size() - 1), juce::roundToInt (real))];
    }

    return {};
}

namespace
{
    using F = FxParamInfo::Format;

    FxParamInfo knob (const char* label, float minimum, float maximum, float centre, float defaultValue, F format)
    {
        return { label, minimum, maximum, centre, defaultValue, format, {} };
    }

    FxParamInfo percent (const char* label, float defaultValue)
    {
        return knob (label, 0.0f, 1.0f, 0.0f, defaultValue, F::percent);
    }

    FxParamInfo choice (const char* label, const juce::StringArray& choices, int defaultIndex)
    {
        return { label, 0.0f, (float) (choices.size() - 1), 0.0f, (float) defaultIndex, F::choice, choices };
    }

    std::vector<FxTypeInfo> makeTypes()
    {
        std::vector<FxTypeInfo> types ((size_t) FxType::count);

        types[(size_t) FxType::none] = { FxType::none, "none", "Empty", {} };

        types[(size_t) FxType::distortion] = { FxType::distortion, "distortion", "Distortion", {
            choice ("Type", { "Tube", "Hard", "Fold", "Crush" }, 0),
            percent ("Drive", 0.4f),
            percent ("Tone", 1.0f),
            percent ("Mix", 1.0f),
            knob ("Level", -12.0f, 12.0f, 0.0f, 0.0f, F::signedDecibels) } };

        types[(size_t) FxType::phaser] = { FxType::phaser, "phaser", "Phaser", {
            knob ("Rate", 0.02f, 10.0f, 0.5f, 0.4f, F::hertz),
            percent ("Depth", 0.7f),
            knob ("Feedback", 0.0f, 0.9f, 0.0f, 0.5f, F::percent),
            knob ("Centre", 100.0f, 4000.0f, 800.0f, 950.0f, F::hertz),
            choice ("Stages", { "4", "6", "8", "12" }, 1),
            percent ("Spread", 0.5f),
            percent ("Mix", 0.7f) } };

        types[(size_t) FxType::flanger] = { FxType::flanger, "flanger", "Flanger", {
            knob ("Rate", 0.02f, 10.0f, 0.5f, 0.25f, F::hertz),
            percent ("Depth", 0.6f),
            knob ("Feedback", -0.95f, 0.95f, 0.0f, 0.5f, F::bipolarPercent),
            knob ("Delay", 0.2f, 5.0f, 1.2f, 0.6f, F::milliseconds),
            percent ("Spread", 0.5f),
            percent ("Mix", 0.7f) } };

        types[(size_t) FxType::chorus] = { FxType::chorus, "chorus", "Chorus", {
            choice ("Mode", { "I", "II", "I+II", "Free" }, 0),
            knob ("Rate", 0.05f, 10.0f, 1.0f, 0.5f, F::hertz),
            percent ("Depth", 0.5f),
            knob ("Tone", 2000.0f, 16000.0f, 6000.0f, 8000.0f, F::hertz),
            percent ("Width", 1.0f),
            percent ("Mix", 0.6f) } };

        types[(size_t) FxType::delay] = { FxType::delay, "delay", "Delay", {
            choice ("Sync", Choices::syncDivisions(), 8),
            knob ("Time", 0.01f, 2.0f, 0.4f, 0.35f, F::seconds),
            knob ("Feedback", 0.0f, 0.95f, 0.0f, 0.35f, F::percent),
            percent ("Mix", 0.25f),
            percent ("Tape", 0.3f),
            knob ("Low Cut", 20.0f, 1000.0f, 150.0f, 70.0f, F::hertz),
            knob ("High Cut", 1000.0f, 20000.0f, 6000.0f, 12000.0f, F::hertz),
            percent ("Width", 1.0f),
            knob ("Offset", -0.5f, 0.5f, 0.0f, 0.0f, F::bipolarPercent) } };

        types[(size_t) FxType::compressor] = { FxType::compressor, "compressor", "Compressor", {
            knob ("Thresh", -48.0f, 0.0f, 0.0f, -18.0f, F::decibels),
            knob ("Ratio", 1.0f, 20.0f, 4.0f, 4.0f, F::ratio),
            knob ("Attack", 0.001f, 0.1f, 0.01f, 0.01f, F::seconds),
            knob ("Release", 0.01f, 1.0f, 0.15f, 0.15f, F::seconds),
            knob ("Knee", 0.0f, 24.0f, 0.0f, 6.0f, F::decibels),
            knob ("Makeup", 0.0f, 24.0f, 0.0f, 0.0f, F::signedDecibels),
            percent ("Mix", 1.0f),
            knob ("SC HP", 20.0f, 500.0f, 100.0f, 20.0f, F::hertz) } };

        types[(size_t) FxType::reverb] = { FxType::reverb, "reverb", "Reverb", {
            percent ("Size", 0.6f),
            knob ("Decay", 0.2f, 20.0f, 2.5f, 2.5f, F::seconds),
            knob ("Pre-Delay", 0.0f, 200.0f, 40.0f, 10.0f, F::milliseconds),
            percent ("Damping", 0.5f),
            knob ("Low Cut", 20.0f, 1000.0f, 150.0f, 100.0f, F::hertz),
            percent ("Width", 1.0f),
            percent ("Mod", 0.3f),
            percent ("Mix", 0.25f) } };

        types[(size_t) FxType::equalizer] = { FxType::equalizer, "equalizer", "EQ", {
            knob ("Low Cut", 20.0f, 1000.0f, 120.0f, 20.0f, F::hertz),
            knob ("LS Freq", 40.0f, 600.0f, 150.0f, 120.0f, F::hertz),
            knob ("LS Gain", -18.0f, 18.0f, 0.0f, 0.0f, F::signedDecibels),
            knob ("M1 Freq", 100.0f, 8000.0f, 800.0f, 400.0f, F::hertz),
            knob ("M1 Gain", -18.0f, 18.0f, 0.0f, 0.0f, F::signedDecibels),
            knob ("M1 Q", 0.3f, 8.0f, 1.2f, 1.0f, F::plain),
            knob ("M2 Freq", 300.0f, 16000.0f, 2500.0f, 2500.0f, F::hertz),
            knob ("M2 Gain", -18.0f, 18.0f, 0.0f, 0.0f, F::signedDecibels),
            knob ("M2 Q", 0.3f, 8.0f, 1.2f, 1.0f, F::plain),
            knob ("HS Freq", 2000.0f, 16000.0f, 6000.0f, 6000.0f, F::hertz),
            knob ("HS Gain", -18.0f, 18.0f, 0.0f, 0.0f, F::signedDecibels),
            knob ("High Cut", 1000.0f, 20000.0f, 6000.0f, 20000.0f, F::hertz) } };

        types[(size_t) FxType::filter] = { FxType::filter, "filter", "Filter", {
            choice ("Mode", { "LP 24", "LP 12", "HP 24", "HP 12", "BP 24", "BP 12", "Notch" }, 0),
            knob ("Cutoff", 20.0f, 20000.0f, 1000.0f, 1200.0f, F::hertz),
            percent ("Reso", 0.35f),
            percent ("Drive", 0.2f),
            choice ("Sync", Choices::syncDivisions(), 0),
            knob ("Rate", 0.02f, 20.0f, 1.0f, 0.5f, F::hertz),
            knob ("Depth", 0.0f, 4.0f, 0.0f, 0.0f, F::octaves),
            choice ("Shape", { "Sine", "Triangle", "Saw Up", "Saw Down", "Square" }, 0),
            percent ("Spread", 0.0f),
            percent ("Mix", 1.0f) } };

        types[(size_t) FxType::tremolo] = { FxType::tremolo, "tremolo", "Tremolo", {
            choice ("Mode", { "Tremolo", "Autopan" }, 0),
            choice ("Sync", Choices::syncDivisions(), 0),
            knob ("Rate", 0.05f, 20.0f, 2.0f, 4.0f, F::hertz),
            percent ("Depth", 0.5f),
            choice ("Shape", { "Sine", "Triangle", "Square", "Saw Up", "Saw Down" }, 0),
            percent ("Phase", 0.0f),
            percent ("Smooth", 0.3f) } };

        types[(size_t) FxType::widener] = { FxType::widener, "widener", "Stereo", {
            knob ("Width", 0.0f, 2.0f, 0.0f, 1.4f, F::percent),
            percent ("Spread", 0.3f),
            knob ("Time", 2.0f, 30.0f, 0.0f, 12.0f, F::milliseconds),
            knob ("Bass Mono", 20.0f, 500.0f, 120.0f, 120.0f, F::hertz) } };

        types[(size_t) FxType::limiter] = { FxType::limiter, "limiter", "Limiter", {
            knob ("Gain", 0.0f, 24.0f, 0.0f, 0.0f, F::signedDecibels),
            knob ("Ceiling", -24.0f, 0.0f, 0.0f, -0.3f, F::decibels),
            knob ("Release", 0.01f, 1.0f, 0.1f, 0.1f, F::seconds) } };

        types[(size_t) FxType::multiband] = { FxType::multiband, "multiband", "Multiband", {
            percent ("Depth", 0.6f),
            knob ("Time", 0.1f, 4.0f, 1.0f, 1.0f, F::multiplier),
            knob ("Low X", 40.0f, 1000.0f, 150.0f, 120.0f, F::hertz),
            knob ("High X", 1000.0f, 10000.0f, 2500.0f, 2500.0f, F::hertz),
            percent ("Upward", 1.0f),
            percent ("Downward", 1.0f),
            knob ("Low", -12.0f, 12.0f, 0.0f, 0.0f, F::signedDecibels),
            knob ("Mid", -12.0f, 12.0f, 0.0f, 0.0f, F::signedDecibels),
            knob ("High", -12.0f, 12.0f, 0.0f, 0.0f, F::signedDecibels),
            knob ("Output", -12.0f, 12.0f, 0.0f, 4.0f, F::signedDecibels) } };

        types[(size_t) FxType::shifter] = { FxType::shifter, "shifter", "Freq Shift", {
            knob ("Shift", -2000.0f, 2000.0f, 0.0f, 100.0f, F::signedHertz),
            knob ("Fine", -20.0f, 20.0f, 0.0f, 0.0f, F::signedHertz),
            knob ("Feedback", 0.0f, 0.9f, 0.0f, 0.0f, F::percent),
            percent ("Spread", 0.0f),
            percent ("Mix", 1.0f) } };

        return types;
    }
}

const FxTypeInfo& getFxTypeInfo (FxType type)
{
    static const std::vector<FxTypeInfo> types = makeTypes();
    return types[(size_t) juce::jlimit (0, (int) FxType::count - 1, (int) type)];
}

FxType fxTypeFromId (const juce::String& id)
{
    for (int i = 0; i < (int) FxType::count; ++i)
        if (getFxTypeInfo (static_cast<FxType> (i)).id == id)
            return static_cast<FxType> (i);

    return FxType::none;
}

} // namespace sonder
