#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace skell::dsp {
struct ClockPosition {
    double beat = 0.0; // Quarter-note position on host timeline
    double phase = 0.0; // [0, 1) within slice
    int64_t sliceIndex = 0;
    bool boundary = false;
};
class TempoClock {
public:
    void prepare(double sr) { sampleRate = std::max(1.0, sr); runningSamples = 0; lastSliceIndex = INT64_MIN; }
    void setHost(double bpmValue, int numeratorValue, int denominatorValue, double ppq, bool available,
                 bool syncEnabled, int division, double freeLengthMs) {
        bpm = std::clamp(bpmValue, 20.0, 400.0);
        numerator = std::clamp(numeratorValue, 1, 32);
        denominator = std::clamp(denominatorValue, 1, 32);
        startPPQ = ppq;
        hostAvailable = available;
        sync = syncEnabled;
        selectedDivision = std::clamp(division, 0, 8);
        freeMs = std::clamp(freeLengthMs, 10.0, 8000.0);
    }
    double quarterNotesPerSlice() const {
        static constexpr double beats[] = {0.25, 1.0 / 3.0, 0.5, 1.0, 2.0};
        if (selectedDivision < 5) return beats[selectedDivision];
        const double bar = static_cast<double>(numerator) * 4.0 / denominator;
        return bar * static_cast<double>(1 << (selectedDivision - 5));
    }
    double getSamplesPerSlice() const {
        if (!sync) return freeMs * .001 * sampleRate;
        return quarterNotesPerSlice() * (60.0 / bpm) * sampleRate;
    }
    double samplesPerQuarter() const { return sampleRate * 60.0 / bpm; }
    double getBpm() const { return bpm; }
    double getSampleRate() const { return sampleRate; }
    double getBarLength() const { return static_cast<double>(numerator) * 4.0 / denominator; }
    ClockPosition atSample(int offset) const {
        const double len = std::max(1.0, getSamplesPerSlice());
        double raw;
        if (sync && hostAvailable) raw = (startPPQ + static_cast<double>(offset) / samplesPerQuarter()) / quarterNotesPerSlice();
        else raw = static_cast<double>(runningSamples + offset) / len;
        const double whole = std::floor(raw);
        return {sync && hostAvailable ? startPPQ + offset / samplesPerQuarter() :
                    (runningSamples + offset) / samplesPerQuarter(),
                std::clamp(raw - whole, 0.0, std::nextafter(1.0, 0.0)),
                static_cast<int64_t>(whole), false};
    }
    ClockPosition tick(int offset) {
        auto pos = atSample(offset);
        pos.boundary = pos.sliceIndex != lastSliceIndex;
        lastSliceIndex = pos.sliceIndex;
        return pos;
    }
    void advance(int blockSamples) { runningSamples += blockSamples; }
    void resetTracking() { lastSliceIndex = INT64_MIN; }
private:
    double sampleRate = 48000.0, bpm = 120.0, startPPQ = 0.0, freeMs = 500.0;
    int numerator = 4, denominator = 4, selectedDivision = 5;
    int64_t runningSamples = 0, lastSliceIndex = INT64_MIN;
    bool sync = true, hostAvailable = false;
};
}
