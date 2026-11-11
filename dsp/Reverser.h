#pragma once
#include "Crossfader.h"
#include "SliceCapture.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace skell::dsp {
class Reverser {
public:
    enum Style { Classic, Tape, PingPong, Stutter };
    struct PitchContext {
        double ratio = 1.0, grainLen = 1440.0;
        bool enabled = false;
    };
    Reverser() {
        // Built once off the audio thread, not once for every grain/sample.
        for (size_t i = 0; i < hann.size(); ++i)
            hann[i] = static_cast<float>(.5 - .5 * std::cos(
                6.28318530717958647692 * static_cast<double>(i) / hann.size()));
    }
    static PitchContext makePitchContext(float semitones, int sampleRate) {
        const double st = std::clamp(static_cast<double>(semitones), -12.0, 12.0);
        return {std::exp2(st / 12.0), std::clamp(std::max(sampleRate, 1) * .03, 64.0, 4096.0),
                std::abs(st) >= .001};
    }
    float sample(const SliceCapture& capture, int ch, double phase, int style,
                 int64_t sliceIndex, float semitones, int sampleRate,
                 double /*smoothFraction*/ = .0) const {
        return samplePrepared(capture, ch, phase, style, sliceIndex,
                              makePitchContext(semitones, sampleRate));
    }
    float samplePrepared(const SliceCapture& capture, int ch, double phase, int style,
                         int64_t sliceIndex, const PitchContext& pitch) const {
        const int length = capture.length();
        if (length < 5) return 0.f;
        double p = std::clamp(phase, 0.0, 1.0);
        const bool forward = style == PingPong && (sliceIndex & 1);
        // Ease-out tape transport: starts fast and decelerates toward the end.
        // The previous p*p curve instead accelerated throughout the slice.
        if (style == Tape) p = p * (2.0 - p);
        if (style == Stutter) p = std::fmod(p * 4.0, 1.0) * .25;
        const double readPosition = forward
            ? 2.0 + p * (length - 6)
            : (length - 3) - p * (length - 6);
        constexpr std::array<float, 4> stutterGains{1.f, .8f, .64f, .512f};
        const float gain = style == Stutter
            ? stutterGains[static_cast<size_t>(std::clamp(static_cast<int>(phase * 4.0), 0, 3))]
            : 1.f;
        if (!pitch.enabled) return gain * capture.readFinished(ch, readPosition);
        // Overlap two equal-length Hann grains. Normalisation prevents
        // amplitude modulation at grain joins; interpolation avoids table steps.
        const double period = pitch.grainLen;
        const double grainPhase = std::fmod(p * length, period);
        float sum = 0.f, weight = 0.f;
        for (int grain = 0; grain < 2; ++grain) {
            const double local = std::fmod(grainPhase + grain * period * .5, period);
            const float window = hannAt(local / period);
            const double offset = (pitch.ratio - 1.0) * (local - period * .5);
            // Ping-pong's forward half must honour pitch, too. The old early
            // return bypassed pitch-shifting on every odd slice.
            sum += window * capture.readFinished(ch, readPosition + (forward ? offset : -offset));
            weight += window;
        }
        return gain * sum / std::max(weight, .001f);
    }
private:
    float hannAt(double phase) const {
        const double index = std::clamp(phase, 0.0, 1.0) * hann.size();
        const size_t i = static_cast<size_t>(index) % hann.size();
        const size_t next = (i + 1) % hann.size();
        const float fraction = static_cast<float>(index - std::floor(index));
        return hann[i] + (hann[next] - hann[i]) * fraction;
    }
    std::array<float, 2048> hann{};
};
}
