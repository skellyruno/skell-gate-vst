#pragma once
#include <algorithm>
#include <cmath>

namespace skell::dsp {
// Gate coefficients are updated only when attack/release change, never every sample.
class GateShaper {
public:
    void prepare(double sr) {
        sampleRate = std::max(sr, 1.0);
        current = 1.f;
        lastAttackMs = lastReleaseMs = -1.f;
        setTimes(3.f, 15.f);
    }
    void reset(float value = 1.f) { current = value; }
    void setTimes(float attackMs, float releaseMs) {
        if (attackMs != lastAttackMs) {
            lastAttackMs = attackMs;
            attackSamples = std::max(1.f, attackMs * .001f * static_cast<float>(sampleRate));
            attackAlpha = 1.f - std::exp(-4.60517f / attackSamples);
        }
        if (releaseMs != lastReleaseMs) {
            lastReleaseMs = releaseMs;
            releaseSamples = std::max(1.f, releaseMs * .001f * static_cast<float>(sampleRate));
            releaseAlpha = 1.f - std::exp(-4.60517f / releaseSamples);
        }
    }
    float next(float target, int curve) {
        const bool opening = target > current;
        const float count = opening ? attackSamples : releaseSamples;
        const float alpha = opening ? attackAlpha : releaseAlpha;
        float factor = alpha;
        if (curve == 0) factor = std::min(1.f, 1.f / count);
        else if (curve == 2) factor = std::min(1.f, alpha * (1.f + 1.5f * std::abs(target - current)));
        current += (target - current) * factor;
        return current;
    }
    // Retain the old interface for isolated DSP tests and external integrations.
    float next(float target, float attackMs, float releaseMs, int curve) {
        setTimes(attackMs, releaseMs);
        return next(target, curve);
    }
private:
    double sampleRate = 48000.0;
    float current = 1.f;
    float lastAttackMs = -1.f, lastReleaseMs = -1.f;
    float attackSamples = 1.f, releaseSamples = 1.f;
    float attackAlpha = 1.f, releaseAlpha = 1.f;
};
}
