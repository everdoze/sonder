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

int Wavetable::levelForFrequency (float frequencyOverSampleRate) noexcept
{
    // Уровень k хранит 1024 >> k гармоник; нужно, чтобы все они были ниже Найквиста
    const float x = 2048.0f * frequencyOverSampleRate;
    if (x <= 1.0f)
        return 0;

    return juce::jlimit (0, kNumLevels - 1, (int) std::ceil (std::log2 (x)));
}

float Wavetable::read (const Level& level, int frame, float phase) const noexcept
{
    const float position = phase * (float) level.size;
    const int i0 = (int) position;
    const float frac = position - (float) i0;
    const int mask = level.size - 1;
    const float* data = level.data.data() + (size_t) frame * (size_t) level.size;
    const float a = data[i0 & mask];
    const float b = data[(i0 + 1) & mask];
    return a + (b - a) * frac;
}

float Wavetable::sample (float phase, float position, int levelIndex) const noexcept
{
    const auto& level = levels[(size_t) levelIndex];
    const float framePosition = juce::jlimit (0.0f, 1.0f, position) * (float) (numFrames - 1);
    const int frame0 = (int) framePosition;
    const float frac = framePosition - (float) frame0;

    const float a = read (level, frame0, phase);
    if (frac <= 0.0f || frame0 + 1 >= numFrames)
        return a;

    const float b = read (level, frame0 + 1, phase);
    return a + (b - a) * frac;
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

    using FrameFunction = std::function<float (float t, float morph)>;

    std::vector<std::vector<float>> render (const FrameFunction& function)
    {
        std::vector<std::vector<float>> frames ((size_t) kBuiltInFrames, std::vector<float> ((size_t) N));

        for (int f = 0; f < kBuiltInFrames; ++f)
        {
            const float morph = (float) f / (float) (kBuiltInFrames - 1);
            for (int i = 0; i < N; ++i)
                frames[(size_t) f][(size_t) i] = function ((float) i / (float) N, morph);
        }

        return frames;
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

    std::vector<std::vector<float>> renderVocal()
    {
        // Аддитивно: гармоники 110 Гц, амплитуды повторяют формантную огибающую гласной
        constexpr float f0 = 110.0f;
        constexpr int harmonics = 180;
        std::vector<std::vector<float>> frames ((size_t) kBuiltInFrames, std::vector<float> ((size_t) N, 0.0f));

        for (int f = 0; f < kBuiltInFrames; ++f)
        {
            const float position = (float) f / (float) (kBuiltInFrames - 1) * 4.0f;
            const int v0 = juce::jmin (3, (int) position);
            const float a = position - (float) v0;

            std::array<float, harmonics + 1> amplitudes {};
            for (int h = 1; h <= harmonics; ++h)
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

                amplitudes[(size_t) h] = amplitude + 0.02f / (float) h;
            }

            for (int i = 0; i < N; ++i)
            {
                const float t = (float) i / (float) N;
                float sum = 0.0f;
                for (int h = 1; h <= harmonics; ++h)
                    sum += amplitudes[(size_t) h] * std::sin (twoPi * t * (float) h);

                frames[(size_t) f][(size_t) i] = sum;
            }
        }

        return frames;
    }
}

const juce::StringArray& Wavetables::builtInNames()
{
    static const juce::StringArray names { "Basic Shapes", "PWM", "Harmonic Sweep", "Vocal", "FM Growl", "Sync", "Fold" };
    return names;
}

std::shared_ptr<const Wavetable> Wavetables::createBuiltIn (const juce::String& name)
{
    std::vector<std::vector<float>> frames;

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
        frames = render ([] (float t, float morph)
        {
            const int count = 1 + juce::roundToInt (morph * 63.0f);
            float sum = 0.0f;
            for (int h = 1; h <= count; ++h)
                sum += std::sin (twoPi * t * (float) h) / (float) h;
            return sum;
        });
    }
    else if (name == "Vocal")
    {
        frames = renderVocal();
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

    std::vector<std::vector<float>> frames;
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
