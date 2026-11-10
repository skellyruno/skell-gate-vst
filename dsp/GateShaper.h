#pragma once
#include <algorithm>
#include <cmath>

namespace skell::dsp {
class GateShaper {
public:
    void prepare(double sr) {
        sampleRate = std::max(sr, 1.0);
        lastAttackMs = lastReleaseMs = -1.f;
        setTimes(3.f, 15.f);
        reset();
    }
    void reset(float value = 1.f) {
        current = linearStart = linearTarget = value;
        linearProgress = 0; linearDuration = 1; previousCurve = -1;
    }
    void setTimes(float attackMs, float releaseMs) {
        if (attackMs != lastAttackMs) {
            lastAttackMs = attackMs;
            attackSamples = std::max(1.f, attackMs * .001f * static_cast<float>(sampleRate));
            attackAlpha = 1.f - std::exp(-4.60517f / attackSamples);
            previousCurve = -1; // Recompute an ongoing linear ramp after automation.
        }
        if (releaseMs != lastReleaseMs) {
            lastReleaseMs = releaseMs;
            releaseSamples = std::max(1.f, releaseMs * .001f * static_cast<float>(sampleRate));
            releaseAlpha = 1.f - std::exp(-4.60517f / releaseSamples);
            previousCurve = -1;
        }
    }
    float next(float target, int curve) {
        if (curve == 0) {
            // A true linear ramp has a fixed step and reaches the target
            // after exactly ceil(attack/release samples), not asymptotically.
            if (previousCurve != 0 || target != linearTarget) {
                linearStart = current; linearTarget = target;
                // Round sample counts to nearest integer: e.g. 10 ms at 1 kHz
                // must be 10 samples, not 11 due to float precision noise.
                linearDuration = std::max(1, static_cast<int>(std::lround(
                    target >= current ? attackSamples : releaseSamples)));
                linearProgress = 0;
            }
            previousCurve = 0;
            if (linearProgress < linearDuration) ++linearProgress;
            const float t = static_cast<float>(linearProgress) / linearDuration;
            current = linearStart + (linearTarget - linearStart) * t;
            return current;
        }
        previousCurve = curve;
        const float alpha = target > current ? attackAlpha : releaseAlpha;
        float factor = alpha;
        if (curve == 2)
            factor = std::min(1.f, alpha * (1.f + 1.5f * std::abs(target - current)));
        current += (target - current) * factor;
        return current;
    }
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
    float linearStart = 1.f, linearTarget = 1.f;
    int linearProgress = 0, linearDuration = 1, previousCurve = -1;
};
}
