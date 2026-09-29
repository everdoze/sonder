#pragma once

#include <array>
#include <atomic>

namespace sonder
{

// Кольцевой буфер выходного сигнала для осциллографа: пишет аудиопоток, читает UI.
// Relaxed-атомики на x86 бесплатны и избавляют от гонки данных.
class ScopeBuffer
{
public:
    static constexpr int kSize = 8192;

    void push (const float* left, const float* right, int numSamples) noexcept
    {
        int position = writePosition.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            samples[(size_t) position].store (0.5f * (left[i] + right[i]), std::memory_order_relaxed);
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

    std::atomic<float> sampleRate { 44100.0f };
    std::atomic<float> noteFrequency { 0.0f };

private:
    std::array<std::atomic<float>, kSize> samples {};
    std::atomic<int> writePosition { 0 };
};

} // namespace sonder
