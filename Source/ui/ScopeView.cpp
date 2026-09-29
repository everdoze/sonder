#include "ScopeView.h"
#include "SonderLookAndFeel.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace sonder::ui
{

ScopeView::ScopeView (const ScopeBuffer& buffer)
    : scope (buffer)
{
    samples.resize ((size_t) ScopeBuffer::kSize);
    startTimerHz (60);
}

void ScopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (Palette::deep);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto plot = bounds.reduced (10.0f, 12.0f);

    // Сетка как на экране прибора
    g.setColour (Palette::outline.withAlpha (0.55f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine (juce::roundToInt (plot.getX() + plot.getWidth() * (float) i / 8.0f), plot.getY(), plot.getBottom());
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine (juce::roundToInt (plot.getY() + plot.getHeight() * (float) i / 4.0f), plot.getX(), plot.getRight());

    g.setColour (Palette::outline);
    g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());

    // Окно: целое число периодов последней ноты
    const float sampleRate = scope.sampleRate.load();
    const float frequency = scope.noteFrequency.load();
    const float period = frequency > 0.0f ? sampleRate / frequency : 512.0f;
    const float cycles = juce::jlimit (2.0f, 12.0f, std::ceil (400.0f / period));
    const int window = juce::jlimit (64, 4096, juce::roundToInt (period * cycles));
    const int search = juce::jlimit (1, 2048, juce::roundToInt (period) + 1);
    const int total = window + search;

    scope.copyLatest (samples.data(), total);

    // Последний переход через ноль вверх в зоне поиска
    int trigger = 0;
    for (int i = search - 1; i > 0; --i)
    {
        if (samples[(size_t) i - 1] < 0.0f && samples[(size_t) i] >= 0.0f)
        {
            trigger = i;
            break;
        }
    }

    float peak = 0.0f;
    for (int i = 0; i < window; ++i)
        peak = juce::jmax (peak, std::abs (samples[(size_t) (trigger + i)]));

    // Плавная автоподстройка масштаба
    const float targetGain = peak > 1.0e-4f ? juce::jlimit (0.5f, 30.0f, 0.85f / peak) : displayGain;
    displayGain += (targetGain - displayGain) * 0.12f;

    juce::Path wave;
    for (int i = 0; i < window; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) (window - 1);
        const float value = juce::jlimit (-1.1f, 1.1f, samples[(size_t) (trigger + i)] * displayGain);
        const float y = plot.getCentreY() - value * plot.getHeight() * 0.5f;

        if (i == 0)
            wave.startNewSubPath (x, y);
        else
            wave.lineTo (x, y);
    }

    auto fill = wave;
    fill.lineTo (plot.getRight(), plot.getCentreY());
    fill.lineTo (plot.getX(), plot.getCentreY());
    fill.closeSubPath();
    g.setColour (Palette::accent.withAlpha (0.07f));
    g.fillPath (fill);

    g.setColour (Palette::accent.withAlpha (0.1f));
    g.strokePath (wave, { 8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    g.setColour (Palette::accent.withAlpha (0.28f));
    g.strokePath (wave, { 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    g.setColour (Palette::accentBright);
    g.strokePath (wave, { 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    // Нота и частота
    if (frequency > 0.0f && peak > 1.0e-4f)
    {
        const int note = juce::roundToInt (69.0f + 12.0f * std::log2 (frequency / 440.0f));
        const auto text = juce::MidiMessage::getMidiNoteName (note, true, true, 4) + "  " + juce::String (frequency, 1) + " Hz";

        g.setColour (Palette::textDim);
        g.setFont (makeFont (11.0f, true, 0.08f));
        g.drawText (text, bounds.reduced (12.0f, 8.0f), juce::Justification::topRight);
    }

    // Лёгкая виньетка по краям экрана
    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

} // namespace sonder::ui
