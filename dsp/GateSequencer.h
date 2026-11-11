#pragma once
#include "GlitchRandomizer.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace skell::dsp {
class GateSequencer {
public:
    enum State { Off, On, Accent, Reverse };
    void setStep(int index, int state, float probability) {
        if (index < 0 || index >= 32) return;
        states[static_cast<size_t>(index)] = std::clamp(state, 0, 3);
        probabilities[static_cast<size_t>(index)] = std::clamp(probability, 0.f, 100.f);
    }
    int stepAt(double cyclePhase, int count, double swing) const {
        count = std::clamp(count, 1, 32);
        const auto wrapped = cyclePhase - std::floor(cyclePhase);
        const double scaled = wrapped * count;
        int i = std::clamp(static_cast<int>(std::floor(scaled)), 0, count - 1);
        if ((i & 1) && scaled - i < std::clamp(swing, 0.0, .75) * 0.5) --i;
        return i;
    }
    struct Decision {
        float gain = 1.f;
        bool reverse = false;
    };
    // Resolve the probability just once per step and share the decision between
    // gate level and the reverse engine. Reverse must not depend on gate depth:
    // the old gain > closedGain check accidentally suppressed reverse steps
    // when the closed gain was close to or equal to unity.
    Decision evaluate(int index, int64_t cycle, int seed, float closedGain) const {
        index = std::clamp(index, 0, 31);
        const auto i = static_cast<size_t>(index);
        const int state = states[i];
        const bool enabled = GlitchRandomizer::stepEnabled(seed, cycle, index, probabilities[i]);
        if (!enabled || state == Off) return {closedGain, false};
        if (state == Accent) return {1.15f, false};
        return {1.f, state == Reverse};
    }
    float target(int index, int64_t cycle, int seed, float closedGain) const {
        return evaluate(index, cycle, seed, closedGain).gain;
    }
    bool isReverseStep(int index) const { return states[static_cast<size_t>(std::clamp(index, 0, 31))] == Reverse; }
private:
    std::array<int, 32> states{};
    std::array<float, 32> probabilities = [] {
        std::array<float, 32> v{}; v.fill(100.0f); return v;
    }();
};
}
