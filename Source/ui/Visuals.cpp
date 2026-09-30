#include "Visuals.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "Theme.h"

namespace sonder::ui
{

Visuals::Visuals (SonderAudioProcessor& p)
    : processor (p)
{
    work.resize ((size_t) kFftSize * 2);
    preSpectrum.fill (kSilenceDb);
    postSpectrum.fill (kSilenceDb);
    startTimerHz (30);
}

float Visuals::bandFrequency (float band) noexcept
{
    return kMinFrequency * std::pow (kMaxFrequency / kMinFrequency, band / (float) kNumBands);
}

juce::Colour Visuals::trace() const
{
    return shiftColour (Palette::accent, timbre);
}

juce::Colour Visuals::traceBright() const
{
    return shiftColour (Palette::accentBright, timbre);
}

ScreenFx Visuals::screenFx (float persistence) const
{
    const auto& settings = Settings::get();
    ScreenFx fx;

    fx.bloom = 1.3f * (0.6f + 0.7f * level) * power;
    fx.curvature = persistence > 0.0f ? 0.08f : 0.06f; // у мелких экранов подписи ближе к краю
    fx.aberration = 2.4f;
    fx.time = (float) std::fmod (now(), 100.0);

    // Строки, затемнение к краям, послесвечение и зерно относятся к настройке "CRT screens"
    if (settings.crtScreen)
    {
        fx.scanlines = 0.28f;
        fx.vignette = 0.45f;
        fx.persistence = persistence;
        fx.noise = 0.03f + 0.12f * cold;
    }

    return fx;
}

ScreenFx Visuals::staticScreenFx (bool interactive)
{
    ScreenFx fx;
    fx.bloom = 1.1f;
    fx.curvature = interactive ? 0.025f : 0.06f;
    fx.aberration = 1.0f; // подписи на маленьких экранах близко к краю, сильное расслоение их размывает

    if (Settings::get().crtScreen)
    {
        fx.scanlines = 0.28f;
        fx.vignette = 0.45f;
        fx.noise = 0.03f;
        fx.time = (float) std::fmod (now(), 100.0);
    }

    return fx;
}

bool Visuals::wantsShaders (const juce::Component& screen)
{
    if (! Settings::get().shaderFx)
        return false;

    for (const auto* c = &screen; c != nullptr; c = c->getParentComponent())
        if (! c->isVisible())
            return false;

    return true;
}

void Visuals::transform (std::vector<float>& samples)
{
    window.multiplyWithWindowingTable (samples.data(), (size_t) kFftSize);
    std::fill (samples.begin() + kFftSize, samples.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (samples.data());

    // Амплитуда синуса с учётом окна Ханна
    juce::FloatVectorOperations::multiply (samples.data(), 4.0f / (float) kFftSize, kFftSize / 2);
}

void Visuals::updateFilterSpectra()
{
    const float sampleRate = processor.scope.sampleRate.load();
    const double time = now();
    const float seconds = (float) juce::jlimit (0.001, 0.1, time - lastSpectrumTime);
    lastSpectrumTime = time;

    processor.preFilterTap.copyLatest (work.data(), kFftSize);
    transform (work);
    updateSpectrum (preSpectrum, sampleRate, seconds);

    processor.postFilterTap.copyLatest (work.data(), kFftSize);
    transform (work);
    updateSpectrum (postSpectrum, sampleRate, seconds);
}

void Visuals::updateSpectrum (Spectrum& spectrum, float sampleRate, float seconds)
{
    // Сглаживание по времени, а не по кадрам: вид не зависит от частоты кадров.
    // Вверх быстро, вниз плавно и равномерно, как у анализатора на приборе.
    const float attack = 1.0f - std::exp (-seconds / 0.015f);
    const float fall = 55.0f * seconds;

    const float binHz = sampleRate / (float) kFftSize;
    const int lastBin = kFftSize / 2 - 1;

    for (int band = 0; band < kNumBands; ++band)
    {
        const float low = bandFrequency ((float) band) / binHz;
        const float high = bandFrequency ((float) band + 1.0f) / binHz;
        float magnitude = 0.0f;

        if (high - low < 1.0f)
        {
            // Полоса уже одного отсчёта БПФ: интерполируем между соседними
            const float centre = juce::jlimit (1.0f, (float) lastBin, 0.5f * (low + high));
            const int index = juce::jmin (lastBin - 1, (int) centre);
            const float fraction = centre - (float) index;
            magnitude = work[(size_t) index] + (work[(size_t) index + 1] - work[(size_t) index]) * fraction;
        }
        else
        {
            for (int bin = juce::jlimit (1, lastBin, (int) low); bin <= juce::jmin (lastBin, (int) high); ++bin)
                magnitude = juce::jmax (magnitude, work[(size_t) bin]);
        }

        // Наклон +3 дБ на октаву: без него верх спектра почти не виден
        const float tilt = 3.0f * std::log2 (bandFrequency ((float) band + 0.5f) / 1000.0f);
        const float db = juce::Decibels::gainToDecibels (magnitude, kSilenceDb) + tilt;

        auto& shown = spectrum[(size_t) band];
        shown = db > shown ? shown + (db - shown) * attack : juce::jmax (db, shown - fall);
    }
}

void Visuals::timerCallback()
{
    const auto& settings = Settings::get();
    const float sampleRate = processor.scope.sampleRate.load();

    // Выход: громкость и яркость тембра (спектральный центроид)
    processor.scope.copyLatest (work.data(), kFftSize);

    double sumOfSquares = 0.0;
    for (int i = 0; i < kFftSize; ++i)
        sumOfSquares += (double) work[(size_t) i] * work[(size_t) i];

    const float rmsDb = juce::Decibels::gainToDecibels ((float) std::sqrt (sumOfSquares / kFftSize), -100.0f);
    const float targetLevel = juce::jlimit (0.0f, 1.0f, (rmsDb + 48.0f) / 40.0f);
    level += (targetLevel - level) * (targetLevel > level ? 0.5f : 0.15f);

    float targetTimbre = 0.0f;

    if (settings.timbreColour && targetLevel > 0.02f)
    {
        transform (work);

        const float binHz = sampleRate / (float) kFftSize;
        const int lastBin = juce::jmin (kFftSize / 2 - 1, (int) (16000.0f / binHz));
        float weighted = 0.0f, total = 0.0f;

        for (int bin = 2; bin <= lastBin; ++bin)
        {
            weighted += work[(size_t) bin] * (float) bin;
            total += work[(size_t) bin];
        }

        if (total > 1.0e-6f)
        {
            // 200 Гц - глухой бас, 5 кГц - яркий лид
            const float centroid = juce::jmax (20.0f, weighted / total * binHz);
            const float brightness = std::log (centroid / 200.0f) / std::log (5000.0f / 200.0f);
            targetTimbre = juce::jlimit (-1.0f, 1.0f, brightness * 2.0f - 1.0f) * juce::jmin (1.0f, targetLevel * 4.0f);
        }
    }

    timbre += (targetTimbre - timbre) * 0.2f;

    // Накал экранов: на холодном синте они тусклее и подрагивают, при просадке питания свечение проседает
    float targetPower = 1.0f;
    cold = settings.analogGlow ? processor.displayCold.load() : 0.0f;

    if (settings.analogGlow)
    {
        const double t = now();
        const float flicker = 0.5f + 0.5f * (float) (std::sin (t * 41.0) * std::sin (t * 13.7));
        targetPower = juce::jlimit (0.35f, 1.0f, 1.0f - cold * (0.45f + 0.2f * flicker) - 1.2f * processor.displaySag.load());
    }

    power += (targetPower - power) * 0.5f;
}

void ScreenGlass::draw (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerSize)
{
    if (! Settings::get().crtScreen || bounds.isEmpty())
        return;

    const float scale = juce::jlimit (0.5f, 3.0f, g.getInternalContext().getPhysicalPixelScaleFactor());

    if (cache.isNull() || bounds != cachedBounds || scale != cachedScale)
    {
        cachedBounds = bounds;
        cachedScale = scale;
        cache = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (bounds.getWidth() * scale)),
                             juce::jmax (1, juce::roundToInt (bounds.getHeight() * scale)), true, juce::NativeImageType());

        juce::Graphics cacheGraphics (cache);
        cacheGraphics.addTransform (juce::AffineTransform::scale (scale));
        render (cacheGraphics, bounds.withZeroOrigin(), cornerSize);
    }

    g.setOpacity (1.0f);
    g.drawImage (cache, bounds);
}

void ScreenGlass::render (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerSize)
{
    juce::Path clip;
    clip.addRoundedRectangle (bounds, cornerSize);
    g.reduceClipRegion (clip);

    // Строки развёртки
    g.setColour (juce::Colours::black.withAlpha (0.13f));
    for (float y = bounds.getY() + 1.0f; y < bounds.getBottom(); y += 3.0f)
        g.fillRect (bounds.getX(), y, bounds.getWidth(), 1.0f);

    // Затемнение к краям, как у выпуклого стекла кинескопа
    const auto edge = juce::Colours::black.withAlpha (0.34f);
    const auto clear = juce::Colours::transparentBlack;

    juce::ColourGradient vertical (edge, 0.0f, bounds.getY(), edge, 0.0f, bounds.getBottom(), false);
    vertical.addColour (0.3, clear);
    vertical.addColour (0.7, clear);
    g.setGradientFill (vertical);
    g.fillRect (bounds);

    juce::ColourGradient horizontal (edge, bounds.getX(), 0.0f, edge, bounds.getRight(), 0.0f, false);
    horizontal.addColour (0.12, clear);
    horizontal.addColour (0.88, clear);
    g.setGradientFill (horizontal);
    g.fillRect (bounds);
}

} // namespace sonder::ui
