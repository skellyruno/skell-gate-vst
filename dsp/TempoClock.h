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
    void prepare(double sr) {
        sampleRate = std::isfinite(sr) && sr > 0.0 ? sr : 48000.0;
        runningSamples = 0; lastSliceIndex = INT64_MIN;
        updateDerived();
    }
    void setHost(double bpmValue, int numeratorValue, int denominatorValue, double ppq, bool available,
                 bool syncEnabled, int division, double freeLengthMs) {
        // Hosts may report invalid transport values during stop/seek or startup.
        // Never let NaN/Inf reach floor() or the integer slice index conversion.
        bpm = std::isfinite(bpmValue) ? std::clamp(bpmValue, 20.0, 400.0) : 120.0;
        numerator = std::clamp(numeratorValue, 1, 32);
        denominator = std::clamp(denominatorValue, 1, 32);
        hostAvailable = available && std::isfinite(ppq);
        startPPQ = hostAvailable ? ppq : 0.0;
        sync = syncEnabled;
        selectedDivision = std::clamp(division, 0, 8);
        freeMs = std::isfinite(freeLengthMs) ? std::clamp(freeLengthMs, 10.0, 8000.0) : 500.0;
        updateDerived();
    }
    double quarterNotesPerSlice() const { return cachedSliceQuarters; }
    double getSamplesPerSlice() const { return cachedSamplesPerSlice; }
    double samplesPerQuarter() const { return cachedSamplesPerQuarter; }
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
    // Recalculate once per host block / MIDI mode change, not per sample.
    void updateDerived() {
        static constexpr double beats[] = {0.25, 1.0 / 3.0, 0.5, 1.0, 2.0};
        const double bar = static_cast<double>(numerator) * 4.0 / denominator;
        cachedSliceQuarters = selectedDivision < 5
            ? beats[selectedDivision] : bar * static_cast<double>(1 << (selectedDivision - 5));
        cachedSamplesPerQuarter = sampleRate * 60.0 / bpm;
        cachedSamplesPerSlice = sync ? cachedSliceQuarters * cachedSamplesPerQuarter
                                     : freeMs * .001 * sampleRate;
    }
    double sampleRate = 48000.0, bpm = 120.0, startPPQ = 0.0, freeMs = 500.0;
    double cachedSliceQuarters = 4.0, cachedSamplesPerQuarter = 24000.0,
           cachedSamplesPerSlice = 96000.0;
    int numerator = 4, denominator = 4, selectedDivision = 5;
    int64_t runningSamples = 0, lastSliceIndex = INT64_MIN;
    bool sync = true, hostAvailable = false;
};
}
