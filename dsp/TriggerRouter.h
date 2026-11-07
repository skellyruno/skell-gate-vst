#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace skell::dsp {
class TriggerRouter {
public:
    enum Source { Auto, MIDI, Sidechain, Manual };
    void prepare(double sr) { sampleRate = sr; envelope = previous = 0.0f; hold = 0; activeFrames = 0; }
    void trigger(float velocity = 1.f, int midiNote = 60) { activeFrames = static_cast<int>(sampleRate * .50); lastVelocity = velocity; lastNote = midiNote; }
    bool processSidechain(float signal) {
        // Low-latency multi-stage onset energy proxy; not an FFT spectral flux analyzer.
        const float amplitude = std::abs(signal);
        envelope += (amplitude - envelope) * (amplitude > envelope ? .10f : .001f);
        const float difference = envelope - previous;
        previous = envelope;
        if (hold > 0) --hold;
        if (difference > 0.025f && hold == 0) {
            hold = static_cast<int>(sampleRate * .070);
            trigger(std::min(1.f, envelope * 4.f));
            return true;
        }
        return false;
    }
    void tick() { if (activeFrames > 0) --activeFrames; }
    bool active() const { return activeFrames > 0; }
    float velocity() const { return lastVelocity; }
    int note() const { return lastNote; }
private:
    double sampleRate = 48000.0;
    float envelope = 0.f, previous = 0.f, lastVelocity = 1.f;
    int hold = 0, activeFrames = 0, lastNote = 60;
};
}
