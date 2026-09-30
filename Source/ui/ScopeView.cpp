#include "ScopeView.h"
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
    float period = frequency > 0.0f ? sampleRate / frequency : 512.0f;

    // Зона поиска - два периода: реальная высота отличается от номинальной (дрейф, расстройка, вибрато, глайд),
    // и в зоне ровно в один период переход через ноль иногда не находился - картинка прыгала
    const int search = juce::jlimit (4, 4000, juce::roundToInt (period * 2.0f) + 4);
    const int maxWindow = ScopeBuffer::kSize - search - 2;
    int window = juce::jlimit (64, juce::jmin (4096, maxWindow), juce::roundToInt (period * juce::jlimit (2.0f, 12.0f, std::ceil (400.0f / period))));
    const int total = window + search + 2;

    scope.copyLatest (samples.data(), total);

    float peak = 0.0f;
    for (int i = 0; i < total; ++i)
        peak = juce::jmax (peak, std::abs (samples[(size_t) i]));

    // Переходы через ноль вверх с гистерезисом: перед переходом сигнал должен уйти заметно ниже нуля,
    // иначе шум около нуля даёт ложные срабатывания. Берём последний переход в зоне.
    const float threshold = 0.08f * peak;
    float trigger = 0.0f, previousTrigger = -1.0f;
    bool armed = false, found = false;

    for (int i = 1; i < search; ++i)
    {
        const float a = samples[(size_t) i - 1], b = samples[(size_t) i];

        if (b < -threshold)
            armed = true;

        if (armed && a < 0.0f && b >= 0.0f)
        {
            // Точное место перехода между сэмплами: без него высокие ноты дрожат на один сэмпл
            if (found)
                previousTrigger = trigger;

            trigger = (float) (i - 1) + a / (a - b);
            found = true;
            armed = false;
        }
    }

    // Если в зоне нашлись два перехода, период известен точно: окно покрывает целое число настоящих периодов
    if (found && previousTrigger >= 0.0f)
    {
        const float measured = trigger - previousTrigger;
        if (measured > period * 0.8f && measured < period * 1.25f)
        {
            const float cycles = juce::jmax (1.0f, std::round ((float) window / measured));
            window = juce::jlimit (64, juce::jmin (4096, maxWindow), juce::roundToInt (cycles * measured));
            period = measured;
        }
    }

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

    // Короткое окно рисуется по сэмплам, длинное - по огибающей "минимум-максимум"
    int count = 0;
    const bool raw = wanted <= ScopeBuffer::kSize - 64;

    if (raw)
    {
        count = wanted;
        scope.copyLatest (samples.data(), count);
    }
    else
    {
        count = juce::jmin (ScopeBuffer::kNumPeaks, wanted / ScopeBuffer::kPeakHop);
        scope.copyLatestPeaks (samples.data(), samplesRight.data(), count);
    }

    if (count < 2)
        return false;

    const float* minimums = samples.data();
    const float* maximums = raw ? samples.data() : samplesRight.data();

    // По столбцам экрана: в каждом минимум и максимум
    const int columns = juce::jmax (2, (int) plot.getWidth());
    float peak = 0.0f;

    for (int i = 0; i < count; ++i)
        peak = juce::jmax (peak, std::abs (minimums[i]), std::abs (maximums[i]));

    followGain (peak, 0.9f, 0.04f);

    const float halfHeight = plot.getHeight() * 0.5f;
    const auto yFor = [&] (float value)
    {
        return plot.getCentreY() - juce::jlimit (-1.1f, 1.1f, value * displayGain) * halfHeight;
    };

    std::vector<juce::Point<float>> lowerPoints;
    lowerPoints.reserve ((size_t) columns);

    for (int column = 0; column < columns; ++column)
    {
        const int from = (int) ((juce::int64) column * count / columns);
        const int to = juce::jmax (from + 1, (int) ((juce::int64) (column + 1) * count / columns));
        float low = minimums[from], high = maximums[from];

        for (int i = from + 1; i < juce::jmin (to, count); ++i)
        {
            low = juce::jmin (low, minimums[i]);
            high = juce::jmax (high, maximums[i]);
        }

        const float x = plot.getX() + plot.getWidth() * (float) column / (float) (columns - 1);
        lowerPoints.emplace_back (x, yFor (low));

        if (column == 0)
            trace.startNewSubPath (x, yFor (high));
        else
            trace.lineTo (x, yFor (high));
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
