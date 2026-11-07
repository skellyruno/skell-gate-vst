#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace skell::dsp {
class SliceCapture {
public:
    // Ring capacity is fixed in prepare(), never resized in process().
    void prepare(int numberOfChannels, int maxSliceSamples) {
        channels = std::clamp(numberOfChannels, 1, 2);
        capacity = std::max(8192, maxSliceSamples * 2 + 4096);
        for (auto& b : data) b.assign(static_cast<size_t>(capacity), 0.0f);
        writeIndex = 0;
        segmentStart = 0;
        finishedStart = 0;
        finishedLength = 0;
    }
    void clear() {
        // Transport seeks must never zero megabytes of ring memory on the audio thread.
        // No old samples can be observed: readFinished() requires a completed slice,
        // and the first new completed slice only spans freshly written samples.
        writeIndex = 0; segmentStart = 0; finishedStart = 0; finishedLength = 0;
    }
    void onBoundary() {
        const int64_t length = writeIndex - segmentStart;
        if (length > 0 && length <= capacity / 2) {
            finishedStart = segmentStart;
            finishedLength = static_cast<int>(length);
        } else if (length > capacity / 2) finishedLength = 0;
        segmentStart = writeIndex;
    }
    void write(float a, float b) {
        if (capacity == 0) return;
        const size_t idx = static_cast<size_t>(writeIndex % capacity);
        data[0][idx] = a;
        data[1][idx] = b;
        ++writeIndex;
    }
    int length() const { return finishedLength; }
    float read(int channel, double position) const {
        if (!std::isfinite(position) || capacity == 0 || position < static_cast<double>(writeIndex - capacity + 3)
            || position > static_cast<double>(writeIndex - 3)) return 0.0f;
        const int64_t i = static_cast<int64_t>(std::floor(position));
        const float frac = static_cast<float>(position - static_cast<double>(i));
        const auto c = static_cast<size_t>(std::clamp(channel, 0, channels - 1));
        const auto sample = [&](int64_t n) { return data[c][static_cast<size_t>((n % capacity + capacity) % capacity)]; };
        // Four-point Hermite interpolation.
        const float x0 = sample(i - 1), x1 = sample(i), x2 = sample(i + 1), x3 = sample(i + 2);
        const float a = -0.5f * x0 + 1.5f * x1 - 1.5f * x2 + 0.5f * x3;
        const float b = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
        const float c1 = -0.5f * x0 + 0.5f * x2;
        return ((a * frac + b) * frac + c1) * frac + x1;
    }
    float readFinished(int channel, double fractionalIndex) const {
        if (finishedLength < 5) return 0.0f;
        return read(channel, finishedStart + std::clamp(fractionalIndex, 2.0, static_cast<double>(finishedLength - 3)));
    }
    int64_t totalWritten() const { return writeIndex; }
private:
    std::array<std::vector<float>, 2> data;
    int channels = 2, capacity = 0, finishedLength = 0;
    int64_t writeIndex = 0, segmentStart = 0, finishedStart = 0;
};
}
