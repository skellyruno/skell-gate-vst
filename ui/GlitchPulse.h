#pragma once
// Visual-only, deterministic xorshift sequencer for rare electronic glitches.
// Tick from the JUCE message thread at 30 Hz. No audio callbacks, threads,
// heap allocations, timers or state serialization are involved.
#include <cstdint>

class GlitchPulse final {
public:
    explicit GlitchPulse(std::uint32_t seed = 0x5E11C07u)
        : state(seed ? seed : 0x5E11C07u) {
        quietFrames = 120 + static_cast<int>(next() % 151u);
    }
    void tick() noexcept {
        if (remaining > 0) {
            --remaining;
            ++phase;
            return;
        }
        if (--quietFrames > 0) return;
        seedForBurst = next();
        remaining = 3 + static_cast<int>(next() % 3u); // 100-167 ms at 30 Hz
        phase = 0;
        quietFrames = 165 + static_cast<int>(next() % 196u); // 5.5-12 s
        ++burstCount;
    }
    int intensity() const noexcept {
        return remaining > 0 ? 1 + static_cast<int>((seedForBurst + phase) % 3u) : 0;
    }
    std::uint32_t visualSeed() const noexcept {
        return seedForBurst ^ (0x9e3779b9u * static_cast<std::uint32_t>(phase + 1));
    }
    std::uint32_t bursts() const noexcept { return burstCount; }
private:
    std::uint32_t next() noexcept {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    std::uint32_t state = 0, seedForBurst = 0, burstCount = 0;
    int quietFrames = 120, remaining = 0, phase = 0;
};
