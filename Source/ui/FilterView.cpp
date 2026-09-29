#include "FilterView.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "dsp/FormantFilter.h"

#include <complex>

namespace sonder::ui
{

namespace
{
    using Complex = std::complex<float>;

    constexpr float kMinFrequency = 20.0f, kMaxFrequency = 20000.0f;
    constexpr float kMinDb = -36.0f, kMaxDb = 24.0f;

    // Линейная модель ladder-фильтра (без насыщения): четыре однополюсных звена с обратной связью
    Complex ladderResponse (float frequency, float cutoff, float resonance, int mode, float sampleRate)
    {
        const float pi = juce::MathConstants<float>::pi;
        const float g = std::tan (pi * juce::jmin (cutoff, 0.45f * sampleRate) / sampleRate);
        const float omega = std::tan (pi * juce::jmin (frequency, 0.49f * sampleRate) / sampleRate) / g;
        const Complex h1 = 1.0f / Complex (1.0f, omega);
        const float k = 4.5f * resonance;
        const Complex h2 = h1 * h1, h4 = h2 * h2;
        const Complex denominator = 1.0f + k * h4;

        switch (mode)
        {
            case 1:  return h2 / denominator * (1.0f + 0.2f * k);
            case 2:  return 2.0f * (h1 - h2) / denominator;
            case 3:  return std::pow (1.0f - h1, 4.0f) / denominator;
            default: return h4 / denominator * (1.0f + 0.3f * k);
        }
    }

    Complex vowelResponse (float frequency, float cutoff, float resonance, float vowel, float sampleRate)
    {
        const float pi = juce::MathConstants<float>::pi;
        const float shift = std::exp2 (juce::jlimit (-1.0f, 1.0f, 0.4f * std::log2 (cutoff / 1000.0f)));
        const float k = 1.0f / (3.0f + resonance * 17.0f);
        const float omegaSignal = std::tan (pi * juce::jmin (frequency, 0.49f * sampleRate) / sampleRate);

        Complex result;
        for (const auto& formant : FormantFilter::formantsFor (vowel, shift))
        {
            const float centre = std::tan (pi * juce::jlimit (40.0f, 0.45f * sampleRate, formant.frequency) / sampleRate);
            const float omega = omegaSignal / centre;
            const Complex bandpass = Complex (0.0f, omega) / Complex (1.0f - omega * omega, k * omega);
            result += formant.gain * 2.0f * k * bandpass;
        }

        const Complex body = 1.0f / Complex (1.0f, frequency / 180.0f);
        return result + 0.5f * body * body;
    }
}

FilterView::FilterView (SonderAudioProcessor& p)
    : processor (p)
{
    startTimerHz (30);
}

void FilterView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Palette::deep);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto plot = bounds.reduced (10.0f, 8.0f).withTrimmedBottom (10.0f);
    const auto xFor = [&plot] (float frequency)
    {
        return plot.getX() + plot.getWidth() * std::log (frequency / kMinFrequency) / std::log (kMaxFrequency / kMinFrequency);
    };
    const auto yFor = [&plot] (float db)
    {
        return plot.getY() + plot.getHeight() * (kMaxDb - juce::jlimit (kMinDb, kMaxDb, db)) / (kMaxDb - kMinDb);
    };

    // Сетка
    g.setFont (makeFont (9.0f, true));
    for (float frequency : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const bool major = frequency == 100.0f || frequency == 1000.0f || frequency == 10000.0f;
        const float x = xFor (frequency);
        g.setColour (Palette::outline.withAlpha (major ? 0.9f : 0.45f));
        g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());

        if (major)
        {
            g.setColour (Palette::textFaint);
            g.drawText (frequency >= 1000.0f ? juce::String ((int) (frequency / 1000.0f)) + "k" : juce::String ((int) frequency),
                        juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 1.0f, 40.0f, 10.0f), juce::Justification::centred);
        }
    }

    for (float db : { 12.0f, 0.0f, -12.0f, -24.0f })
    {
        g.setColour (Palette::outline.withAlpha (db == 0.0f ? 0.9f : 0.45f));
        g.drawHorizontalLine (juce::roundToInt (yFor (db)), plot.getX(), plot.getRight());
    }

    // Параметры: пока звучит нота - живые значения из голоса
    auto& state = processor.parameters;
    const int mode = (int) state.getRawParameterValue (ParamIDs::filterMode)->load();
    const float resonance = state.getRawParameterValue (ParamIDs::resonance)->load();
    const float liveCutoff = processor.displayCutoff.load();
    const bool live = liveCutoff > 0.0f;
    const float cutoff = live ? liveCutoff : state.getRawParameterValue (ParamIDs::cutoff)->load();
    const float vowel = live ? processor.displayVowel.load() : state.getRawParameterValue (ParamIDs::vowel)->load();
    const float sampleRate = processor.scope.sampleRate.load() * 2.0f;

    juce::Path curve;
    const int steps = juce::jmax (64, (int) plot.getWidth() / 2);
    for (int i = 0; i <= steps; ++i)
    {
        const float frequency = kMinFrequency * std::pow (kMaxFrequency / kMinFrequency, (float) i / (float) steps);
        const Complex response = mode == 4 ? vowelResponse (frequency, cutoff, resonance, vowel, sampleRate)
                                           : ladderResponse (frequency, cutoff, resonance, mode, sampleRate);
        const float db = juce::Decibels::gainToDecibels (std::abs (response), -60.0f);
        const juce::Point<float> p (xFor (frequency), yFor (db));

        if (i == 0)
            curve.startNewSubPath (p);
        else
            curve.lineTo (p);
    }

    auto fill = curve;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (0.25f), 0.0f, plot.getY(),
                                             Palette::accent.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
    g.fillPath (fill);

    g.setColour (Palette::accent.withAlpha (0.14f));
    g.strokePath (curve, { 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    g.setColour (Palette::accentBright);
    g.strokePath (curve, { 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    // Метка среза
    if (mode != 4)
    {
        const float x = xFor (juce::jlimit (kMinFrequency, kMaxFrequency, cutoff));
        g.setColour (Palette::accent.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (x - 0.5f, plot.getY(), 1.0f, plot.getHeight()));
    }

    // Подпись
    auto caption = Choices::filterModes()[mode].toUpperCase() + "   ";
    caption += cutoff < 1000.0f ? juce::String (juce::roundToInt (cutoff)) + " Hz" : juce::String (cutoff / 1000.0f, 2) + " kHz";
    if (mode == 4)
    {
        static const char* vowels[] { "A", "E", "I", "O", "U" };
        caption += "   " + juce::String (vowels[juce::jlimit (0, 4, juce::roundToInt (vowel * 4.0f))]);
    }

    g.setColour (live ? Palette::accentBright : Palette::textDim);
    g.setFont (makeFont (10.5f, true, 0.08f));
    g.drawText (caption, bounds.reduced (10.0f, 6.0f), juce::Justification::topLeft);

    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

} // namespace sonder::ui
