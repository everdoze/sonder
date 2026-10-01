#include "ScopeView.h"
#include "ScopeTrigger.h"
#include "SonderLookAndFeel.h"
#include "Theme.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace sonder::ui
{

namespace
{
    const char* const modeNames[] { "WAVE", "ROLL", "XY" };
    constexpr float rollWindows[] { 0.05f, 0.2f, 0.5f, 1.0f, 2.0f };
    constexpr int kNumRollWindows = (int) std::size (rollWindows);
    constexpr int kVectorSamples = 1024;

    // Гасит картинку послесвечения. В ней цвет уже умножен на прозрачность, поэтому достаточно
    // умножить все байты; простой цикл по строкам компилятор векторизует.
    void fadeImage (juce::Image& image, int multiplier)
    {
        juce::Image::BitmapData data (image, juce::Image::BitmapData::readWrite);
        const int count = data.width * data.pixelStride;

        for (int y = 0; y < data.height; ++y)
        {
            auto* line = data.getLinePointer (y);
            for (int i = 0; i < count; ++i)
                line[i] = (juce::uint8) ((line[i] * multiplier) >> 8);
        }
    }

    juce::String formatWindow (float seconds)
    {
        return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms" : juce::String (seconds, 0) + " s";
    }
}

ScopeView::ScopeView (const ScopeBuffer& buffer, const Visuals& v)
    : scope (buffer), visuals (v)
{
    samples.resize ((size_t) ScopeBuffer::kSize);
    samplesRight.resize ((size_t) ScopeBuffer::kSize);
    setTooltip ("WAVE: waveform locked to the note. ROLL: scrolling waveform, mouse wheel changes the time window. "
                "XY: stereo vectorscope");
    startTimerHz (60);
}

ScopeView::Mode ScopeView::mode() const
{
    return static_cast<Mode> (juce::jlimit (0, 2, Settings::get().scopeMode));
}

void ScopeView::setMode (Mode newMode)
{
    auto& settings = Settings::get();
    if (settings.scopeMode == (int) newMode)
        return;

    settings.scopeMode = (int) newMode;
    settings.save();
    displayGain = 1.0f;
    repaint();
}

juce::Rectangle<float> ScopeView::tabBounds (int index) const
{
    return { 9.0f + (float) index * 40.0f, 4.0f, 38.0f, 16.0f };
}

void ScopeView::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 3; ++i)
    {
        if (tabBounds (i).contains (e.position))
        {
            setMode (static_cast<Mode> (i));
            return;
        }
    }

    // В режиме ROLL клик по экрану перебирает окна времени
    if (mode() == Mode::roll)
    {
        auto& settings = Settings::get();
        settings.scopeWindow = (juce::jlimit (0, kNumRollWindows - 1, settings.scopeWindow) + 1) % kNumRollWindows;
        settings.save();
        displayGain = 1.0f;
    }
}

void ScopeView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (mode() != Mode::roll || wheel.deltaY == 0.0f)
    {
        juce::Component::mouseWheelMove (e, wheel);
        return;
    }

    auto& settings = Settings::get();
    const int next = juce::jlimit (0, kNumRollWindows - 1, settings.scopeWindow + (wheel.deltaY > 0.0f ? -1 : 1));

    if (next != settings.scopeWindow)
    {
        settings.scopeWindow = next;
        settings.save();
    }
}

void ScopeView::followGain (float peak, float target, float speed)
{
    // Автоподстройка масштаба: уменьшаем сразу (иначе громкий звук вылезает за экран),
    // увеличиваем плавно, чтобы картинка не дёргалась на затухании
    signalPresent = peak > 1.0e-4f;
    const float targetGain = signalPresent ? juce::jlimit (0.5f, 30.0f, target / peak) : displayGain;

    if (targetGain < displayGain)
        displayGain = targetGain;
    else
        displayGain += (targetGain - displayGain) * speed;
}

//==============================================================================
bool ScopeView::buildWave (juce::Rectangle<float> plot, juce::Path& trace, juce::Path& fill)
{
    // Окно: целое число периодов последней ноты
    const float sampleRate = scope.sampleRate.load();
    const float frequency = scope.noteFrequency.load();
    const float nominal = frequency > 0.0f ? sampleRate / frequency : 512.0f;

    // Весь буфер: настоящий период может быть длиннее номинального (дрейф, бенд, глайд, строй)
    // и даже кратным ему (суб на октаву ниже); начало окна ищется у свежего края
    const int total = ScopeBuffer::kSize - 2;
    scope.copyLatest (samples.data(), total);

    float peak = 0.0f;
    for (int i = 0; i < total; ++i)
        peak = juce::jmax (peak, std::abs (samples[(size_t) i]));

    // Настоящий период - автокорреляцией по свежей части буфера; окно покрывает целое число таких периодов
    // Период меняется медленно: считаем его раз в несколько кадров и сразу при смене ноты
    if (nominal != periodNominal || --periodCountdown <= 0)
    {
        const int recent = juce::jmin (total, juce::roundToInt (nominal * 12.0f) + 16);
        const float measured = ScopeTrigger::measurePeriod (samples.data() + total - recent, recent, nominal);
        cachedPeriod = measured > 0.0f ? measured : nominal;
        periodNominal = nominal;
        periodCountdown = 6;
    }

    const float period = cachedPeriod;
    // Не меньше двух периодов ноты: с сабом (период вдвое длиннее) это один настоящий период
    const float cycles = juce::jlimit (period > nominal * 1.5f ? 1.0f : 2.0f, 12.0f, std::ceil (400.0f / period));
    const int window = juce::jlimit (64, 4096, juce::roundToInt (cycles * period));
    const int search = total - window - 2;

    // Начало окна. У сложной формы несколько переходов через ноль за период, и выбор между ними от кадра
    // к кадру заставлял картинку прыгать. Поэтому: пока звучит та же нота - по сходству с прошлым кадром;
    // первый кадр ноты - по фазе основной гармоники; если основной тон слишком слаб - по переходу через ноль.
    const bool sameNote = havePreviousShape && std::abs (period - previousNominal) < period * 0.02f && peak > 1.0e-4f;

    ScopeTrigger::Result start;
    if (sameNote)
        start = ScopeTrigger::byContinuity (samples.data(), total, search, period, window, previousShape.data(), kShapePoints);
    if (! start.found)
        start = ScopeTrigger::byFundamental (samples.data(), total, search, period, window);
    if (! start.found)
        start = ScopeTrigger::byZeroCrossing (samples.data(), search, period, peak);

    const float trigger = start.found ? juce::jlimit (0.0f, (float) (total - window - 2), start.trigger) : 0.0f;

    ScopeTrigger::sampleShape (samples.data(), trigger, period, previousShape.data(), kShapePoints);
    previousNominal = period;
    havePreviousShape = peak > 1.0e-4f;

    float windowPeak = 0.0f;
    const int first = (int) trigger;
    for (int i = 0; i <= window + 1; ++i)
        windowPeak = juce::jmax (windowPeak, std::abs (samples[(size_t) (first + i)]));

    followGain (windowPeak, 0.85f, 0.08f);

    const float fraction = trigger - (float) first;
    const int sourceStep = juce::jmax (1, window / 48);

    for (int i = 0; i <= window + 1; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * ((float) i - fraction) / (float) window;
        const float value = juce::jlimit (-1.1f, 1.1f, samples[(size_t) (first + i)] * displayGain);
        const float y = plot.getCentreY() - value * plot.getHeight() * 0.5f;

        if (i == 0)
            trace.startNewSubPath (x, y);
        else
            trace.lineTo (x, y);

        if (i % sourceStep == 0 && plot.contains (x, y))
            sparkSources.emplace_back (x, y);
    }

    fill = trace;
    fill.lineTo (plot.getRight() + plot.getWidth() / (float) window, plot.getCentreY());
    fill.lineTo (plot.getX() - plot.getWidth() / (float) window, plot.getCentreY());
    fill.closeSubPath();
    return true;
}

bool ScopeView::buildRoll (juce::Rectangle<float> plot, juce::Path& trace, juce::Path& fill)
{
    const float sampleRate = scope.sampleRate.load();
    const float seconds = rollWindows[juce::jlimit (0, kNumRollWindows - 1, Settings::get().scopeWindow)];
    const int wanted = juce::roundToInt (seconds * sampleRate);

    // Короткое окно рисуется по сэмплам, длинное - по огибающей "минимум-максимум" (пары на kPeakHop сэмплов)
    const bool raw = wanted <= ScopeBuffer::kSize - 64;
    const int unit = raw ? 1 : ScopeBuffer::kPeakHop;
    const int capacity = raw ? ScopeBuffer::kSize : ScopeBuffer::kNumPeaks;
    const int elements = juce::jmin (capacity - 64, wanted / unit);

    // Столбец экрана - целое число элементов, и столбцы привязаны к абсолютному времени: волна сдвигается
    // на целые столбцы, а уже нарисованные столбцы не меняются. Иначе край буфера каждый кадр сдвигался
    // на случайное число сэмплов, минимум и максимум в столбцах "гуляли", и густой бас мерцал муаром.
    const int columns = juce::jmax (2, (int) plot.getWidth());
    const int bucket = juce::jmax (1, juce::roundToInt ((float) elements / (float) columns));
    const int buckets = juce::jmax (2, elements / bucket);
    const int count = juce::jmin (capacity - 1, (buckets + 1) * bucket);

    const auto endIndex = raw ? scope.copyLatestAligned (samples.data(), count)
                              : scope.copyLatestPeaksAligned (samples.data(), samplesRight.data(), count);

    const float* minimums = samples.data();
    const float* maximums = raw ? samples.data() : samplesRight.data();

    std::vector<float> lows ((size_t) buckets), highs ((size_t) buckets);
    if (! ScopeTrigger::rollColumns (minimums, maximums, count, endIndex, bucket, buckets, lows.data(), highs.data()))
        return false;

    float peak = 0.0f;
    for (int b = 0; b < buckets; ++b)
        peak = juce::jmax (peak, std::abs (lows[(size_t) b]), std::abs (highs[(size_t) b]));

    // Период ноты укладывается меньше чем в восемь столбцов: верхняя и нижняя линии переплетаются в сетку.
    // Тогда рисуем огибающую за два периода - сплошную форму, как у высоких нот
    const float frequency = scope.noteFrequency.load();
    if (frequency > 0.0f)
    {
        const float periodColumns = sampleRate / frequency / (float) (unit * bucket);
        if (periodColumns < 8.0f)
        {
            const int half = juce::jmax (1, (int) std::ceil (periodColumns));
            std::vector<float> spreadLows ((size_t) buckets), spreadHighs ((size_t) buckets);

            for (int b = 0; b < buckets; ++b)
            {
                float low = lows[(size_t) b], high = highs[(size_t) b];
                for (int k = juce::jmax (0, b - half); k <= juce::jmin (buckets - 1, b + half); ++k)
                {
                    low = juce::jmin (low, lows[(size_t) k]);
                    high = juce::jmax (high, highs[(size_t) k]);
                }

                spreadLows[(size_t) b] = low;
                spreadHighs[(size_t) b] = high;
            }

            lows = std::move (spreadLows);
            highs = std::move (spreadHighs);
        }
    }

    followGain (peak, 0.9f, 0.04f);

    const float halfHeight = plot.getHeight() * 0.5f;
    const auto yFor = [&] (float value)
    {
        return plot.getCentreY() - juce::jlimit (-1.1f, 1.1f, value * displayGain) * halfHeight;
    };

    std::vector<juce::Point<float>> lowerPoints;
    lowerPoints.reserve ((size_t) buckets);

    for (int b = 0; b < buckets; ++b)
    {
        const float x = plot.getX() + plot.getWidth() * (float) b / (float) (buckets - 1);
        lowerPoints.emplace_back (x, yFor (lows[(size_t) b]));

        if (b == 0)
            trace.startNewSubPath (x, yFor (highs[(size_t) b]));
        else
            trace.lineTo (x, yFor (highs[(size_t) b]));
    }

    // Заливка между верхней и нижней огибающими, обводка по обеим
    fill = trace;
    for (auto it = lowerPoints.rbegin(); it != lowerPoints.rend(); ++it)
        fill.lineTo (*it);
    fill.closeSubPath();

    trace.startNewSubPath (lowerPoints.front());
    for (size_t i = 1; i < lowerPoints.size(); ++i)
        trace.lineTo (lowerPoints[i]);

    // Искры летят от правого края, где волна "пишется"
    for (size_t i = lowerPoints.size() - juce::jmin (lowerPoints.size(), (size_t) 5); i < lowerPoints.size(); ++i)
        sparkSources.push_back (lowerPoints[i]);

    sparkSources.push_back (trace.getCurrentPosition());
    return true;
}

bool ScopeView::buildVector (juce::Rectangle<float> plot, juce::Path& trace)
{
    scope.copyLatestStereo (samples.data(), samplesRight.data(), kVectorSamples);

    float peak = 0.0f;
    for (int i = 0; i < kVectorSamples; ++i)
        peak = juce::jmax (peak, std::abs (samples[(size_t) i]), std::abs (samplesRight[(size_t) i]));

    followGain (peak, 0.9f, 0.05f);

    // Середина - по вертикали, разность каналов - по горизонтали: моно даёт вертикальную черту
    const float radius = juce::jmin (plot.getWidth(), plot.getHeight()) * 0.5f;
    const auto centre = plot.getCentre();

    for (int i = 0; i < kVectorSamples; ++i)
    {
        const float left = samples[(size_t) i] * displayGain, right = samplesRight[(size_t) i] * displayGain;
        const float x = centre.x + juce::jlimit (-1.4f, 1.4f, 0.5f * (left - right) * 1.6f) * radius;
        const float y = centre.y - juce::jlimit (-1.1f, 1.1f, 0.5f * (left + right)) * radius;

        if (i == 0)
            trace.startNewSubPath (x, y);
        else
            trace.lineTo (x, y);

        if (i % 20 == 0)
            sparkSources.emplace_back (x, y);
    }

    return true;
}

//==============================================================================
void ScopeView::updateSparks (juce::Rectangle<float> area)
{
    const double now = Visuals::now();
    const float dt = (float) juce::jlimit (0.0, 0.05, now - lastSparkTime);
    lastSparkTime = now;

    // Полёт: искры тормозятся, слегка падают и гаснут
    for (auto& spark : sparks)
    {
        spark.age += dt;
        spark.velocity.y += 70.0f * dt;
        spark.velocity *= 1.0f - 1.4f * dt;
        spark.position += spark.velocity * dt;
    }

    sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [&area] (const Spark& spark)
                  {
                      return spark.age >= spark.lifetime || ! area.contains (spark.position);
                  }),
                  sparks.end());

    const float level = visuals.getLevel();
    const float rise = juce::jmax (0.0f, level - lastLevel);
    lastLevel = level;

    if (! Settings::get().particles || ! signalPresent || sparkSources.empty())
    {
        sparkDebt = 0.0f;
        return;
    }

    // Рождение: чем громче, тем больше искр; атака даёт вспышку
    sparkDebt = juce::jmin (12.0f, sparkDebt + dt * 80.0f * std::pow (juce::jmax (0.0f, level - 0.35f), 1.5f) + rise * 120.0f);

    while (sparkDebt >= 1.0f && sparks.size() < 200)
    {
        sparkDebt -= 1.0f;

        // Из нескольких случайных точек следа берём самую дальнюю от середины: искры летят с гребней
        auto origin = sparkSources[(size_t) random.nextInt ((int) sparkSources.size())];
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            const auto other = sparkSources[(size_t) random.nextInt ((int) sparkSources.size())];
            if (std::abs (other.y - area.getCentreY()) > std::abs (origin.y - area.getCentreY()))
                origin = other;
        }

        const float away = origin.y < area.getCentreY() ? -1.0f : 1.0f;
        const float speed = 25.0f + 70.0f * random.nextFloat();

        Spark spark;
        spark.position = origin;
        spark.velocity = { (random.nextFloat() - 0.5f) * 2.0f * speed, away * speed * (0.3f + 0.7f * random.nextFloat()) };
        spark.lifetime = 0.3f + 0.6f * random.nextFloat();
        sparks.push_back (spark);
    }
}

void ScopeView::drawGrid (juce::Graphics& g, juce::Rectangle<float> plot) const
{
    if (mode() == Mode::xy)
    {
        // Оси M (середина) и S (стороны), диагонали - левый и правый каналы
        const float radius = juce::jmin (plot.getWidth(), plot.getHeight()) * 0.5f;
        const auto centre = plot.getCentre();

        g.setColour (Palette::outline.withAlpha (0.55f));
        g.drawLine (centre.x - radius, centre.y - radius, centre.x + radius, centre.y + radius, 1.0f);
        g.drawLine (centre.x - radius, centre.y + radius, centre.x + radius, centre.y - radius, 1.0f);
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.0f);

        g.setColour (Palette::outline);
        g.drawVerticalLine (juce::roundToInt (centre.x), plot.getY(), plot.getBottom());
        g.drawHorizontalLine (juce::roundToInt (centre.y), plot.getX(), plot.getRight());

        g.setColour (Palette::textFaint);
        g.setFont (makeFont (9.5f, true));
        g.drawText ("L", juce::Rectangle<float> (centre.x - radius - 14.0f, centre.y - radius - 2.0f, 12.0f, 12.0f), juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (centre.x + radius + 2.0f, centre.y - radius - 2.0f, 12.0f, 12.0f), juce::Justification::centred);
        return;
    }

    // Сетка как на экране прибора
    g.setColour (Palette::outline.withAlpha (0.55f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine (juce::roundToInt (plot.getX() + plot.getWidth() * (float) i / 8.0f), plot.getY(), plot.getBottom());
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine (juce::roundToInt (plot.getY() + plot.getHeight() * (float) i / 4.0f), plot.getX(), plot.getRight());

    g.setColour (Palette::outline);
    g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());
}

void ScopeView::timerCallback()
{
    if (shader.isAvailable() && Visuals::wantsShaders (*this))
    {
        // Послесвечение считает шейдер. У бегущей волны его нет - она и так движется.
        const auto currentMode = mode();
        const float persistence = currentMode == Mode::roll ? 0.0f : (currentMode == Mode::xy ? 0.86f : 0.7f);

        shader.render (*this, visuals.screenFx (persistence), [this] (juce::Graphics& g) { paintScreen (g, true); });
    }
    else
    {
        shader.invalidate();
    }

    repaint();
}

void ScopeView::paint (juce::Graphics& g)
{
    // С шейдерами экран уже отрисован в таймере, здесь только выводится готовая картинка
    if (! shader.draw (g, getLocalBounds().toFloat()))
        paintScreen (g, false);

    paintLabels (g);
}

void ScopeView::paintScreen (juce::Graphics& g, bool forShader)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto currentMode = mode();
    const auto& settings = Settings::get();

    g.setColour (Palette::deep);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto plot = bounds.reduced (10.0f, 12.0f);
    drawGrid (g, plot);

    juce::Path trace, fill;
    sparkSources.clear();
    const bool drawn = currentMode == Mode::wave ? buildWave (plot, trace, fill)
                     : currentMode == Mode::roll ? buildRoll (plot, trace, fill)
                                                 : buildVector (plot, trace);
    updateSparks (bounds);

    // Цвет следа идёт за тембром, свечение - за громкостью и "накалом" экрана
    const float power = visuals.getPower();
    const float glow = power * (0.65f + 0.7f * visuals.getLevel());
    const auto colour = visuals.trace();
    const auto bright = visuals.traceBright();
    const juce::PathStrokeType::JointStyle joint = currentMode == Mode::roll ? juce::PathStrokeType::beveled : juce::PathStrokeType::curved;

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (bounds.toNearestInt());

        // Послесвечение без шейдеров: прошлые кадры постепенно гаснут в отдельной картинке
        const bool persistence = settings.crtScreen && currentMode != Mode::roll && ! forShader;

        if (persistence)
        {
            const float scale = juce::jlimit (0.5f, 3.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
            const int width = juce::jmax (1, juce::roundToInt ((float) getWidth() * scale));
            const int height = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));

            if (trail.getWidth() != width || trail.getHeight() != height || trailMode != currentMode)
            {
                trail = juce::Image (juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
                trailShown = juce::Image (juce::Image::ARGB, width, height, true, juce::NativeImageType());
                trailMode = currentMode;
            }

            fadeImage (trail, currentMode == Mode::xy ? 225 : 205);

            if (drawn && signalPresent)
            {
                juce::Graphics trailGraphics (trail);
                trailGraphics.addTransform (juce::AffineTransform::scale (scale));
                trailGraphics.setColour (colour.withAlpha (juce::jmin (1.0f, (currentMode == Mode::xy ? 0.5f : 0.4f) * glow)));
                trailGraphics.strokePath (trace, { currentMode == Mode::xy ? 1.2f : 1.6f, joint, juce::PathStrokeType::rounded });
            }

            {
                const juce::Image::BitmapData from (trail, juce::Image::BitmapData::readOnly);
                juce::Image::BitmapData to (trailShown, juce::Image::BitmapData::writeOnly);

                for (int y = 0; y < height; ++y)
                    std::memcpy (to.getLinePointer (y), from.getLinePointer (y), (size_t) width * 4);
            }

            g.setOpacity (1.0f);
            g.drawImage (trailShown, getLocalBounds().toFloat());
        }
        else if (trail.isValid())
        {
            trail = {};
            trailShown = {};
        }

        if (drawn)
        {
            if (currentMode == Mode::xy)
            {
                if (! forShader)
                {
                    g.setColour (colour.withAlpha (juce::jmin (1.0f, 0.12f * glow)));
                    g.strokePath (trace, { 4.0f, joint, juce::PathStrokeType::rounded });
                }

                g.setColour (bright.withAlpha (juce::jmin (1.0f, 0.75f * power)));
                g.strokePath (trace, { 1.0f, joint, juce::PathStrokeType::rounded });
            }
            else if (currentMode == Mode::roll)
            {
                g.setColour (colour.withAlpha (juce::jmin (1.0f, 0.3f * glow)));
                g.fillPath (fill);
                g.setColour (bright.withAlpha (juce::jmin (1.0f, 0.3f + 0.6f * power)));
                g.strokePath (trace, { 1.0f, joint, juce::PathStrokeType::butt });
            }
            else
            {
                g.setColour (colour.withAlpha (juce::jmin (1.0f, 0.07f * glow)));
                g.fillPath (fill);

                // Широкий ореол нужен только без шейдеров: там свечение делает размытие
                if (! forShader)
                {
                    g.setColour (colour.withAlpha (juce::jmin (1.0f, 0.1f * glow)));
                    g.strokePath (trace, { 8.0f, joint, juce::PathStrokeType::rounded });
                }

                if (! forShader)
                {
                    g.setColour (colour.withAlpha (juce::jmin (1.0f, 0.28f * glow)));
                    g.strokePath (trace, { 3.5f, joint, juce::PathStrokeType::rounded });
                }

                g.setColour (bright.withAlpha (juce::jmin (1.0f, 0.35f + 0.65f * power)));
                g.strokePath (trace, { forShader ? 1.8f : 1.5f, joint, juce::PathStrokeType::rounded });
            }
        }

        // Искры
        for (const auto& spark : sparks)
        {
            const float fade = 1.0f - spark.age / spark.lifetime;
            const float size = 1.2f + 1.0f * fade;
            g.setColour (bright.withAlpha (juce::jmin (1.0f, fade * (0.4f + 0.6f * power))));
            g.fillRect (spark.position.x - size * 0.5f, spark.position.y - size * 0.5f, size, size);
        }
    }

    if (! forShader)
        glass.draw (g, bounds);
}

void ScopeView::paintLabels (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto currentMode = mode();

    // Режимы
    g.setFont (makeFont (9.5f, true, 0.12f));
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (i == (int) currentMode ? Palette::accentBright : Palette::textFaint);
        g.drawText (modeNames[i], tabBounds (i), juce::Justification::centredLeft);
    }

    // Подпись справа: нота и частота, окно времени
    juce::String caption;
    const float frequency = scope.noteFrequency.load();

    if (currentMode == Mode::roll)
    {
        caption = formatWindow (rollWindows[juce::jlimit (0, kNumRollWindows - 1, Settings::get().scopeWindow)]);
    }
    else if (currentMode == Mode::wave && frequency > 0.0f && signalPresent)
    {
        const int note = juce::roundToInt (69.0f + 12.0f * std::log2 (frequency / 440.0f));
        caption = juce::MidiMessage::getMidiNoteName (note, true, true, 4) + "  " + juce::String (frequency, 1) + " Hz";
    }

    if (caption.isNotEmpty())
    {
        g.setColour (Palette::textDim);
        g.setFont (makeFont (11.0f, true, 0.08f));
        g.drawText (caption, bounds.reduced (12.0f, 8.0f), juce::Justification::topRight);
    }

    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

} // namespace sonder::ui
