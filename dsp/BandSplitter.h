#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace skell::dsp {
class Biquad {
public:
    void set(double freq, double sr, bool high) {
        const double omega = 2.0 * 3.14159265358979323846 * std::clamp(freq, 1.0, sr * .45) / sr;
        const double cosW = std::cos(omega), sinW = std::sin(omega);
        const double alpha = sinW * .7071067811865476; // Q = 1/sqrt(2)
        const double a0 = 1.0 + alpha;
        const double k = high ? (1.0 + cosW) * .5 : (1.0 - cosW) * .5;
        b0 = k / a0; b1 = (high ? -2.0 : 2.0) * k / a0; b2 = k / a0;
        a1 = -2.0 * cosW / a0; a2 = (1.0 - alpha) / a0;
    }
    float process(float input) {
        const double out = b0 * input + z1;
        z1 = b1 * input - a1 * out + z2;
        z2 = b2 * input - a2 * out;
        return static_cast<float>(out);
    }
    void reset() { z1 = z2 = 0.0; }
private:
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
};
class BandSplitter {
public:
    void prepare(double sr) {
        // Reject impossible/invalid host sample rates before filter design.
        sampleRate = std::isfinite(sr) && sr >= 8000.0 ? sr : 48000.0;
        oldLow = oldHigh = -1.0;
        for (auto& c : filters) for (auto& f : c) f.reset();
        setCuts(20, 20000);
    }
    void setCuts(double low, double high) {
        const double limit = std::max(20.0, sampleRate * .45);
        low = std::isfinite(low) ? std::clamp(low, 20.0, limit) : 20.0;
        high = std::isfinite(high) ? std::clamp(high, 20.0, limit)
                                  : std::min(20000.0, limit);
        if (high < low) std::swap(low, high);
        high = std::max(high, low + .00001);
        if (std::abs(low - oldLow) < .01 && std::abs(high - oldHigh) < .01) return;
        oldLow = low; oldHigh = high;
        lowActive = low > 20.01;
        highActive = high < std::min(19999.0, sampleRate * .45 - .01);
        for (auto& channel : filters) {
            channel[0].set(low, sampleRate, true);
            channel[1].set(low, sampleRate, true);
            channel[2].set(high, sampleRate, false);
            channel[3].set(high, sampleRate, false);
        }
    }
    void process(int ch, float input, float& inside, float& outside) {
        auto& f = filters[static_cast<size_t>(std::clamp(ch, 0, 1))];
        if (!std::isfinite(input)) input = 0.f;
        inside = input;
        if (lowActive) { inside = f[0].process(inside); inside = f[1].process(inside); }
        if (highActive) { inside = f[2].process(inside); inside = f[3].process(inside); }
        // Perfect-reconstruction complement: any processing-disabled split nulls exactly.
        outside = input - inside;
    }
private:
    double sampleRate = 48000, oldLow = -1, oldHigh = -1;
    bool lowActive = false, highActive = false;
    std::array<std::array<Biquad, 4>, 2> filters;
};
}
