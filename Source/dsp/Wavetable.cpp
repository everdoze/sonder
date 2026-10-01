#include "Wavetable.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

namespace sonder
{

namespace
{
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    int levelSize (int level)
    {
        // Таблица должна вмещать гармоники с запасом для линейной интерполяции
        const int harmonics = 1024 >> level;
        return juce::jlimit (64, Wavetable::kFrameSize, juce::nextPowerOfTwo (harmonics * 4));
    }
}

std::shared_ptr<const Wavetable> Wavetable::create (const juce::String& tableName, const std::vector<std::vector<float>>& frames)
{
    if (frames.empty())
        return nullptr;

    std::shared_ptr<Wavetable> table (new Wavetable());
    table->name = tableName;
    table->numFrames = juce::jmin ((int) frames.size(), kMaxFrames);

    for (int k = 0; k < kNumLevels; ++k)
    {
        table->levels[(size_t) k].size = levelSize (k);
        table->levels[(size_t) k].data.assign ((size_t) (table->numFrames * levelSize (k)), 0.0f);
    }

    juce::dsp::FFT fft (11); // 2048
    std::vector<float> spectrum ((size_t) kFrameSize * 2), work ((size_t) kFrameSize * 2);

    for (int f = 0; f < table->numFrames; ++f)
    {
        const auto& source = frames[(size_t) f];
        std::fill (spectrum.begin(), spectrum.end(), 0.0f);
        std::copy_n (source.begin(), juce::jmin ((int) source.size(), kFrameSize), spectrum.begin());

        fft.performRealOnlyForwardTransform (spectrum.data());
        spectrum[0] = spectrum[1] = 0.0f; // убираем постоянную составляющую

        for (int k = 0; k < kNumLevels; ++k)
        {
            const int maxHarmonic = 1024 >> k;
            work = spectrum;

            // Обнуляем гармоники выше предела (спектр хранится как N комплексных чисел)
            for (int bin = maxHarmonic + 1; bin < kFrameSize - maxHarmonic; ++bin)
                work[(size_t) bin * 2] = work[(size_t) bin * 2 + 1] = 0.0f;

            fft.performRealOnlyInverseTransform (work.data());

            auto& level = table->levels[(size_t) k];
            const int step = kFrameSize / level.size;
            float* destination = level.data.data() + (size_t) f * (size_t) level.size;

            for (int i = 0; i < level.size; ++i)
                destination[i] = work[(size_t) (i * step)];
        }
    }

    // Нормируем по пику полной версии
    float peak = 0.0f;
    for (float v : table->levels[0].data)
        peak = juce::jmax (peak, std::abs (v));

    if (peak > 1.0e-6f)
        for (auto& level : table->levels)
            for (auto& v : level.data)
                v /= peak;

    return table;
}

std::shared_ptr<const Wavetable> Wavetable::createHarmonic (const juce::String& tableName,
                                                           const std::function<std::pair<double, double> (int)>& amplitude)
{
    std::shared_ptr<Wavetable> table (new Wavetable());
    table->name = tableName;
    table->numFrames = 1;

    // Синус и косинус по целым фазам: k * n по модулю размера - точные значения без накопления ошибки
    constexpr int size = kClassicSize;
    std::vector<double> sine ((size_t) size);
    for (int n = 0; n < size; ++n)
        sine[(size_t) n] = std::sin (2.0 * juce::MathConstants<double>::pi * n / size);

    std::vector<std::pair<double, double>> amplitudes (1025);
    for (int k = 1; k <= 1024; ++k)
        amplitudes[(size_t) k] = amplitude (k);

    for (int level = 0; level < kNumLevels; ++level)
    {
        const int maxHarmonic = 1024 >> level;
        auto& destination = table->levels[(size_t) level];
        destination.size = size;
        destination.data.assign ((size_t) size, 0.0f);

        for (int n = 0; n < size; ++n)
        {
            double value = 0.0;
            for (int k = 1; k <= maxHarmonic; ++k)
            {
                const auto [sinAmplitude, cosAmplitude] = amplitudes[(size_t) k];
                if (sinAmplitude == 0.0 && cosAmplitude == 0.0)
                    continue;

                const int index = (k * n) & (size - 1);
                value += sinAmplitude * sine[(size_t) index] + cosAmplitude * sine[(size_t) ((index + size / 4) & (size - 1))];
            }

            destination.data[(size_t) n] = (float) value;
        }
    }

    return table;
}

Wavetable::MipSelection Wavetable::selectMip (float frequencyOverSampleRate) noexcept
{
    // Уровень k хранит 1024 >> k гармоник; безопасен тот, где все они ниже Найквиста: k = ceil (log2 (2048 * f / fs)).
    // Внутри октавы плавно переходим к следующему уровню, и на границе переключение уже не слышно.
    const float x = 2048.0f * frequencyOverSampleRate;
    if (x <= 0.5f)
        return {};

    const float logX = std::log2 (x);
    int level = (int) std::ceil (logX);
    float blend = 0.0f;

    if (level <= 0)
    {
        level = 0;
        blend = juce::jlimit (0.0f, 1.0f, logX + 1.0f);
    }
    else
    {
        blend = logX - (float) (level - 1);
    }

    if (level >= kNumLevels - 1)
        return { kNumLevels - 1, 0.0f };

    return { level, blend };
}

const float* Wavetable::getFrame (int frame) const noexcept
{
    return levels[0].data.data() + (size_t) juce::jlimit (0, numFrames - 1, frame) * (size_t) levels[0].size;
}

//==============================================================================
namespace
{
    constexpr int kBuiltInFrames = 64;
    constexpr int N = Wavetable::kFrameSize;
    constexpr int kMaxPartials = 512;

    using Frames = std::vector<std::vector<float>>;

    // Таблица, заданная формулой во времени: y (t, morph)
    Frames render (const std::function<float (float t, float morph)>& function)
    {
        Frames frames ((size_t) kBuiltInFrames, std::vector<float> ((size_t) N));

        for (int f = 0; f < kBuiltInFrames; ++f)
        {
            const float morph = (float) f / (float) (kBuiltInFrames - 1);
            for (int i = 0; i < N; ++i)
                frames[(size_t) f][(size_t) i] = function ((float) i / (float) N, morph);
        }

        return frames;
    }

    struct Partial
    {
        float amplitude = 0.0f, phase = 0.0f; // фаза в радианах
    };

    using Spectrum = std::array<Partial, kMaxPartials + 1>; // индекс - номер гармоники

    // Таблица, заданная спектром: кадр собирается обратным БПФ (на порядки быстрее суммы синусов)
    Frames renderAdditive (const std::function<void (float morph, Spectrum& partials)>& function)
    {
        juce::dsp::FFT fft (11);
        Frames frames ((size_t) kBuiltInFrames, std::vector<float> ((size_t) N));
        std::vector<float> bins ((size_t) N * 2);
        auto spectrum = std::make_unique<Spectrum>();

        for (int f = 0; f < kBuiltInFrames; ++f)
        {
            *spectrum = {};
            function ((float) f / (float) (kBuiltInFrames - 1), *spectrum);

            // a * sin (theta + phi)  <=>  Re = a * sin (phi), Im = -a * cos (phi)
            std::fill (bins.begin(), bins.end(), 0.0f);
            for (int h = 1; h <= kMaxPartials; ++h)
            {
                const auto& partial = (*spectrum)[(size_t) h];
                bins[(size_t) h * 2] = partial.amplitude * std::sin (partial.phase);
                bins[(size_t) h * 2 + 1] = -partial.amplitude * std::cos (partial.phase);
            }

            fft.performRealOnlyInverseTransform (bins.data());
            std::copy_n (bins.begin(), N, frames[(size_t) f].begin());
        }

        return frames;
    }

    // Детерминированное "случайное" число в [0, 1): таблицы одинаковы от запуска к запуску
    float hash01 (uint32_t a, uint32_t b = 0)
    {
        uint32_t h = a * 0x9e3779b1u ^ (b * 0x85ebca77u + 0x165667b1u);
        h ^= h >> 15;
        h *= 0x2c1b3c6du;
        h ^= h >> 12;
        h *= 0x297a2d39u;
        h ^= h >> 15;
        return (float) (h >> 8) / 16777216.0f;
    }

    float sine (float t)     { return std::sin (twoPi * t); }
    float triangle (float t) { return t < 0.25f ? 4.0f * t : (t < 0.75f ? 2.0f - 4.0f * t : 4.0f * t - 4.0f); }
    float saw (float t)      { return t < 0.5f ? 2.0f * t : 2.0f * t - 2.0f; }
    float square (float t)   { return t < 0.5f ? 1.0f : -1.0f; }

    // Форманты гласных (Peterson & Barney): частоты и относительные уровни
    struct Formants { float freq[3]; float gain[3]; };
    constexpr Formants vowelTable[5] {
        { { 730.0f, 1090.0f, 2440.0f }, { 1.0f, 0.50f, 0.25f } }, // A
        { { 530.0f, 1840.0f, 2480.0f }, { 1.0f, 0.40f, 0.30f } }, // E
        { { 270.0f, 2290.0f, 3010.0f }, { 1.0f, 0.30f, 0.20f } }, // I
        { { 570.0f,  840.0f, 2410.0f }, { 1.0f, 0.45f, 0.20f } }, // O
        { { 300.0f,  870.0f, 2240.0f }, { 1.0f, 0.35f, 0.15f } }, // U
    };

    // Гармоники 110 Гц, амплитуды повторяют формантную огибающую гласной A-E-I-O-U
    void vocalSpectrum (float morph, Spectrum& partials)
    {
        constexpr float f0 = 110.0f;
        const float position = morph * 4.0f;
        const int v0 = juce::jmin (3, (int) position);
        const float a = position - (float) v0;

        for (int h = 1; h <= 180; ++h)
        {
            const float frequency = f0 * (float) h;
            float amplitude = 0.0f;

            for (int formant = 0; formant < 3; ++formant)
            {
                const float centre = std::exp (std::log (vowelTable[v0].freq[formant]) * (1.0f - a)
                                               + std::log (vowelTable[v0 + 1].freq[formant]) * a);
                const float gain = vowelTable[v0].gain[formant] * (1.0f - a) + vowelTable[v0 + 1].gain[formant] * a;
                const float bandwidth = 60.0f + 0.06f * centre;
                const float x = (frequency - centre) / bandwidth;
                amplitude += gain / (1.0f + x * x);
            }

            partials[(size_t) h].amplitude = amplitude + 0.02f / (float) h;
        }
    }
}

const Wavetable& Wavetables::classicSaw()
{
    // 2t - 1 = -(2 / pi) * sum sin (2 pi k t) / k
    static const auto table = Wavetable::createHarmonic ("Saw", [] (int k)
    {
        return std::pair { -2.0 / (juce::MathConstants<double>::pi * k), 0.0 };
    });
    return *table;
}

const Wavetable& Wavetables::classicTriangle()
{
    // 1 - 4 |t - 0.5| = -(8 / pi^2) * sum cos (2 pi k t) / k^2 по нечётным k
    static const auto table = Wavetable::createHarmonic ("Triangle", [] (int k)
    {
        const double pi = juce::MathConstants<double>::pi;
        return std::pair { 0.0, k % 2 == 1 ? -8.0 / (pi * pi * k * k) : 0.0 };
    });
    return *table;
}

const Wavetable& Wavetables::classicSine()
{
    static const auto table = Wavetable::createHarmonic ("Sine", [] (int k)
    {
        return std::pair { k == 1 ? 1.0 : 0.0, 0.0 };
    });
    return *table;
}

const juce::StringArray& Wavetables::builtInNames()
{
    static const juce::StringArray names { "Basic Shapes", "PWM", "Harmonic Sweep", "Vocal", "FM Growl", "Sync", "Fold",
                                           "Analog", "Organ", "Digital", "Resonant", "Metallic", "Formant Sweep",
                                           "Spectral Noise", "Crush" };
    return names;
}

std::shared_ptr<const Wavetable> Wavetables::createBuiltIn (const juce::String& name)
{
    Frames frames;

    if (name == "Basic Shapes")
    {
        // синус -> треугольник -> пила -> меандр
        frames = render ([] (float t, float morph)
        {
            const float position = morph * 3.0f;
            const int segment = juce::jmin (2, (int) position);
            const float a = position - (float) segment;
            const float from = segment == 0 ? sine (t) : (segment == 1 ? triangle (t) : saw (t));
            const float to = segment == 0 ? triangle (t) : (segment == 1 ? saw (t) : square (t));
            return from + (to - from) * a;
        });
    }
    else if (name == "PWM")
    {
        frames = render ([] (float t, float morph) { return t < 0.5f - 0.47f * morph ? 1.0f : -1.0f; });
    }
    else if (name == "Harmonic Sweep")
    {
        // Гармоники добавляются по одной, последняя плавно "въезжает"
        frames = renderAdditive ([] (float morph, Spectrum& partials)
        {
            const float count = 1.0f + morph * 63.0f;
            for (int h = 1; h <= 64; ++h)
                partials[(size_t) h].amplitude = juce::jlimit (0.0f, 1.0f, count - (float) (h - 1)) / (float) h;
        });
    }
    else if (name == "Vocal")
    {
        frames = renderAdditive (vocalSpectrum);
    }
    else if (name == "FM Growl")
    {
        frames = render ([] (float t, float morph) { return std::sin (twoPi * t + morph * 6.0f * std::sin (twoPi * 2.0f * t)); });
    }
    else if (name == "Sync")
    {
        frames = render ([] (float t, float morph)
        {
            const float ratio = 1.0f + morph * 7.0f;
            const float slave = t * ratio - std::floor (t * ratio);
            return 2.0f * slave - 1.0f;
        });
    }
    else if (name == "Fold")
    {
        frames = render ([] (float t, float morph)
        {
            return std::sin (juce::MathConstants<float>::halfPi * (1.0f + morph * 7.0f) * std::sin (twoPi * t));
        });
    }
    else if (name == "Analog")
    {
        // "Неидеальная" аналоговая пила, переходящая в меандр: завал верхов и небольшой разброс фаз гармоник
        frames = renderAdditive ([] (float morph, Spectrum& partials)
        {
            for (int h = 1; h <= 300; ++h)
            {
                const float even = h % 2 == 0 ? 1.0f - morph : 1.0f;
                const float rolloff = 1.0f / std::sqrt (1.0f + (float) (h * h) / (48.0f * 48.0f));
                partials[(size_t) h] = { even * rolloff / (float) h, 0.25f * std::sin ((float) h * 1.7f) };
            }
        });
    }
    else if (name == "Organ")
    {
        // Регистры электрооргана выдвигаются один за другим
        frames = renderAdditive ([] (float morph, Spectrum& partials)
        {
            static constexpr int drawbars[] { 1, 2, 3, 4, 6, 8, 10, 12, 16 };
            static constexpr float levels[] { 1.0f, 0.8f, 0.7f, 0.55f, 0.45f, 0.4f, 0.3f, 0.25f, 0.2f };

            for (int k = 0; k < 9; ++k)
            {
                const float weight = k == 0 ? 1.0f : juce::jlimit (0.0f, 1.0f, morph * 8.0f - (float) (k - 1));
                // Фазы регистров разные, как у независимых колёс: иначе пики складываются в острые всплески
                partials[(size_t) drawbars[k]] = { levels[k] * weight, twoPi * hash01 ((uint32_t) k, 11u) };
            }
        });
    }
    else if (name == "Digital")
    {
        // В духе ранних цифровых синтов: восемь "случайных" спектров, между которыми идёт морф
        frames = renderAdditive ([] (float morph, Spectrum& partials)
        {
            const float position = morph * 7.0f;
            const int key = juce::jmin (6, (int) position);
            const float a = position - (float) key;
            const auto amplitude = [] (int h, int spectrum)
            {
                const float r = hash01 ((uint32_t) h, (uint32_t) spectrum + 17u);
                return r < 0.45f ? 0.0f : r * r / std::sqrt ((float) h);
            };

            partials[1].amplitude = 1.0f;
            for (int h = 2; h <= 40; ++h)
                partials[(size_t) h].amplitude = amplitude (h, key) * (1.0f - a) + amplitude (h, key + 1) * a;
        });
    }
    else if (name == "Resonant")
    {
        // Пила с резонансным пиком, который едет вверх, как срез фильтра с высоким резонансом
        frames = renderAdditive ([] (float morph, Spectrum& partials)
        {
            const float centre = 1.0f + morph * 40.0f;
            const float width = 1.5f + 0.05f * centre;
            for (int h = 1; h <= 160; ++h)
            {
                const float x = ((float) h - centre) / width;
                partials[(size_t) h].amplitude = (1.0f + 8.0f / (1.0f + x * x)) / (float) h;
            }
        });
    }
    else if (name == "Metallic")
    {
        // Обертоны "расползаются" от гармонического ряда, как у жёсткой струны или колокола
        frames = renderAdditive ([] (float morph, Spectrum& partials)
        {
            const float stiffness = 0.0005f + morph * 0.03f;
            for (int k = 1; k <= 28; ++k)
            {
                const int h = juce::roundToInt ((float) k * std::sqrt (1.0f + stiffness * (float) (k * k)));
                if (h > kMaxPartials)
                    break;

                partials[(size_t) h].amplitude += std::pow ((float) k, -0.8f);
                partials[(size_t) h].phase = twoPi * hash01 ((uint32_t) k, 5u);
            }
        });
    }
    else if (name == "Formant Sweep")
    {
        // Одна форманта, проезжающая по гармоникам: эффект "ток-бокса"
        frames = renderAdditive ([] (float morph, Spectrum& partials)
        {
            const float centre = 1.5f + morph * 30.0f;
            const float width = 1.2f + 0.12f * centre;
            for (int h = 1; h <= 120; ++h)
            {
                const float x = ((float) h - centre) / width;
                partials[(size_t) h].amplitude = std::exp (-x * x) + 0.25f / (float) h;
            }
        });
    }
    else if (name == "Spectral Noise")
    {
        // Плотный спектр со случайными фазами: от гула к шипящему "жужжанию"
        frames = renderAdditive ([] (float morph, Spectrum& partials)
        {
            const float count = 6.0f + morph * 250.0f;
            for (int h = 1; h <= 256; ++h)
            {
                const float fade = juce::jlimit (0.0f, 1.0f, count - (float) (h - 1));
                partials[(size_t) h] = { fade * (0.3f + 0.7f * hash01 ((uint32_t) h, 3u)) / std::sqrt ((float) h),
                                         twoPi * hash01 ((uint32_t) h, 9u) };
            }
        });
    }
    else if (name == "Crush")
    {
        // Синус, у которого падают разрядность и частота дискретизации
        frames = render ([] (float t, float morph)
        {
            const float steps = std::floor (256.0f * std::exp2 (-morph * 5.4f));
            const float levels = 2.0f + (1.0f - morph) * 30.0f;
            const float held = std::floor (t * steps) / steps;
            return std::round (std::sin (twoPi * held) * levels) / levels;
        });
    }
    else
    {
        return nullptr;
    }

    return Wavetable::create (name, frames);
}

std::shared_ptr<const Wavetable> Wavetables::loadFromFile (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 16)
        return nullptr;

    const int length = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) N * 1024);
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, length);
    reader->read (&buffer, 0, length, 0, true, true);

    // В моно
    std::vector<float> mono ((size_t) length, 0.0f);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < length; ++i)
            mono[(size_t) i] += buffer.getSample (ch, i) / (float) buffer.getNumChannels();

    // Размер кадра: Serum пишет по 2048; короткий файл считаем одиночным циклом
    int frameSize = N;
    if (length % N != 0)
    {
        if (length <= 8192)
            frameSize = length;
        else if (length % 1024 == 0)
            frameSize = 1024;
        else if (length % 256 == 0 && length / 256 <= Wavetable::kMaxFrames)
            frameSize = length / 256;
    }

    int numFrames = juce::jmax (1, length / frameSize);
    const int step = numFrames > Wavetable::kMaxFrames ? numFrames / Wavetable::kMaxFrames : 1;

    Frames frames;
    for (int f = 0; f < numFrames && (int) frames.size() < Wavetable::kMaxFrames; f += step)
    {
        // Пересэмплирование кадра в 2048 точек
        std::vector<float> frame ((size_t) N);
        const float* source = mono.data() + (size_t) f * (size_t) frameSize;

        for (int i = 0; i < N; ++i)
        {
            const float position = (float) i * (float) frameSize / (float) N;
            const int i0 = (int) position;
            const float frac = position - (float) i0;
            const float a = source[i0 % frameSize];
            const float b = source[(i0 + 1) % frameSize];
            frame[(size_t) i] = a + (b - a) * frac;
        }

        frames.push_back (std::move (frame));
    }

    return Wavetable::create (file.getFileNameWithoutExtension(), frames);
}

} // namespace sonder
