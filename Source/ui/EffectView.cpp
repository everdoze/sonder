#include "EffectView.h"
#include "PluginProcessor.h"
#include "SonderLookAndFeel.h"
#include "Theme.h"
#include "Visuals.h"

#include <complex>

namespace sonder::ui
{

namespace
{
    using Complex = std::complex<float>;

    constexpr float kMinFrequency = 20.0f, kMaxFrequency = 20000.0f;
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
    constexpr float kEqRangeDb = 20.0f;

    float xForFrequency (juce::Rectangle<float> plot, float frequency)
    {
        return plot.getX() + plot.getWidth() * std::log (frequency / kMinFrequency) / std::log (kMaxFrequency / kMinFrequency);
    }

    float frequencyForX (juce::Rectangle<float> plot, float x)
    {
        const float proportion = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
        return kMinFrequency * std::pow (kMaxFrequency / kMinFrequency, proportion);
    }

    // Линия с подсветкой для включённого эффекта и тусклая для выключенного
    void strokeCurve (juce::Graphics& g, const juce::Path& path, bool active, float thickness = 1.6f)
    {
        if (active)
        {
            g.setColour (Palette::accent.withAlpha (0.14f));
            g.strokePath (path, { thickness * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
        }

        g.setColour (active ? Palette::accentBright : Palette::textFaint);
        g.strokePath (path, { thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
    }

    void fillToBaseline (juce::Graphics& g, juce::Path path, juce::Rectangle<float> plot, float baselineY, bool active)
    {
        path.lineTo (plot.getRight(), baselineY);
        path.lineTo (plot.getX(), baselineY);
        path.closeSubPath();
        g.setColour (Palette::accent.withAlpha (active ? 0.14f : 0.04f));
        g.fillPath (path);
    }

    void drawFrequencyGrid (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        for (float frequency : { 100.0f, 1000.0f, 10000.0f })
        {
            g.setColour (Palette::outline.withAlpha (0.7f));
            g.drawVerticalLine (juce::roundToInt (xForFrequency (plot, frequency)), plot.getY(), plot.getBottom());
        }
    }

    // АЧХ на логарифмической оси частот; response возвращает комплексный или вещественный коэффициент передачи
    template <typename Response>
    void drawResponse (juce::Graphics& g, juce::Rectangle<float> plot, float minDb, float maxDb, bool active, Response&& response)
    {
        drawFrequencyGrid (g, plot);

        const auto yForDb = [&] (float db)
        {
            return plot.getY() + plot.getHeight() * (maxDb - juce::jlimit (minDb, maxDb, db)) / (maxDb - minDb);
        };

        g.setColour (Palette::outline);
        g.drawHorizontalLine (juce::roundToInt (yForDb (0.0f)), plot.getX(), plot.getRight());

        juce::Path curve;
        const int steps = juce::jmax (64, (int) plot.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const float frequency = kMinFrequency * std::pow (kMaxFrequency / kMinFrequency, (float) i / (float) steps);
            const float db = juce::Decibels::gainToDecibels (std::abs (response (frequency)), -80.0f);
            const juce::Point<float> point (xForFrequency (plot, frequency), yForDb (db));

            if (i == 0)
                curve.startNewSubPath (point);
            else
                curve.lineTo (point);
        }

        fillToBaseline (g, curve, plot, yForDb (0.0f), active);
        strokeCurve (g, curve, active);
    }

    float hash01 (uint32_t value)
    {
        value ^= value >> 15;
        value *= 0x2c1b3c6du;
        value ^= value >> 12;
        value *= 0x297a2d39u;
        value ^= value >> 15;
        return (float) (value >> 8) / 16777216.0f;
    }

    juce::String formatSeconds (float seconds)
    {
        return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms" : juce::String (seconds, 2) + " s";
    }

    void drawCaption (juce::Graphics& g, juce::Rectangle<float> plot, const juce::String& text, bool active,
                      juce::Justification justification = juce::Justification::topRight)
    {
        g.setColour (active ? Palette::textDim : Palette::textFaint);
        g.setFont (makeFont (10.5f, true, 0.08f));
        g.drawText (text, plot, justification);
    }
}

EffectView::EffectView (SonderAudioProcessor& p, int slotIndex)
    : processor (p), slot (slotIndex)
{
    startTimerHz (30);
}

FxType EffectView::type() const
{
    return processor.fxRack.getType (slot);
}

bool EffectView::isOn() const
{
    return processor.parameters.getRawParameterValue (ParamIDs::fxOn (slot))->load() > 0.5f;
}

float EffectView::value (int param) const
{
    return processor.fxController.getRealValue (slot, param);
}

std::array<float, kNumFxParams> EffectView::values() const
{
    std::array<float, kNumFxParams> result {};
    for (int i = 0; i < kNumFxParams; ++i)
        result[(size_t) i] = value (i);

    return result;
}

juce::Rectangle<float> EffectView::plotArea() const
{
    return getLocalBounds().toFloat().reduced (1.0f).reduced (10.0f, 9.0f);
}

bool EffectView::hitTest (int, int)
{
    // Мышь нужна только эквалайзеру: у него перетаскиваются точки полос
    return type() == FxType::equalizer;
}

void EffectView::render (bool immediate)
{
    if (shader.isAvailable() && Visuals::wantsShaders (*this))
    {
        // Эквалайзер таскают мышью: он искривляется слабее
        const auto fx = Visuals::staticScreenFx (type() == FxType::equalizer);
        shader.render (*this, fx, [this] (juce::Graphics& g) { paintScreen (g, true); }, immediate);
    }
    else
    {
        shader.invalidate();
    }
}

void EffectView::timerCallback()
{
    render (dragBand >= 0);
    repaint();
}

void EffectView::refresh()
{
    render (true);
    repaint();
}

void EffectView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    if (! shader.draw (g, getLocalBounds().toFloat()))
        paintScreen (g, false);

    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
}

void EffectView::paintScreen (juce::Graphics& g, bool forShader)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Palette::deep);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto plot = plotArea();
    const bool active = isOn();

    switch (type())
    {
        case FxType::distortion: paintDistortion (g, plot, active); break;
        case FxType::phaser:     paintPhaser (g, plot, active); break;
        case FxType::flanger:    paintFlanger (g, plot, active); break;
        case FxType::chorus:     paintChorus (g, plot, active); break;
        case FxType::delay:      paintDelay (g, plot, active); break;
        case FxType::compressor: paintCompressor (g, plot, active); break;
        case FxType::reverb:     paintReverb (g, plot, active); break;
        case FxType::equalizer:  paintEqualizer (g, plot, active); break;
        case FxType::filter:     paintFilter (g, plot, active); break;
        case FxType::tremolo:    paintTremolo (g, plot, active); break;
        case FxType::widener:    paintWidener (g, plot, active); break;
        case FxType::limiter:    paintLimiter (g, plot, active); break;
        case FxType::multiband:  paintMultiband (g, plot, active); break;
        case FxType::shifter:    paintShifter (g, plot, active); break;
        case FxType::none:
        case FxType::count:      break;
    }

    if (! forShader)
        glass.draw (g, bounds);
}

//==============================================================================
void EffectView::paintDistortion (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const auto settings = Distortion::makeSettings (MasterDistortion::typeFromParameter (value (fxp::distortion::type)),
                                                    value (fxp::distortion::drive), value (fxp::distortion::mix), 1.0f);

    const auto toPoint = [&plot] (float in, float out)
    {
        return juce::Point<float> (plot.getCentreX() + in * plot.getWidth() * 0.5f,
                                   plot.getCentreY() - juce::jlimit (-1.0f, 1.0f, out) * plot.getHeight() * 0.5f);
    };
    const auto output = [&settings] (float in) { return in + (Distortion::shape (in, settings) - in) * settings.mix; };

    // Оси и диагональ "без искажений"
    g.setColour (Palette::outline);
    g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());
    g.drawVerticalLine (juce::roundToInt (plot.getCentreX()), plot.getY(), plot.getBottom());
    g.drawLine ({ toPoint (-1.0f, -1.0f), toPoint (1.0f, 1.0f) }, 1.0f);

    // Диапазон, в котором сейчас находится сигнал
    const float peak = active ? juce::jmin (1.0f, processor.getFxChain().getSlot (slot).distortion.getDisplayPeak()) : 0.0f;
    if (peak > 0.001f)
    {
        g.setColour (Palette::accent.withAlpha (0.1f));
        g.fillRect (juce::Rectangle<float> (toPoint (-peak, 0.0f).x, plot.getY(), peak * plot.getWidth(), plot.getHeight()));
    }

    juce::Path curve;
    const int steps = juce::jmax (64, (int) plot.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const float in = -1.0f + 2.0f * (float) i / (float) steps;
        const auto point = toPoint (in, output (in));
        if (i == 0)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    strokeCurve (g, curve, active);

    if (peak > 0.001f)
    {
        for (float in : { -peak, peak })
        {
            const auto dot = toPoint (in, output (in));
            g.setColour (Palette::accent.withAlpha (0.3f));
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (dot));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (dot));
        }
    }
}

void EffectView::paintPhaser (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const float feedback = value (fxp::phaser::feedback);
    const float mix = value (fxp::phaser::mix);
    const int stages = Phaser::stagesFromParameter (value (fxp::phaser::stages));
    const float sampleRate = processor.scope.sampleRate.load();

    // Пока эффект работает, показываем текущее положение провалов; иначе - среднее
    const float centre = active ? processor.getFxChain().getSlot (slot).phaser.getDisplayFrequency()
                                : value (fxp::phaser::centre);

    const float t = std::tan (juce::MathConstants<float>::pi * juce::jmin (centre, 0.45f * sampleRate) / sampleRate);
    const float a = (t - 1.0f) / (t + 1.0f);

    drawResponse (g, plot, -30.0f, 12.0f, active, [&] (float frequency)
    {
        const Complex z1 = std::polar (1.0f, -kTwoPi * frequency / sampleRate);
        const Complex allpass = (a + z1) / (1.0f + a * z1);
        const Complex chain = std::pow (allpass, (float) stages);
        const Complex wet = chain / (1.0f - feedback * chain);
        return 1.0f + 0.5f * mix * (wet - 1.0f);
    });
}

void EffectView::paintFlanger (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const float feedback = value (fxp::flanger::feedback);
    const float mix = value (fxp::flanger::mix);
    const float sampleRate = processor.scope.sampleRate.load();

    const float delayMs = active ? processor.getFxChain().getSlot (slot).flanger.getDisplayDelayMs()
                                 : Flanger::delayMs (0.0f, value (fxp::flanger::depth), value (fxp::flanger::delay));
    const float delaySamples = delayMs * 0.001f * sampleRate;

    drawResponse (g, plot, -30.0f, 12.0f, active, [&] (float frequency)
    {
        const Complex delayed = std::polar (1.0f, -kTwoPi * frequency / sampleRate * delaySamples);
        const Complex wet = delayed / (1.0f - feedback * delayed);
        return 1.0f + 0.5f * mix * (wet - 1.0f);
    });
}

void EffectView::paintChorus (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const auto mode = static_cast<Chorus::Mode> (juce::jlimit (0, 3, juce::roundToInt (value (fxp::chorus::mode))));
    const auto motion = Chorus::motionFor (mode, value (fxp::chorus::rate), value (fxp::chorus::depth));
    const float width = value (fxp::chorus::width);

    // Высота кривой - глубина качания задержки (полная высота экрана - 3 мс)
    const float depth = juce::jlimit (0.08f, 1.0f, motion.depthMs / 3.0f);
    const float phase = processor.getFxChain().getSlot (slot).chorus.getDisplayPhase();
    const auto triangle = [] (float p) { return 1.0f - 4.0f * std::abs (p - std::floor (p) - 0.5f); };
    const auto toPoint = [&] (float p, float v)
    {
        return juce::Point<float> (plot.getX() + p * plot.getWidth(), plot.getCentreY() - v * depth * plot.getHeight() * 0.5f);
    };

    g.setColour (Palette::outline);
    g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());

    // Левый канал и правый: при полной ширине они качаются в противофазе
    for (float scale : { 1.0f, 1.0f - 2.0f * width })
    {
        const bool leftChannel = scale == 1.0f;
        juce::Path curve;
        for (int i = 0; i <= 80; ++i)
        {
            const float p = (float) i / 80.0f;
            const auto point = toPoint (p, scale * triangle (p));
            if (i == 0)
                curve.startNewSubPath (point);
            else
                curve.lineTo (point);
        }

        strokeCurve (g, curve, active && leftChannel, 1.4f);

        if (active)
        {
            const auto dot = toPoint (phase, scale * triangle (phase));
            g.setColour (Palette::accent.withAlpha (0.3f));
            g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (dot));
            g.setColour (leftChannel ? juce::Colours::white : Palette::accentBright);
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (dot));
        }
    }

    drawCaption (g, plot, juce::String (motion.rateHz, 2) + " Hz", active);
}

void EffectView::paintDelay (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const auto p = values();
    const float feedback = p[fxp::delay::feedback];
    const float width = p[fxp::delay::width];
    const float seconds = TapeDelay::delaySeconds (p.data(), processor.displayBpm.load());

    const float centreY = plot.getCentreY();
    g.setColour (Palette::outline);
    g.drawHorizontalLine (juce::roundToInt (centreY), plot.getX(), plot.getRight());

    // Исходный звук в начале шкалы
    g.setColour (active ? Palette::text.withAlpha (0.8f) : Palette::textFaint);
    g.fillRect (juce::Rectangle<float> (plot.getX(), plot.getY() + 4.0f, 2.0f, plot.getHeight() - 8.0f));

    // Повторы: вверх - левый канал, вниз - правый. В пинг-понге они чередуются,
    // при Width = 0 каждый повтор звучит в обоих каналах.
    constexpr int maxTaps = 9;
    const float step = plot.getWidth() / ((float) maxTaps + 0.5f);
    const float fullLength = plot.getHeight() * 0.5f - 6.0f;
    const auto colour = active ? Palette::accent : Palette::textFaint;

    for (int tap = 1; tap <= maxTaps; ++tap)
    {
        const float amplitude = std::pow (feedback, (float) (tap - 1));
        if (amplitude < 0.02f)
            break;

        const float x = plot.getX() + step * (float) tap;
        const bool odd = tap % 2 == 1;
        const float levels[2] { amplitude * (odd ? 1.0f : 1.0f - width), amplitude * (odd ? 1.0f - width : 1.0f) };

        for (int channel = 0; channel < 2; ++channel)
        {
            const float length = levels[channel] * fullLength;
            if (length < 1.0f)
                continue;

            const float endY = channel == 0 ? centreY - length : centreY + length;
            g.setColour (colour.withAlpha (0.35f + 0.65f * amplitude));
            g.fillRect (juce::Rectangle<float> (x - 1.0f, juce::jmin (centreY, endY), 2.0f, length));
            g.setColour (active ? Palette::accentBright : Palette::textFaint);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ x, endY }));
        }
    }

    drawCaption (g, plot, formatSeconds (seconds), active);

    g.setColour (Palette::textFaint);
    g.setFont (makeFont (9.5f, true));
    g.drawText ("L", plot.withTrimmedLeft (6.0f).withHeight (12.0f), juce::Justification::topLeft);
    g.drawText ("R", plot.withTrimmedLeft (6.0f).withTop (plot.getBottom() - 12.0f), juce::Justification::bottomLeft);
}

void EffectView::paintCompressor (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const float threshold = value (fxp::compressor::threshold);
    const float ratio = value (fxp::compressor::ratio);
    const float knee = value (fxp::compressor::knee);
    const float makeup = value (fxp::compressor::makeup);
    const auto& compressor = processor.getFxChain().getSlot (slot).compressor;

    constexpr float minDb = -60.0f;
    const auto meter = plot.removeFromRight (10.0f);
    plot.removeFromRight (8.0f);

    const auto toPoint = [&] (float inDb, float outDb)
    {
        return juce::Point<float> (plot.getX() + plot.getWidth() * (inDb - minDb) / -minDb,
                                   plot.getBottom() - plot.getHeight() * (juce::jlimit (minDb, 0.0f, outDb) - minDb) / -minDb);
    };
    const auto output = [&] (float inDb) { return inDb - Compressor::gainReductionFor (inDb, threshold, ratio, knee) + makeup; };

    // Диагональ "без обработки" и порог
    g.setColour (Palette::outline);
    g.drawLine ({ toPoint (minDb, minDb), toPoint (0.0f, 0.0f) }, 1.0f);
    g.setColour (Palette::accent.withAlpha (active ? 0.35f : 0.12f));
    g.fillRect (juce::Rectangle<float> (toPoint (threshold, 0.0f).x - 0.5f, plot.getY(), 1.0f, plot.getHeight()));

    juce::Path curve;
    for (int i = 0; i <= 120; ++i)
    {
        const float inDb = minDb * (1.0f - (float) i / 120.0f);
        const auto point = toPoint (inDb, output (inDb));
        if (i == 0)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    strokeCurve (g, curve, active);

    // Текущий уровень на кривой
    const float input = compressor.getDisplayInput();
    if (active && input > minDb)
    {
        const auto dot = toPoint (input, output (input));
        g.setColour (Palette::accent.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (dot));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (5.5f, 5.5f).withCentre (dot));
    }

    // Индикатор подавления: столбик растёт сверху вниз, полная высота - 24 дБ
    const float reduction = active ? compressor.getDisplayReduction() : 0.0f;
    g.setColour (Palette::track);
    g.fillRoundedRectangle (meter, 3.0f);
    g.setColour (Palette::accent);
    g.fillRoundedRectangle (meter.withHeight (meter.getHeight() * juce::jlimit (0.0f, 1.0f, reduction / 24.0f)), 3.0f);

    drawCaption (g, plot, "GR  -" + juce::String (reduction, 1) + " dB", active, juce::Justification::topLeft);
}

void EffectView::paintReverb (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const float decay = value (fxp::reverb::decay);
    const float preDelay = value (fxp::reverb::preDelay) * 0.001f;
    const float mix = value (fxp::reverb::mix);
    const float damping = value (fxp::reverb::damping);

    // Ширина экрана подстраивается под длину хвоста
    const float spanSeconds = juce::jmax (1.5f, decay * 1.15f + preDelay);
    const float centreY = plot.getCentreY();
    const auto envelope = [&] (float seconds)
    {
        // Хвост начинается после предзадержки и падает на 60 дБ за время Decay
        return seconds < preDelay ? 0.0f : std::exp (-4.0f * (seconds - preDelay) / decay);
    };

    g.setColour (Palette::outline);
    g.drawHorizontalLine (juce::roundToInt (centreY), plot.getX(), plot.getRight());

    // Прямой звук
    g.setColour (active ? Palette::text.withAlpha (0.8f) : Palette::textFaint);
    g.fillRect (juce::Rectangle<float> (plot.getX(), plot.getY() + 4.0f, 2.0f, plot.getHeight() - 8.0f));

    // Хвост: плотные отражения со случайной высотой под общей огибающей; Damping делает конец хвоста тусклее
    const auto colour = active ? Palette::accent : Palette::textFaint;
    for (float x = 3.0f; x < plot.getWidth(); x += 3.0f)
    {
        const float seconds = x / plot.getWidth() * spanSeconds;
        const float level = envelope (seconds);
        if (level < 0.004f)
            continue;

        const float height = level * (0.35f + 0.65f * hash01 ((uint32_t) x + 7u)) * (plot.getHeight() * 0.5f - 3.0f);
        const float brightness = 1.0f - damping * juce::jlimit (0.0f, 1.0f, (seconds - preDelay) / decay);
        g.setColour (colour.withAlpha ((active ? 0.25f + 0.6f * mix : 0.25f) * (0.35f + 0.65f * brightness)));
        g.fillRect (juce::Rectangle<float> (plot.getX() + x, centreY - height, 1.5f, height * 2.0f));
    }

    juce::Path outline;
    bool started = false;
    for (int i = 0; i <= 120; ++i)
    {
        const float p = (float) i / 120.0f;
        const float seconds = p * spanSeconds;
        if (seconds < preDelay)
            continue;

        const juce::Point<float> point (plot.getX() + p * plot.getWidth(),
                                        centreY - envelope (seconds) * (plot.getHeight() * 0.5f - 3.0f));
        if (! started)
            outline.startNewSubPath (point);
        else
            outline.lineTo (point);

        started = true;
    }

    strokeCurve (g, outline, active, 1.3f);
    drawCaption (g, plot, formatSeconds (decay), active);
}

//==============================================================================
const std::array<EffectView::EqBand, 4>& EffectView::eqBands()
{
    using namespace fxp::equalizer;
    static const std::array<EqBand, 4> bands { { { lowFreq, lowGain, -1 },
                                                 { mid1Freq, mid1Gain, mid1Q },
                                                 { mid2Freq, mid2Gain, mid2Q },
                                                 { highFreq, highGain, -1 } } };
    return bands;
}

juce::Point<float> EffectView::eqPoint (int band, juce::Rectangle<float> plot) const
{
    const auto& b = eqBands()[(size_t) band];
    const float gain = value (b.gainParam);
    return { xForFrequency (plot, value (b.frequencyParam)),
             plot.getY() + plot.getHeight() * (kEqRangeDb - gain) / (2.0f * kEqRangeDb) };
}

int EffectView::eqBandAt (juce::Point<float> position) const
{
    const auto plot = plotArea();
    int best = -1;
    float bestDistance = 14.0f;

    for (int band = 0; band < 4; ++band)
    {
        const float distance = eqPoint (band, plot).getDistanceFrom (position);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = band;
        }
    }

    return best;
}

void EffectView::paintEqualizer (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const auto p = values();
    const float sampleRate = processor.scope.sampleRate.load();
    const auto bands = Equalizer::makeBands (p.data(), sampleRate);

    drawResponse (g, plot, -kEqRangeDb, kEqRangeDb, active,
                  [&] (float frequency) { return Equalizer::response (bands, frequency, sampleRate); });

    // Точки полос: их можно таскать
    for (int band = 0; band < 4; ++band)
    {
        const auto dot = eqPoint (band, plot);
        const bool highlighted = band == hoverBand || band == dragBand;
        const float size = highlighted ? 11.0f : 8.0f;

        if (highlighted)
        {
            g.setColour (Palette::accent.withAlpha (0.3f));
            g.fillEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (dot));
        }

        g.setColour (Palette::deep);
        g.fillEllipse (juce::Rectangle<float> (size + 3.0f, size + 3.0f).withCentre (dot));
        g.setColour (active ? Palette::accentBright : Palette::textFaint);
        g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (dot));
    }

    // Подпись у полосы под курсором
    const int shown = dragBand >= 0 ? dragBand : hoverBand;
    if (shown >= 0)
    {
        const auto& b = eqBands()[(size_t) shown];
        const auto& info = getFxTypeInfo (FxType::equalizer);
        auto text = info.params[(size_t) b.frequencyParam].text (value (b.frequencyParam)) + "   "
                  + info.params[(size_t) b.gainParam].text (value (b.gainParam));

        if (b.qParam >= 0)
            text += "   Q " + juce::String (value (b.qParam), 2);

        drawCaption (g, plot, text, true, juce::Justification::topLeft);
    }
}

//==============================================================================
void EffectView::paintFilter (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const auto p = values();
    const float sampleRate = processor.scope.sampleRate.load();
    const float cutoff = active ? processor.getFxChain().getSlot (slot).filter.getDisplayCutoff() : p[fxp::filter::cutoff];
    const auto mode = FilterFx::modeFromParameter (p[fxp::filter::mode]);
    const float resonance = p[fxp::filter::resonance];

    drawResponse (g, plot, -30.0f, 18.0f, active, [&] (float frequency)
    {
        return LadderFilter::response (frequency, cutoff, resonance, mode, sampleRate);
    });

    // Диапазон, по которому ходит LFO
    if (p[fxp::filter::depth] > 0.01f)
    {
        const float low = xForFrequency (plot, juce::jlimit (kMinFrequency, kMaxFrequency, FilterFx::cutoffFor (p.data(), -1.0f)));
        const float high = xForFrequency (plot, juce::jlimit (kMinFrequency, kMaxFrequency, FilterFx::cutoffFor (p.data(), 1.0f)));
        g.setColour (Palette::accent.withAlpha (active ? 0.25f : 0.1f));
        g.fillRect (juce::Rectangle<float> (low, plot.getBottom() - 3.0f, high - low, 3.0f));
    }

    drawCaption (g, plot, getFxTypeInfo (FxType::filter).params[fxp::filter::cutoff].text (cutoff), active);
}

void EffectView::paintTremolo (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const auto p = values();
    const bool autopan = p[fxp::tremolo::mode] > 0.5f;
    const int shape = juce::roundToInt (p[fxp::tremolo::shape]);
    const float depth = p[fxp::tremolo::depth];
    const float phase = processor.getFxChain().getSlot (slot).tremolo.getDisplayPhase();

    // Тремоло: громкость от 0 (низ) до 1 (верх). Автопан: положение от левого (верх) до правого (низ).
    const auto yFor = [&] (float value)
    {
        const float normalised = autopan ? 0.5f - 0.5f * depth * value : 1.0f - depth * (0.5f - 0.5f * value);
        return plot.getBottom() - normalised * plot.getHeight();
    };

    if (autopan)
    {
        g.setColour (Palette::outline);
        g.drawHorizontalLine (juce::roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());
        g.setColour (Palette::textFaint);
        g.setFont (makeFont (9.5f, true));
        g.drawText ("L", plot.withWidth (12.0f).withHeight (12.0f), juce::Justification::centredLeft);
        g.drawText ("R", plot.withWidth (12.0f).withTrimmedTop (plot.getHeight() - 12.0f), juce::Justification::centredLeft);
    }

    juce::Path curve;
    const int steps = juce::jmax (64, (int) plot.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        const juce::Point<float> point (plot.getX() + t * plot.getWidth(), yFor (Tremolo::shapeValue (shape, t)));
        if (i == 0)
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    strokeCurve (g, curve, active);

    if (active)
    {
        const juce::Point<float> dot (plot.getX() + phase * plot.getWidth(), yFor (Tremolo::shapeValue (shape, phase)));
        g.setColour (Palette::accent.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (dot));
        g.setColour (Palette::accentBright);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (dot));
    }

    const auto& info = getFxTypeInfo (FxType::tremolo);
    const auto rate = p[fxp::tremolo::sync] > 0.5f ? info.params[fxp::tremolo::sync].text (p[fxp::tremolo::sync])
                                                   : info.params[fxp::tremolo::rate].text (p[fxp::tremolo::rate]);
    drawCaption (g, plot, rate, active);
}

void EffectView::paintWidener (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const auto p = values();
    const float width = p[fxp::widener::width];
    const float correlation = active ? processor.getFxChain().getSlot (slot).widener.getDisplayCorrelation() : 1.0f;

    // Стерео-картина: веер из центра внизу, чем шире - тем больше угол
    const auto meter = plot.removeFromTop (10.0f);
    plot.removeFromTop (4.0f);

    const juce::Point<float> origin (plot.getCentreX(), plot.getBottom());
    const float angle = juce::jlimit (0.03f, 1.45f, 0.55f * width + 0.3f * p[fxp::widener::spread]);
    const float length = plot.getHeight() / std::cos (juce::jmin (angle, 1.0f)) * 0.95f;
    const juce::Point<float> leftEdge (origin.x - std::sin (angle) * length, origin.y - std::cos (angle) * length);
    const juce::Point<float> rightEdge (origin.x + std::sin (angle) * length, origin.y - std::cos (angle) * length);

    juce::Path fan;
    fan.startNewSubPath (origin);
    fan.lineTo (leftEdge);
    fan.lineTo (rightEdge);
    fan.closeSubPath();

    g.setColour (Palette::accent.withAlpha (active ? 0.16f : 0.05f));
    g.fillPath (fan);

    juce::Path edges;
    edges.startNewSubPath (leftEdge);
    edges.lineTo (origin);
    edges.lineTo (rightEdge);
    strokeCurve (g, edges, active);

    g.setColour (Palette::outline);
    g.drawVerticalLine (juce::roundToInt (origin.x), plot.getY(), plot.getBottom());

    // Корреляция: -1 слева, +1 справа
    g.setColour (Palette::track);
    g.fillRoundedRectangle (meter, 3.0f);
    const float x = meter.getX() + (correlation + 1.0f) * 0.5f * meter.getWidth();
    g.setColour (active ? (correlation < 0.0f ? juce::Colour (0xffff6b5e) : Palette::accentBright) : Palette::textFaint);
    g.fillRect (juce::Rectangle<float> (x - 1.5f, meter.getY(), 3.0f, meter.getHeight()));

    drawCaption (g, plot, getFxTypeInfo (FxType::widener).params[fxp::widener::width].text (width), active);
}

void EffectView::paintLimiter (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const auto p = values();
    const auto& limiter = processor.getFxChain().getSlot (slot).limiter;
    const float reduction = active ? limiter.getDisplayReduction() : 0.0f;
    const float input = active ? limiter.getDisplayInput() : -60.0f;
    const float output = active ? limiter.getDisplayOutput() : -60.0f;
    const float ceiling = p[fxp::limiter::ceiling];

    constexpr float minDb = -36.0f;
    const auto yFor = [&] (float db, juce::Rectangle<float> area)
    {
        return area.getBottom() - juce::jlimit (0.0f, 1.0f, (db - minDb) / -minDb) * area.getHeight();
    };

    // Подписи под столбиками, числа справа
    auto labels = plot.removeFromBottom (12.0f);
    auto readout = plot.removeFromRight (plot.getWidth() * 0.4f);
    labels.removeFromRight (readout.getWidth());

    // Три столбика: вход, подавление (сверху вниз), выход с линией потолка
    const float columnWidth = juce::jmin (24.0f, plot.getWidth() / 5.0f);
    const float gap = (plot.getWidth() - 3.0f * columnWidth) / 4.0f;
    const auto column = [&] (int index) { return plot.withX (plot.getX() + gap + (float) index * (columnWidth + gap)).withWidth (columnWidth); };
    const auto colour = active ? Palette::accent : Palette::textFaint;

    for (int i = 0; i < 3; ++i)
    {
        g.setColour (Palette::track);
        g.fillRoundedRectangle (column (i), 2.0f);
    }

    const auto in = column (0), gr = column (1), out = column (2);
    g.setColour (colour.withAlpha (0.6f));
    g.fillRect (in.withTop (yFor (input, in)));

    const float reductionHeight = juce::jlimit (0.0f, 1.0f, -reduction / 24.0f) * gr.getHeight();
    g.setColour (active ? juce::Colour (0xffff8a3d) : Palette::textFaint);
    g.fillRect (gr.withHeight (reductionHeight));

    g.setColour (colour);
    g.fillRect (out.withTop (yFor (output, out)));
    g.setColour (Palette::accentBright);
    g.fillRect (juce::Rectangle<float> (out.getX() - 3.0f, yFor (ceiling, out) - 0.5f, out.getWidth() + 6.0f, 1.0f));

    g.setColour (Palette::textFaint);
    g.setFont (makeFont (9.0f, true));
    for (auto [area, text] : { std::pair (in, "IN"), std::pair (gr, "GR"), std::pair (out, "OUT") })
        g.drawText (text, labels.withX (area.getX() - 10.0f).withWidth (area.getWidth() + 20.0f), juce::Justification::centred);

    drawCaption (g, readout, "GR " + juce::String (reduction, 1) + " dB", active);
    g.setColour (Palette::textFaint);
    g.setFont (makeFont (9.5f, true));
    const auto levelText = [] (float db) { return db <= -59.0f ? juce::String ("-inf") : juce::String (db, 1); };
    g.drawText ("IN  " + levelText (input), readout.withTrimmedTop (readout.getHeight() - 28.0f).withHeight (13.0f), juce::Justification::centredRight);
    g.drawText ("OUT " + levelText (output), readout.withTrimmedTop (readout.getHeight() - 14.0f), juce::Justification::centredRight);
}

void EffectView::paintMultiband (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const auto& multiband = processor.getFxChain().getSlot (slot).multiband;
    constexpr float minDb = -72.0f;
    const auto yFor = [&] (float db) { return plot.getBottom() - juce::jlimit (0.0f, 1.0f, (db - minDb) / -minDb) * plot.getHeight(); };

    const auto labels = plot.removeFromBottom (12.0f);

    // Пороги: выше верхнего громкое прижимается, ниже нижнего тихое вытягивается
    for (float threshold : { Multiband::kDownThresholdDb, Multiband::kUpThresholdDb })
    {
        g.setColour (Palette::outline);
        g.drawHorizontalLine (juce::roundToInt (yFor (threshold)), plot.getX(), plot.getRight());
    }

    const char* names[] { "LOW", "MID", "HIGH" };
    const float columnWidth = plot.getWidth() / 3.0f;

    for (int band = 0; band < Multiband::kNumBands; ++band)
    {
        const auto column = plot.withX (plot.getX() + (float) band * columnWidth).withWidth (columnWidth).reduced (columnWidth * 0.22f, 0.0f);
        const float level = active ? multiband.getDisplayLevel (band) : minDb;
        const float gain = active ? multiband.getDisplayGain (band) : 0.0f;

        g.setColour (Palette::track);
        g.fillRoundedRectangle (column, 2.0f);

        // Уровень полосы и куда его сдвинули: вверх - светлым, вниз - оранжевым
        const float from = yFor (level), to = yFor (level + gain);
        g.setColour ((active ? Palette::accent : Palette::textFaint).withAlpha (0.45f));
        g.fillRect (column.withTop (juce::jmax (from, to)));

        if (active && std::abs (gain) > 0.2f)
        {
            g.setColour (gain > 0.0f ? Palette::accentBright : juce::Colour (0xffff8a3d));
            g.fillRect (column.withTop (juce::jmin (from, to)).withBottom (juce::jmax (from, to)));
        }

        g.setColour (Palette::textFaint);
        g.setFont (makeFont (9.0f, true));
        g.drawText (names[band], labels.withX (column.getX()).withWidth (column.getWidth()), juce::Justification::centred);
    }
}

void EffectView::paintShifter (juce::Graphics& g, juce::Rectangle<float> plot, bool active)
{
    const auto p = values();
    const float shift = FreqShifter::shiftHz (p.data());
    const float spread = p[fxp::shifter::spread];
    const float phase = processor.getFxChain().getSlot (slot).shifter.getDisplayPhase();

    // Фазор слева: вращается со скоростью сдвига
    const float radius = juce::jmin (plot.getHeight() * 0.38f, plot.getWidth() * 0.18f);
    const juce::Point<float> centre (plot.getX() + radius + 4.0f, plot.getCentreY());
    g.setColour (Palette::outline);
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.0f);

    const float angle = juce::MathConstants<float>::twoPi * phase;
    const juce::Point<float> tip (centre.x + radius * std::cos (angle), centre.y - radius * std::sin (angle));
    juce::Path hand;
    hand.startNewSubPath (centre);
    hand.lineTo (tip);
    strokeCurve (g, hand, active);

    // Ось сдвига: от -2 кГц до +2 кГц
    auto axis = plot.withTrimmedLeft (radius * 2.0f + 16.0f);
    const auto xFor = [&axis] (float hz) { return axis.getX() + (juce::jlimit (-2020.0f, 2020.0f, hz) + 2020.0f) / 4040.0f * axis.getWidth(); };

    g.setColour (Palette::outline);
    g.drawHorizontalLine (juce::roundToInt (axis.getCentreY()), axis.getX(), axis.getRight());
    g.drawVerticalLine (juce::roundToInt (xFor (0.0f)), axis.getY() + 6.0f, axis.getBottom() - 6.0f);

    const auto marker = [&] (float hz, float alpha)
    {
        const float x = xFor (hz);
        g.setColour (Palette::accent.withAlpha ((active ? 0.3f : 0.1f) * alpha));
        g.fillRect (juce::Rectangle<float> (x - 3.0f, axis.getY(), 6.0f, axis.getHeight()));
        g.setColour ((active ? Palette::accentBright : Palette::textFaint).withAlpha (alpha));
        g.fillRect (juce::Rectangle<float> (x - 1.0f, axis.getY(), 2.0f, axis.getHeight()));
    };

    if (spread > 0.01f)
        marker (shift * (1.0f - 2.0f * spread), 0.5f);

    marker (shift, 1.0f);
    drawCaption (g, plot, getFxTypeInfo (FxType::shifter).params[fxp::shifter::shift].text (shift), active);
}

void EffectView::setEqGesture (int band, bool begin)
{
    const auto& b = eqBands()[(size_t) band];

    for (int param : { b.frequencyParam, b.gainParam })
    {
        if (auto* parameter = processor.fxController.getParameter (slot, param))
        {
            if (begin)
                parameter->beginChangeGesture();
            else
                parameter->endChangeGesture();
        }
    }
}

void EffectView::mouseMove (const juce::MouseEvent& e)
{
    const int band = eqBandAt (e.position);
    if (band != hoverBand)
    {
        hoverBand = band;
        refresh();
    }

    setMouseCursor (band >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void EffectView::mouseExit (const juce::MouseEvent&)
{
    hoverBand = -1;
    refresh();
}

void EffectView::mouseDown (const juce::MouseEvent& e)
{
    dragBand = eqBandAt (e.position);
    if (dragBand >= 0)
        setEqGesture (dragBand, true);
}

void EffectView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0)
        return;

    // По горизонтали - частота, по вертикали - усиление
    const auto plot = plotArea();
    const auto& b = eqBands()[(size_t) dragBand];
    const float gain = kEqRangeDb - (e.position.y - plot.getY()) / plot.getHeight() * 2.0f * kEqRangeDb;

    processor.fxController.setRealValue (slot, b.frequencyParam, frequencyForX (plot, e.position.x));
    processor.fxController.setRealValue (slot, b.gainParam, gain);
    refresh();
}

void EffectView::mouseUp (const juce::MouseEvent&)
{
    if (dragBand >= 0)
        setEqGesture (dragBand, false);

    dragBand = -1;
}

void EffectView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Колесо над точкой параметрической полосы меняет добротность
    const int band = eqBandAt (e.position);
    if (band < 0 || eqBands()[(size_t) band].qParam < 0)
        return;

    const int qParam = eqBands()[(size_t) band].qParam;
    const float q = value (qParam) * std::exp2 (wheel.deltaY * 1.5f);

    if (auto* parameter = processor.fxController.getParameter (slot, qParam))
    {
        parameter->beginChangeGesture();
        processor.fxController.setRealValue (slot, qParam, q);
        parameter->endChangeGesture();
    }

    repaint();
}

} // namespace sonder::ui
