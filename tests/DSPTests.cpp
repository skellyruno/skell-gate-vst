#include "dsp/TempoClock.h"
#include "dsp/SliceCapture.h"
#include "dsp/Reverser.h"
#include "dsp/GateSequencer.h"
#include "dsp/GateShaper.h"
#include "dsp/BandSplitter.h"
#include "dsp/Crossfader.h"
#include "dsp/TriggerRouter.h"
#include "dsp/GlitchRandomizer.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace skell::dsp;
static void check(bool passed, const char* reason) { if (!passed) throw std::runtime_error(reason); }
static void approx(double a, double b, double limit, const char* reason) { check(std::abs(a - b) <= limit, reason); }
static void clockTest() {
    TempoClock c; c.prepare(48000);
    c.setHost(120, 4, 4, 0, true, true, 5, 100);
    approx(c.getSamplesPerSlice(), 96000, .001, "1-bar 120BPM sample length");
    check(c.tick(0).boundary, "first boundary");
    check(!c.tick(1).boundary, "unexpected boundary");
    check(c.tick(96000).boundary, "boundary at bar");
    c.setHost(120, 4, 4, 0, true, true, 1, 100);
    approx(c.getSamplesPerSlice(), 8000, .0001, "triplet length");
    c.setHost(120, 4, 4, 0, false, false, 1, 10);
    approx(c.getSamplesPerSlice(), 480, .01, "free length");
    // Blocks of 64, 512, 4096 should generate the same host-locked gate indices.
    std::vector<int64_t> ref;
    for (int blockSize : {64, 512, 4096}) {
        TempoClock block; block.prepare(48000);
        std::vector<int64_t> boundaries;
        for (int offset = 0; offset < 150000; offset += blockSize) {
            const double ppq = offset / 24000.0;
            block.setHost(120, 4, 4, ppq, true, true, 2, 500);
            for (int i = 0; i < std::min(blockSize, 150000 - offset); ++i)
                if (block.tick(i).boundary) boundaries.push_back(offset + i);
        }
        if (ref.empty()) ref = boundaries;
        else check(ref == boundaries, "block size changed bar boundaries");
    }
}
static void sliceTest() {
    SliceCapture b; b.prepare(2, 2048); b.onBoundary();
    for (int i = 0; i < 500; ++i) b.write(static_cast<float>(i / 500.0), static_cast<float>(-i / 500.0));
    b.onBoundary();
    check(b.length() == 500, "wrong slice length");
    approx(b.readFinished(0, 200), .4, .000001, "Hermite interpolation exact samples");
    approx(b.readFinished(1, 200), -.4, .000001, "stereo difference");
    Reverser reverse;
    const float beginning = reverse.sample(b, 0, 0, 0, 0, 0, 48000);
    const float ending = reverse.sample(b, 0, 1, 0, 0, 0, 48000);
    check(beginning > ending, "reverse failed");
    check(std::isfinite(reverse.sample(b, 0, .45, 1, 0, -7, 48000)), "pitch shifter non-finite");
    check(std::isfinite(reverse.sample(b, 0, .45, 3, 0, 9, 48000)), "stutter non-finite");
    b.clear();
    check(b.length() == 0 && b.readFinished(0, 200) == 0.f, "seek exposed stale audio");
    b.onBoundary();
    for (int i = 0; i < 128; ++i) b.write(0.f, 0.f);
    b.onBoundary();
    check(b.length() == 128 && b.readFinished(0, 45) == 0.f, "seek reset leaked previous slice");
    SliceCapture mono; mono.prepare(1, 2048); mono.onBoundary();
    for (int i = 0; i < 512; ++i) mono.write(static_cast<float>(i) / 512.f, -2.f);
    mono.onBoundary();
    check(mono.length() == 512, "mono capture length incorrect");
    approx(mono.readFinished(0, 100), mono.readFinished(1, 100), 1e-7,
           "mono channel mapping returned invalid data");
}
static void gateTest() {
    GateSequencer g;
    g.setStep(0, 0, 100);
    g.setStep(1, 2, 100);
    approx(g.target(0, 0, 1, 0), 0, 1e-6, "gate off");
    approx(g.target(1, 0, 1, 0), 1.15, 1e-6, "accent gain");
    check(g.stepAt(.08, 16, 0) == 1, "gate step wrong");
    check(g.stepAt(.08, 16, .75) == 0, "swing did not delay odd steps");
    g.setStep(2, 3, 100); check(g.isReverseStep(2), "reverse step missing");
    GateShaper shape; shape.prepare(48000); shape.reset(0);
    const float one = shape.next(1, 10, 10, 1);
    check(one > 0 && one < 1, "gate smoothing invalid");
    for (int i = 0; i < 5000; ++i) shape.next(1, 10, 10, 1);
    check(shape.next(1, 10, 10, 1) > .99f, "gate not opening");
    GateShaper cached, reference;
    cached.prepare(48000); reference.prepare(48000);
    cached.reset(0.f); reference.reset(0.f);
    cached.setTimes(8.f, 25.f);
    for (int i = 0; i < 24000; ++i) {
        const float target = ((i / 1500) % 2) == 0 ? 1.f : 0.f;
        const int curve = i % 3;
        const float optimized = cached.next(target, curve);
        const float legacy = reference.next(target, 8.f, 25.f, curve);
        approx(optimized, legacy, 1e-7, "cached envelope differs from reference");
    }
}
static void bandTest() {
    BandSplitter band; band.prepare(48000);
    float inside, outside;
    band.setCuts(20, 20000);
    for (int i = 0; i < 5000; ++i) {
        const float input = std::sin(i * .147f) * .8f;
        band.process(0, input, inside, outside);
        approx(inside + outside, input, 1.e-7, "default band not transparent");
    }
    band.setCuts(300, 4500);
    double error = 0;
    for (int i = 0; i < 5000; ++i) {
        const float input = std::sin(i * .147f) * .8f;
        band.process(0, input, inside, outside);
        error = std::max(error, std::abs(static_cast<double>((inside + outside) - input)));
    }
    check(error < 1.e-6, "complementary band null above -90dB");
}
static void deterministicTest() {
    GlitchRandomizer rng;
    for (int64_t i = -100; i < 200; ++i) {
        check(rng.choose(i, 123, 77, true, 1) == rng.choose(i, 123, 77, true, 222), "seed-lock render changed");
        check(GlitchRandomizer::stepEnabled(123, i, 4, 80) == GlitchRandomizer::stepEnabled(123, i, 4, 80), "step mismatch");
    }
    check(std::isfinite(Crossfader::equalPower(.6f, .2f, .5f)), "crossfade nonfinite");
    approx(Crossfader::spliceHold(.7f, -.3f, 0.f), .7f, 1e-7, "splice discontinuity at start");
    approx(Crossfader::spliceHold(.7f, -.3f, 1.f), -.3f, 1e-7, "splice discontinuity at end");
    for (int i = 0; i <= 100; ++i) {
        const float a = Crossfader::spliceHold(.7f, -.3f, i / 100.f);
        check(std::isfinite(a) && a >= -.301f && a <= .701f, "splice generated overshoot");
    }
    TriggerRouter trigger; trigger.prepare(48000); trigger.trigger(.5f, 60);
    check(trigger.active() && trigger.note() == 60 && trigger.velocity() == .5f, "trigger fail");
}

static std::vector<float> renderForBlockSize(int blockSize) {
    constexpr int totalSamples = 144000;
    TempoClock c; c.prepare(48000);
    SliceCapture capture; capture.prepare(1, 48000);
    Reverser rev;
    GateSequencer gate;
    for (int i = 0; i < 16; ++i) gate.setStep(i, i % 4 == 0 ? 0 : 1, 100);
    std::vector<float> result;
    result.reserve(totalSamples);
    for (int block = 0; block < totalSamples; block += blockSize) {
        const auto count = std::min(blockSize, totalSamples - block);
        const double ppq = block / 24000.0;
        c.setHost(120, 4, 4, ppq, true, true, 3, 500);
        for (int i = 0; i < count; ++i) {
            const int absoluteSample = block + i;
            const auto pos = c.tick(i);
            if (pos.boundary) capture.onBoundary();
            const float input = .4f * std::sin(2 * 3.141592653589793 * 220.0 * absoluteSample / 48000.0) +
                                ((absoluteSample % 12000) < 64 ? .5f : 0.f);
            float output = input;
            if (capture.length() > 0)
                output = rev.sample(capture, 0, pos.phase, 0, pos.sliceIndex, 0, 48000);
            const auto gatePos = (pos.beat / 4.0) - std::floor(pos.beat / 4.0);
            const auto step = gate.stepAt(gatePos, 16, 0);
            output *= gate.target(step, static_cast<int64_t>(std::floor(pos.beat / 4)), 123, 0);
            result.push_back(output);
            capture.write(input, input);
        }
    }
    return result;
}
static void offlineReproTest() {
    const auto reference = renderForBlockSize(512);
    for (const int block : {1, 64, 4096, 16384}) {
        const auto actual = renderForBlockSize(block);
        check(actual.size() == reference.size(), "offline render length mismatch");
        for (size_t i = 0; i < reference.size(); ++i)
            if (std::abs(actual[i] - reference[i]) > 2e-6f) { std::cerr << "block=" << block << " sample=" << i << " actual=" << actual[i] << " reference=" << reference[i] << "\n"; check(false, "offline output not deterministic across block sizes"); }
    }
    for (double p : {0.0, .00001, .25, .5, .75, .99999, 1.0}) {
        const auto fade = Crossfader::boundaryEnvelope(p, 96000, 200, 200, 800);
        check(std::isfinite(fade) && fade >= 0 && fade <= 1, "boundary fade invalid");
    }
    BandSplitter b;
    for (auto sr : {22050, 44100, 48000, 96000, 192000}) {
        b.prepare(sr);
        b.setCuts(20, 20000);
        for (int i = 0; i < 100; ++i) {
            float a, c;
            b.process(0, .75f, a, c);
            check(std::isfinite(a + c), "nonfinite at sample rate");
            check(std::abs(a + c - .75f) < 1e-6f, "sample-rate null failed");
        }
    }
}

int main() {
    try { clockTest(); sliceTest(); gateTest(); bandTest(); deterministicTest(); offlineReproTest(); }
    catch (const std::exception& ex) { std::cerr << "FAIL: " << ex.what() << '\n'; return EXIT_FAILURE; }
    std::cout << "PASS: TempoClock / SliceCapture / Reverser / GateSequencer / GateShaper / BandSplitter / Crossfader / TriggerRouter / seeded randomness\n";
    return EXIT_SUCCESS;
}
