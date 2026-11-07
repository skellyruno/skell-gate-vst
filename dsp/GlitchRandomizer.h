#pragma once
#include <algorithm>
#include <cstdint>

namespace skell::dsp {
class GlitchRandomizer {
public:
    enum Event { Normal, Flip, Skip, HalfTime, Stutter };
    static uint32_t hash(uint32_t x) {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15;
        x *= 0x846ca68bU; x ^= x >> 16; return x;
    }
    static float uniform(uint32_t seed, int64_t index, uint32_t salt = 0) {
        const uint64_t v = static_cast<uint64_t>(index);
        return (hash(seed ^ static_cast<uint32_t>(v) ^ hash(static_cast<uint32_t>(v >> 32) + salt)) >> 8) * (1.0f / 16777216.0f);
    }
    Event choose(int64_t sliceIndex, int seed, float chaosPercent, bool seedLock, int64_t barIndex) const {
        const auto key = static_cast<uint32_t>(seed) ^ (seedLock ? 0U : hash(static_cast<uint32_t>(barIndex)));
        const auto chance = uniform(key, sliceIndex, 0xabc3U);
        if (chance >= std::clamp(chaosPercent, 0.f, 100.f) / 100.f) return Normal;
        return static_cast<Event>(1 + std::min(3, static_cast<int>(uniform(key, sliceIndex, 0x33d4U) * 4)));
    }
    static bool stepEnabled(int seed, int64_t cycle, int step, float probability) {
        return uniform(static_cast<uint32_t>(seed), cycle * 37 + step, 0x1b22U) < std::clamp(probability, 0.f, 100.f) / 100.f;
    }
};
}
