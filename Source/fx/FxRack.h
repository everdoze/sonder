#pragma once

#include "FxTypes.h"

#include <array>
#include <atomic>

namespace sonder
{

// Состояние рэка эффектов: что стоит в каждом слоте и в каком порядке слоты обрабатываются.
// Меняется из интерфейса (message thread), читается аудиопотоком без блокировок.
// Автоматизация привязана к слоту, поэтому при перестановке эффекта она едет вместе с ним.
class FxRack
{
public:
    using Order = std::array<int, kNumFxSlots>; // перестановка номеров слотов

    FxRack()
    {
        for (auto& type : types)
            type.store ((int) FxType::none);
    }

    FxType getType (int slot) const noexcept
    {
        return static_cast<FxType> (types[(size_t) slot].load (std::memory_order_acquire));
    }

    void setType (int slot, FxType type) noexcept
    {
        types[(size_t) slot].store ((int) type, std::memory_order_release);
        ++version;
    }

    static Order defaultOrder() noexcept
    {
        Order order {};
        for (int i = 0; i < kNumFxSlots; ++i)
            order[(size_t) i] = i;

        return order;
    }

    Order getOrder() const noexcept
    {
        const uint64_t value = packedOrder.load (std::memory_order_acquire);
        Order order {};
        for (int i = 0; i < kNumFxSlots; ++i)
            order[(size_t) i] = (int) ((value >> (4 * i)) & 0xfu);

        return order;
    }

    // Принимает только корректную перестановку всех слотов
    bool setOrder (const Order& order) noexcept
    {
        uint32_t seen = 0;
        uint64_t value = 0;

        for (int i = 0; i < kNumFxSlots; ++i)
        {
            const int slot = order[(size_t) i];
            if (slot < 0 || slot >= kNumFxSlots)
                return false;

            seen |= 1u << slot;
            value |= (uint64_t) slot << (4 * i);
        }

        if (seen != (1u << kNumFxSlots) - 1u)
            return false;

        packedOrder.store (value, std::memory_order_release);
        ++version;
        return true;
    }

    // Счётчик изменений: интерфейс перестраивает рэк, когда он меняется
    int getVersion() const noexcept { return version.load(); }

private:
    static uint64_t identityOrder() noexcept
    {
        uint64_t value = 0;
        for (int i = 0; i < kNumFxSlots; ++i)
            value |= (uint64_t) i << (4 * i);

        return value;
    }

    std::array<std::atomic<int>, kNumFxSlots> types;
    std::atomic<uint64_t> packedOrder { identityOrder() };
    std::atomic<int> version { 0 };
};

} // namespace sonder
