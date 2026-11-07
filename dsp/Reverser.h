#pragma once
#include "Crossfader.h"
#include "SliceCapture.h"
#include <algorithm>
#include <cmath>

namespace skell::dsp {
class Reverser {
public:
    enum Style { Classic, Tape, PingPong, Stutter };
    float sample(const SliceCapture& capture, int ch, double phase, int style,
                 int64_t sliceIndex, float semitones, int sampleRate, double smoothFraction = .0) const {
        const int length = capture.length();
        if (length < 5) return 0.0f;
        double p = std::clamp(phase, 0.0, 1.0);
        if (style == PingPong && (sliceIndex & 1)) return capture.readFinished(ch, p * (length - 1));
        if (style == Tape) p = p * p; // Reverse deck progressively slows near the end.
        if (style == Stutter) {
            constexpr double repeats = 4.0;
            p = std::fmod(p * repeats, 1.0) / 4.0;
        }
        const double start = (length - 3) - p * (length - 6);
        const float gain = style == Stutter ? static_cast<float>(std::pow(.80, std::floor(phase * 4.0))) : 1.0f;
        if (std::abs(semitones) < .001f) return gain * capture.readFinished(ch, start);
        // Two overlapping Hann grains: variable-rate reading with constant slice duration.
        const double ratio = std::pow(2.0, std::clamp(semitones, -12.f, 12.f) / 12.0);
        const double grainLen = std::clamp(sampleRate * .03, 64.0, 4096.0);
        const double progress = p * length;
        const double grainPhase = std::fmod(progress, grainLen);
        float result = 0.f, norm = 0.f;
        for (int g = 0; g < 2; ++g) {
            const double local = std::fmod(grainPhase + g * grainLen / 2, grainLen);
            const float win = static_cast<float>(.5 - .5 * std::cos(2.0 * 3.141592653589793 * local / grainLen));
            const double offset = (ratio - 1.0) * (local - grainLen / 2);
            result += win * capture.readFinished(ch, start - offset);
            norm += win;
        }
        return gain * result / std::max(norm, .001f);
    }
};
}
