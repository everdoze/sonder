#include "Parameters.h"

namespace sonder
{

juce::String ParamIDs::modSource (int slot) { return "mod" + juce::String (slot + 1) + "Source"; }
juce::String ParamIDs::modDest (int slot)   { return "mod" + juce::String (slot + 1) + "Dest"; }
juce::String ParamIDs::modAmount (int slot) { return "mod" + juce::String (slot + 1) + "Amount"; }

const juce::StringArray& Choices::oscShapes()
{
    static const juce::StringArray list { "Saw", "Pulse", "Triangle", "Sine" };
    return list;
}

const juce::StringArray& Choices::filterModes()
{
    static const juce::StringArray list { "LP 24", "LP 12", "Band", "High" };
    return list;
}

const juce::StringArray& Choices::lfoShapes()
{
    static const juce::StringArray list { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth" };
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
                                          "Mod Wheel", "Aftertouch", "Key", "Random" };
    return list;
}

const juce::StringArray& Choices::modDestinations()
{
    static const juce::StringArray list { "Off", "Pitch", "Osc 1 Pitch", "Osc 2 Pitch", "Pulse Width", "Osc Mix",
                                          "FM", "Fold", "Sub", "Noise", "Cutoff", "Resonance", "Drive", "Amp", "Pan" };
    return list;
}

const juce::StringArray& Choices::voiceModes()
{
    static const juce::StringArray list { "Poly", "Mono", "Legato" };
    return list;
}

const juce::StringArray& Choices::chorusModes()
{
    static const juce::StringArray list { "Off", "I", "II", "I+II" };
    return list;
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

    juce::String formatBipolarPercent (float value, int)
    {
        const auto percent = juce::roundToInt (value * 100.0f);
        return (percent > 0 ? "+" : "") + juce::String (percent) + "%";
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

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ParamIDs;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Осцилляторы
    layout.add (makeChoice (osc1Shape, "Osc 1 Shape", Choices::oscShapes(), 0),
                makeChoice (osc2Shape, "Osc 2 Shape", Choices::oscShapes(), 0),
                makeInt (osc2Semi, "Osc 2 Semitones", -24, 24, 0,
                         [] (int v, int) { return (v > 0 ? "+" : "") + juce::String (v) + " st"; }),
                makeFloat (osc2Fine,   "Osc 2 Fine",  { -50.0f, 50.0f }, 7.0f, formatCents),
                makeFloat (oscMix,     "Osc Mix",     unitRange(), 0.5f, formatPercent),
                makeFloat (pulseWidth, "Pulse Width", { 0.05f, 0.95f }, 0.5f, formatPercent),
                makeFloat (subLevel,   "Sub",         unitRange(), 0.0f, formatPercent),
                makeFloat (noiseLevel, "Noise",       unitRange(), 0.0f, formatPercent),
                makeFloat (fmAmount,   "FM",          unitRange(), 0.0f, formatPercent),
                makeFloat (ringLevel,  "Ring Mod",    unitRange(), 0.0f, formatPercent),
                makeFloat (foldAmount, "Fold",        unitRange(), 0.0f, formatPercent));

    // Фильтр
    layout.add (makeChoice (filterMode, "Filter Mode", Choices::filterModes(), 0),
                makeFloat (cutoff,       "Cutoff",            skewed (20.0f, 20000.0f, 1000.0f), 2000.0f, formatHz),
                makeFloat (resonance,    "Resonance",         unitRange(),    0.25f, formatPercent),
                makeFloat (drive,        "Drive",             unitRange(),    0.3f,  formatPercent),
                makeFloat (filterEnvAmt, "Filter Env Amount", bipolarRange(), 0.35f, formatBipolarPercent),
                makeFloat (keyTrack,     "Key Tracking",      unitRange(),    0.5f,  formatPercent),
                makeFloat (velToCutoff,  "Velocity to Cutoff", unitRange(),   0.2f,  formatPercent));

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
    layout.add (makeChoice (lfo1Shape, "LFO 1 Shape", Choices::lfoShapes(), 0),
                makeFloat (lfo1Rate, "LFO 1 Rate", skewed (0.02f, 40.0f, 2.0f), 3.0f, formatHz),
                makeChoice (lfo1Sync, "LFO 1 Sync", Choices::syncDivisions(), 0),
                makeChoice (lfo2Shape, "LFO 2 Shape", Choices::lfoShapes(), 1),
                makeFloat (lfo2Rate, "LFO 2 Rate", skewed (0.02f, 40.0f, 2.0f), 0.4f, formatHz),
                makeChoice (lfo2Sync, "LFO 2 Sync", Choices::syncDivisions(), 0));

    // Мод-матрица
    for (int slot = 0; slot < kNumModSlots; ++slot)
    {
        const auto prefix = "Mod " + juce::String (slot + 1) + " ";
        layout.add (makeChoice (modSource (slot), prefix + "Source", Choices::modSources(), 0),
                    makeChoice (modDest (slot), prefix + "Destination", Choices::modDestinations(), 0),
                    makeFloat (modAmount (slot), prefix + "Amount", bipolarRange(), 0.0f, formatBipolarPercent));
    }

    // Аналоговое поведение
    layout.add (makeFloat (drift,  "Drift",        unitRange(), 0.3f, formatPercent),
                makeFloat (jitter, "Jitter",       unitRange(), 0.2f, formatPercent),
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
                makeInt (bendRange, "Bend Range", 1, 24, 2, [] (int v, int) { return juce::String (v) + " st"; }),
                makeFloat (vibrato, "Mod Wheel Vibrato", unitRange(), 0.3f, formatPercent));

    // Эффекты и выход
    layout.add (makeChoice (chorusMode, "Chorus Mode", Choices::chorusModes(), 0),
                makeFloat (chorusMix,     "Chorus Mix",     unitRange(), 0.6f, formatPercent),
                makeChoice (delaySync, "Delay Sync", Choices::syncDivisions(), 8),
                makeFloat (delayTime,     "Delay Time",     skewed (0.01f, 2.0f, 0.4f), 0.35f, formatTime),
                makeFloat (delayFeedback, "Delay Feedback", { 0.0f, 0.95f }, 0.35f, formatPercent),
                makeFloat (delayMix,      "Delay Mix",      unitRange(), 0.0f, formatPercent),
                makeFloat (delayTape,     "Delay Tape",     unitRange(), 0.3f, formatPercent),
                makeFloat (reverbSize,    "Reverb Size",    unitRange(), 0.6f, formatPercent),
                makeFloat (reverbMix,     "Reverb Mix",     unitRange(), 0.0f, formatPercent),
                makeFloat (masterGain,    "Master",         { -36.0f, 6.0f }, -9.0f, formatDb));

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

    osc1Shape = get (ParamIDs::osc1Shape);
    osc2Shape = get (ParamIDs::osc2Shape);
    osc2Semi = get (ParamIDs::osc2Semi);
    osc2Fine = get (ParamIDs::osc2Fine);
    oscMix = get (ParamIDs::oscMix);
    pulseWidth = get (ParamIDs::pulseWidth);
    subLevel = get (ParamIDs::subLevel);
    noiseLevel = get (ParamIDs::noiseLevel);
    fmAmount = get (ParamIDs::fmAmount);
    ringLevel = get (ParamIDs::ringLevel);
    foldAmount = get (ParamIDs::foldAmount);

    filterMode = get (ParamIDs::filterMode);
    cutoff = get (ParamIDs::cutoff);
    resonance = get (ParamIDs::resonance);
    drive = get (ParamIDs::drive);
    filterEnvAmt = get (ParamIDs::filterEnvAmt);
    keyTrack = get (ParamIDs::keyTrack);
    velToCutoff = get (ParamIDs::velToCutoff);

    filterAttack = get (ParamIDs::filterAttack);
    filterDecay = get (ParamIDs::filterDecay);
    filterSustain = get (ParamIDs::filterSustain);
    filterRelease = get (ParamIDs::filterRelease);

    ampAttack = get (ParamIDs::ampAttack);
    ampDecay = get (ParamIDs::ampDecay);
    ampSustain = get (ParamIDs::ampSustain);
    ampRelease = get (ParamIDs::ampRelease);
    ampVelocity = get (ParamIDs::ampVelocity);

    lfo1Shape = get (ParamIDs::lfo1Shape);
    lfo1Rate = get (ParamIDs::lfo1Rate);
    lfo1Sync = get (ParamIDs::lfo1Sync);
    lfo2Shape = get (ParamIDs::lfo2Shape);
    lfo2Rate = get (ParamIDs::lfo2Rate);
    lfo2Sync = get (ParamIDs::lfo2Sync);

    for (int slot = 0; slot < kNumModSlots; ++slot)
    {
        modSource[(size_t) slot] = get (ParamIDs::modSource (slot));
        modDest[(size_t) slot] = get (ParamIDs::modDest (slot));
        modAmount[(size_t) slot] = get (ParamIDs::modAmount (slot));
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
    bendRange = get (ParamIDs::bendRange);
    vibrato = get (ParamIDs::vibrato);

    chorusMode = get (ParamIDs::chorusMode);
    chorusMix = get (ParamIDs::chorusMix);
    delaySync = get (ParamIDs::delaySync);
    delayTime = get (ParamIDs::delayTime);
    delayFeedback = get (ParamIDs::delayFeedback);
    delayMix = get (ParamIDs::delayMix);
    delayTape = get (ParamIDs::delayTape);
    reverbSize = get (ParamIDs::reverbSize);
    reverbMix = get (ParamIDs::reverbMix);

    masterGain = get (ParamIDs::masterGain);
}

} // namespace sonder
