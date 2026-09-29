#pragma once

#include <cstdint>

namespace sonder
{

// xorshift32: дешёвый генератор для шума прямо в аудиопотоке
class FastRandom
{
public:
    explicit FastRandom (uint32_t seed = 0x9e3779b9u) noexcept { setSeed (seed); }

    void setSeed (uint32_t seed) noexcept { state = seed != 0 ? seed : 0x9e3779b9u; }

    uint32_t nextUInt() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    // [0, 1)
    float nextFloat() noexcept { return (float) (nextUInt() >> 8) * (1.0f / 16777216.0f); }

    // [-1, 1)
    float nextBipolar() noexcept { return nextFloat() * 2.0f - 1.0f; }

private:
    uint32_t state;
};

} // namespace sonder
