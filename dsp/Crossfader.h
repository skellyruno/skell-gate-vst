#pragma once
#include <algorithm>
#include <cmath>

namespace skell::dsp {
struct Crossfader {
    struct Gains { float dry = 1.f, wet = 0.f; };
    static Gains equalPowerGains(float fraction) {
        constexpr float halfPi = 1.5707963267948966f;
        const float t = std::clamp(fraction, 0.f, 1.f);
        if (t <= 0.f) return {1.f, 0.f};
        if (t >= 1.f) return {0.f, 1.f};
        return {std::cos(t * halfPi), std::sin(t * halfPi)};
    }
    static float equalPower(float dry, float wet, float fraction) {
        const auto gains = equalPowerGains(fraction);
        return dry * gains.dry + wet * gains.wet;
    }
    // At a slice boundary, crossfade the last wet output sample into the
    // incoming slice; unlike a fade-to-zero this preserves perceived weight.
    // The end points are exact and the Hann easing has zero slope there.
    static float spliceHold(float outgoing, float incoming, float phase) {
        const float t = std::clamp(phase, 0.f, 1.f);
        const float eased = t * t * (3.f - 2.f * t);
        return outgoing + (incoming - outgoing) * eased;
    }
    static float raisedCosine(float x) {
        return .5f - .5f * std::cos(std::clamp(x, 0.f, 1.f) * 3.141592653589793f);
    }
    static float boundaryEnvelope(double phase, double sliceSamples, double fadeInSamples,
                                  double fadeOutSamples, double smoothingSamples) {
        const double into = phase * sliceSamples;
        const double left = std::max(fadeInSamples, smoothingSamples);
        const double right = std::max(fadeOutSamples, smoothingSamples);
        const auto a = left <= 0.0 ? 1.f : raisedCosine(static_cast<float>(into / left));
        const auto b = right <= 0.0 ? 1.f : raisedCosine(static_cast<float>((sliceSamples - into) / right));
        return a * b;
    }
};
}
