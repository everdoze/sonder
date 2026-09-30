#pragma once

#include <algorithm>
#include <array>
#include <atomic>

namespace sonder
{

// Кольцевой буфер сэмплов: пишет аудиопоток, читает UI.
// Relaxed-атомики на x86 бесплатны и избавляют от гонки данных.
class SampleRing
{
public:
    static constexpr int kSize = 8192;

    void push (const float* data, int numSamples) noexcept
    {
        int position = writePosition.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            samples[(size_t) position].store (data[i], std::memory_order_relaxed);
            position = (position + 1) & (kSize - 1);
        }

        writePosition.store (position, std::memory_order_release);
    }

    // Копирует последние count сэмплов (count <= kSize), от старых к новым
    void copyLatest (float* destination, int count) const noexcept
    {
        const int end = writePosition.load (std::memory_order_acquire);
        int position = (end - count) & (kSize - 1);

        for (int i = 0; i < count; ++i)
        {
            destination[i] = samples[(size_t) position].load (std::memory_order_relaxed);
            position = (position + 1) & (kSize - 1);
        }
    }

private:
    std::array<std::atomic<float>, kSize> samples {};
    std::atomic<int> writePosition { 0 };
};

// Выходной сигнал для осциллографа: стерео-сэмплы (синхронная волна и вектороскоп)
// и огибающая "минимум-максимум" по коротким отрезкам для бегущей волны на длинном окне.
class ScopeBuffer
{
public:
    static constexpr int kSize = 8192;
    static constexpr int kPeakHop = 32;    // сэмплов на одну пару минимум-максимум
    static constexpr int kNumPeaks = 8192;

    void push (const float* left, const float* right, int numSamples) noexcept
    {
        int position = writePosition.load (std::memory_order_relaxed);
        int peakPosition = peakWritePosition.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            samplesLeft[(size_t) position].store (left[i], std::memory_order_relaxed);
            samplesRight[(size_t) position].store (right[i], std::memory_order_relaxed);
            position = (position + 1) & (kSize - 1);

            const float mono = 0.5f * (left[i] + right[i]);
            hopMin = hopCount == 0 ? mono : std::min (hopMin, mono);
            hopMax = hopCount == 0 ? mono : std::max (hopMax, mono);

            if (++hopCount >= kPeakHop)
            {
                peaksMin[(size_t) peakPosition].store (hopMin, std::memory_order_relaxed);
                peaksMax[(size_t) peakPosition].store (hopMax, std::memory_order_relaxed);
                peakPosition = (peakPosition + 1) & (kNumPeaks - 1);
                hopCount = 0;
            }
        }

        writePosition.store (position, std::memory_order_release);
        peakWritePosition.store (peakPosition, std::memory_order_release);
    }

    // Последние count сэмплов (count <= kSize), от старых к новым; моно - полусумма каналов
    void copyLatest (float* destination, int count) const noexcept
    {
        const int end = writePosition.load (std::memory_order_acquire);
        int position = (end - count) & (kSize - 1);

        for (int i = 0; i < count; ++i)
        {
            destination[i] = 0.5f * (samplesLeft[(size_t) position].load (std::memory_order_relaxed)
                                     + samplesRight[(size_t) position].load (std::memory_order_relaxed));
            position = (position + 1) & (kSize - 1);
        }
    }

    void copyLatestStereo (float* left, float* right, int count) const noexcept
    {
        const int end = writePosition.load (std::memory_order_acquire);
        int position = (end - count) & (kSize - 1);

        for (int i = 0; i < count; ++i)
        {
            left[i] = samplesLeft[(size_t) position].load (std::memory_order_relaxed);
            right[i] = samplesRight[(size_t) position].load (std::memory_order_relaxed);
            position = (position + 1) & (kSize - 1);
        }
    }

    // Последние count пар минимум-максимум (count <= kNumPeaks), каждая покрывает kPeakHop сэмплов
    void copyLatestPeaks (float* minimums, float* maximums, int count) const noexcept
    {
        const int end = peakWritePosition.load (std::memory_order_acquire);
        int position = (end - count) & (kNumPeaks - 1);

        for (int i = 0; i < count; ++i)
        {
            minimums[i] = peaksMin[(size_t) position].load (std::memory_order_relaxed);
            maximums[i] = peaksMax[(size_t) position].load (std::memory_order_relaxed);
            position = (position + 1) & (kNumPeaks - 1);
        }
    }

    std::atomic<float> sampleRate { 44100.0f };
    std::atomic<float> noteFrequency { 0.0f };

private:
    std::array<std::atomic<float>, kSize> samplesLeft {}, samplesRight {};
    std::array<std::atomic<float>, kNumPeaks> peaksMin {}, peaksMax {};
    std::atomic<int> writePosition { 0 }, peakWritePosition { 0 };

    // Только аудиопоток
    float hopMin = 0.0f, hopMax = 0.0f;
    int hopCount = 0;
};

} // namespace sonder
