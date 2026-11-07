#pragma once
#include <algorithm>
#include <cmath>

namespace skell::dsp {
class GateShaper {
public:
    void prepare(double sr) { sampleRate = std::max(sr, 1.0); current = 1.f; }
    void reset(float v = 1.f) { current = v; }
    float next(float target, float attackMs, float releaseMs, int curve) {
        const float time = target > current ? attackMs : releaseMs;
        const float count = std::max(1.0f, time * .001f * static_cast<float>(sampleRate));
        const float alpha = 1.0f - std::exp(-4.60517f / count);
        float factor = alpha;
        if (curve == 0) factor = std::min(1.0f, 1.0f / count);
        else if (curve == 2) factor = std::min(1.0f, alpha * (1.0f + 1.5f * std::abs(target - current)));
        current += (target - current) * factor;
        return current;
    }
private:
    double sampleRate = 48000.0;
    float current = 1.f;
};
}
