#include "FilterView.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "Theme.h"
#include "dsp/FormantFilter.h"
#include "synth/SynthParams.h"

#include <complex>

namespace sonder::ui
{

namespace
{
    using Complex = std::complex<float>;

    constexpr float kMinFrequency = 20.0f, kMaxFrequency = 20000.0f;
    constexpr float kMinDb = -36.0f, kMaxDb = 24.0f;

    Complex ladderResponse (float frequency, float cutoff, float resonance, LadderFilter::Mode mode, float sampleRate)
    {
        return LadderFilter::response (frequency, cutoff, resonance, mode, sampleRate);
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

FilterView::FilterView (SonderAudioProcessor& p, Visuals& v)
    : processor (p), visuals (v)
{
    // Спектр живой: 60 кадров, иначе он заметно дёргается
    startTimerHz (60);
}

void FilterView::setSelectedFilter (int filter)
{
    selected = juce::jlimit (0, kNumFilters - 1, filter);
    lastSignature = -1.0;
    timerCallback();
}

FilterView::FilterState FilterView::readFilter (int filter) const
{
    auto& state = processor.parameters;
    const auto get = [&] (FilterParam param) { return state.getRawParameterValue (ParamIDs::filterParam (filter, param))->load(); };

    // Пока звучит нота - живые значения из голоса
    FilterState result;
    const float liveCutoff = processor.displayCutoff[(size_t) filter].load();
    result.on = get (FilterParam::on) > 0.5f;
    result.live = liveCutoff > 0.0f && result.on;
    result.mode = (int) get (FilterParam::mode);
    result.resonance = get (FilterParam::resonance);
    result.cutoff = result.live ? liveCutoff : get (FilterParam::cutoff);
    result.vowel = result.live ? processor.displayVowel[(size_t) filter].load() : get (FilterParam::vowel);
    return result;
}

void FilterView::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> plot)
{
    const auto& pre = visuals.getPreFilterSpectrum();
    const auto& post = visuals.getPostFilterSpectrum();

    float peak = Visuals::kSilenceDb;
    for (float db : pre)
        peak = juce::jmax (peak, db);

    if (peak < -90.0f)
        return;

    // Масштаб общий для обоих спектров: вершина сигнала до фильтра стоит чуть ниже верха экрана.
    // Подстраивается плавно, чтобы картинка не прыгала от ноты к ноте.
    const float targetOffset = -6.0f - peak;
    spectrumOffset += (targetOffset - spectrumOffset) * (targetOffset < spectrumOffset ? 0.3f : 0.025f);

    constexpr float range = 60.0f; // дБ на всю высоту экрана
    const auto pathFor = [&] (const Visuals::Spectrum& spectrum)
    {
        juce::Path path;
        for (int band = 0; band < Visuals::kNumBands; ++band)
        {
            const float x = plot.getX() + plot.getWidth() * ((float) band + 0.5f) / (float) Visuals::kNumBands;
            const float height = juce::jlimit (0.0f, 1.0f, 1.0f + (spectrum[(size_t) band] + spectrumOffset) / range);
            const float y = plot.getBottom() - height * plot.getHeight();

            if (band == 0)
                path.startNewSubPath (plot.getX(), y);

            path.lineTo (x, y);
        }

        path.lineTo (plot.getRight(), path.getCurrentPosition().y);
        return path;
    };

    const auto close = [&plot] (juce::Path path)
    {
        path.lineTo (plot.getRight(), plot.getBottom());
        path.lineTo (plot.getX(), plot.getBottom());
        path.closeSubPath();
        return path;
    };

    const float power = visuals.getPower();
    const auto prePath = pathFor (pre);
    const auto postPath = pathFor (post);

    // До фильтра: тусклый силуэт. После: яркая заливка - то, что осталось.
    g.setColour (Palette::textFaint.withAlpha (0.22f * power));
    g.fillPath (close (prePath));
    g.setColour (Palette::textFaint.withAlpha (0.6f * power));
    g.strokePath (prePath, juce::PathStrokeType (1.0f));

    g.setGradientFill (juce::ColourGradient (visuals.trace().withAlpha (0.42f * power), 0.0f, plot.getY(),
                                             visuals.trace().withAlpha (0.10f * power), 0.0f, plot.getBottom(), false));
    g.fillPath (close (postPath));
}

void FilterView::timerCallback()
{
    const auto& settings = Settings::get();

    if (settings.spectrum && isVisible())
        visuals.updateFilterSpectra();

    // Что сейчас на экране: кривые, спектр, настройки вида. Если ничего не поменялось, кадр не нужен.
    const bool shaders = shader.isAvailable() && Visuals::wantsShaders (*this);
    double signature = (shaders ? 1.0e7 : 0.0) + (settings.spectrum ? 2.0e7 : 0.0) + (settings.crtScreen ? 4.0e7 : 0.0)
                     + (double) Palette::accent.getARGB() * 1.0e-3 + selected * 3.0e5
                     + processor.parameters.getRawParameterValue (ParamIDs::filterRouting)->load() * 7.0e5;

    for (int f = 0; f < kNumFilters; ++f)
    {
        const auto filter = readFilter (f);
        signature += (f + 1) * (filter.cutoff + 1000.0 * filter.resonance + 3000.0 * filter.mode
                                + 5000.0 * filter.vowel + (filter.on ? 9.0e4 : 0.0));
    }

    if (settings.spectrum)
        for (size_t band = 0; band < Visuals::kNumBands; ++band)
            signature += (double) (visuals.getPreFilterSpectrum()[band] * 0.37f + visuals.getPostFilterSpectrum()[band]) * (double) (band + 1);

    if (std::abs (signature - lastSignature) < 1.0e-4)
    {
        // Кадр не менялся, но результат шейдеров мог прийти с опозданием: забираем его
        if (shader.flush())
            repaint();

        return;
    }

    lastSignature = signature;

    if (shaders)
        shader.render (*this, visuals.screenFx(), [this] (juce::Graphics& g) { paintScreen (g, true); });
    else
        shader.invalidate();

    repaint();
}

void FilterView::paint (juce::Graphics& g)
{
    if (! shader.draw (g, getLocalBounds().toFloat()))
        paintScreen (g, false);

    paintLabels (g);
}

void FilterView::paintScreen (juce::Graphics& g, bool forShader)
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

    if (Settings::get().spectrum)
        paintSpectrum (g, plot);

    // Каждый фильтр и то, что получается вместе
    std::array<FilterState, kNumFilters> filters;
    for (int f = 0; f < kNumFilters; ++f)
        filters[(size_t) f] = readFilter (f);

    const float sampleRate = processor.scope.sampleRate.load() * 2.0f;
    const bool parallel = processor.parameters.getRawParameterValue (ParamIDs::filterRouting)->load() > 0.5f;
    const int numOn = (int) std::count_if (filters.begin(), filters.end(), [] (const FilterState& f) { return f.on; });

    const auto responseOf = [&] (const FilterState& filter, float frequency)
    {
        return filter.mode == kVowelFilterMode ? vowelResponse (frequency, filter.cutoff, filter.resonance, filter.vowel, sampleRate)
                                               : ladderResponse (frequency, filter.cutoff, filter.resonance,
                                                                 ladderModeForIndex (filter.mode), sampleRate);
    };

    const int steps = juce::jmax (64, (int) plot.getWidth() / 2);
    juce::Path combined;
    std::array<juce::Path, kNumFilters> single;

    for (int i = 0; i <= steps; ++i)
    {
        const float frequency = kMinFrequency * std::pow (kMaxFrequency / kMinFrequency, (float) i / (float) steps);
        const float x = xFor (frequency);

        Complex total = parallel && numOn > 1 ? Complex (0.0f) : Complex (1.0f);

        for (int f = 0; f < kNumFilters; ++f)
        {
            if (! filters[(size_t) f].on)
                continue;

            const auto response = responseOf (filters[(size_t) f], frequency);
            total = parallel && numOn > 1 ? total + 0.5f * response : total * response;

            const float y = yFor (juce::Decibels::gainToDecibels (std::abs (response), -60.0f));
            if (i == 0)
                single[(size_t) f].startNewSubPath (x, y);
            else
                single[(size_t) f].lineTo (x, y);
        }

        const float y = yFor (juce::Decibels::gainToDecibels (std::abs (total), -60.0f));
        if (i == 0)
            combined.startNewSubPath (x, y);
        else
            combined.lineTo (x, y);
    }

    // Отдельные фильтры - тонкими линиями, когда включены оба
    if (numOn > 1)
    {
        for (int f = 0; f < kNumFilters; ++f)
        {
            g.setColour (Palette::accent.withAlpha (f == selected ? 0.75f : 0.35f));
            g.strokePath (single[(size_t) f], juce::PathStrokeType (1.0f));
        }
    }

    auto fill = combined;
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();
    // Со спектром заливка под кривой почти прозрачная, иначе его не видно
    const float fillAlpha = Settings::get().spectrum ? 0.10f : 0.25f;
    g.setGradientFill (juce::ColourGradient (Palette::accent.withAlpha (fillAlpha), 0.0f, plot.getY(),
                                             Palette::accent.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
    g.fillPath (fill);

    if (! forShader)
    {
        g.setColour (Palette::accent.withAlpha (0.14f));
        g.strokePath (combined, { 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }

    g.setColour (numOn > 0 ? Palette::accentBright : Palette::textFaint);
    g.strokePath (combined, { 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    // Метка среза выбранного фильтра
    const auto& current = filters[(size_t) selected];
    if (current.on && current.mode != kVowelFilterMode)
    {
        const float x = xFor (juce::jlimit (kMinFrequency, kMaxFrequency, current.cutoff));
        g.setColour (Palette::accent.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (x - 0.5f, plot.getY(), 1.0f, plot.getHeight()));
    }

    if (! forShader)
        glass.draw (g, bounds);
}

void FilterView::paintLabels (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto filter = readFilter (selected);

    auto caption = "F" + juce::String (selected + 1) + "   ";

    if (! filter.on)
    {
        caption += "OFF";
    }
    else
    {
        caption += Choices::filterModes()[filter.mode].toUpperCase() + "   ";
        caption += filter.cutoff < 1000.0f ? juce::String (juce::roundToInt (filter.cutoff)) + " Hz"
                                           : juce::String (filter.cutoff / 1000.0f, 2) + " kHz";

        if (filter.mode == kVowelFilterMode)
        {
            static const char* vowels[] { "A", "E", "I", "O", "U" };
            caption += "   " + juce::String (vowels[juce::jlimit (0, 4, juce::roundToInt (filter.vowel * 4.0f))]);
        }
    }

    g.setColour (filter.live ? Palette::accentBright : Palette::textDim);
    g.setFont (makeFont (10.5f, true, 0.08f));
    g.drawText (caption, bounds.reduced (10.0f, 6.0f), juce::Justification::topLeft);

    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

} // namespace sonder::ui
