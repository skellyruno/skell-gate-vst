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
        // Do not reserve an unused right-channel ring for mono hosts.
        for (int channel = 0; channel < 2; ++channel) {
            if (channel < channels) data[static_cast<size_t>(channel)].assign(static_cast<size_t>(capacity), 0.0f);
            else { data[static_cast<size_t>(channel)].clear(); data[static_cast<size_t>(channel)].shrink_to_fit(); }
        }
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
        if (channels > 1) data[1][idx] = b;
        ++writeIndex;
    }
    int length() const { return finishedLength; }
    float read(int channel, double position) const {
        if (!std::isfinite(position) || capacity == 0 || position < static_cast<double>(writeIndex - capacity + 3)
            || position > static_cast<double>(writeIndex - 3)) return 0.0f;
        const int64_t i = static_cast<int64_t>(std::floor(position));
        const float frac = static_cast<float>(position - static_cast<double>(i));
        const auto c = static_cast<size_t>(std::clamp(channel, 0, channels - 1));
        // A single modulo and short wrapped offsets replace four 64-bit
        // modulo operations on every Hermite read (hot reverse-grain path).
        int base = static_cast<int>(i % capacity);
        if (base < 0) base += capacity;
        const auto sample = [&](int offset) {
            int index = base + offset;
            if (index < 0) index += capacity;
            else if (index >= capacity) index -= capacity;
            return data[c][static_cast<size_t>(index)];
        };
        const float x0 = sample(-1), x1 = sample(0), x2 = sample(1), x3 = sample(2);
        const float a = -0.5f * x0 + 1.5f * x1 - 1.5f * x2 + 0.5f * x3;
        const float b = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
        const float c1 = -0.5f * x0 + 0.5f * x2;
        const float interpolated = ((a * frac + b) * frac + c1) * frac + x1;
        // Catmull-Rom Hermite can overshoot a full-scale transient by ~25%.
        // Bound each read by its four neighbouring samples to preserve peaks
        // without introducing a processing limiter or clipping the dry signal.
        const float minimum = std::min(std::min(x0, x1), std::min(x2, x3));
        const float maximum = std::max(std::max(x0, x1), std::max(x2, x3));
        return std::clamp(interpolated, minimum, maximum);
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
