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
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
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
    const double invalid = std::numeric_limits<double>::quiet_NaN();
    c.setHost(invalid, 4, 4, invalid, true, true, 5, invalid);
    approx(c.getBpm(), 120., 1e-12, "invalid BPM did not fall back");
    approx(c.getSamplesPerSlice(), 96000., 1e-7, "invalid transport changed slice length");
    check(std::isfinite(c.tick(0).phase), "invalid PPQ caused non-finite phase");
    c.prepare(invalid);
    c.setHost(120., 4, 4, 0., true, true, 5, 500.);
    approx(c.getSamplesPerSlice(), 96000., 1e-7, "invalid sample rate fallback");
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
    for (const float shift : {-12.f, -7.f, 0.f, 4.f, 12.f}) {
        const auto context = Reverser::makePitchContext(shift, 48000);
        for (int k = 0; k <= 100; ++k) {
            const double phase = k / 100.0;
            const float oldApi = reverse.sample(b, 0, phase, 0, 0, shift, 48000);
            const float prepared = reverse.samplePrepared(b, 0, phase, 0, 0, context);
            approx(oldApi, prepared, 1e-6, "cached reverse-pitch result differs");
            check(std::isfinite(prepared), "cached reverse pitch is not finite");
        }
    }
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
    SliceCapture wrapping;
    wrapping.prepare(1, 2048);
    wrapping.onBoundary();
    for (int i = 0; i < 8100; ++i) wrapping.write(.1f, .1f);
    wrapping.onBoundary(); // Intentionally too long for the finished-slice cap.
    for (int i = 0; i < 300; ++i) wrapping.write(.625f, .625f);
    wrapping.onBoundary(); // Cross the 8192-sample physical ring edge.
    check(wrapping.length() == 300, "ring wrap invalid finished length");
    for (double position : {2.0, 40.5, 100.0, 190.25, 297.0})
        approx(wrapping.readFinished(0, position), .625, 1e-6,
               "ring-buffer wrap changed Hermite read");
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
    const auto fullGainReverse = g.evaluate(2, 19, 733, 1.f);
    check(fullGainReverse.reverse && fullGainReverse.gain == 1.f,
          "reverse step incorrectly disabled when gate depth is unity");
    g.setStep(2, 3, 0.f);
    check(!g.evaluate(2, 19, 733, .001f).reverse,
          "probability-zero reverse step should not play");
    g.setStep(2, 3, 100.f);
    for (int64_t cycle = 0; cycle < 64; ++cycle) {
        const auto once = g.evaluate(2, cycle, 733, .8f);
        const auto twice = g.evaluate(2, cycle, 733, .8f);
        check(once.reverse && once.reverse == twice.reverse &&
              once.gain == twice.gain, "reverse probability decision nondeterministic");
    }
    GateShaper shape; shape.prepare(48000); shape.reset(0);
    const float one = shape.next(1, 10, 10, 1);
    check(one > 0 && one < 1, "gate smoothing invalid");
    for (int i = 0; i < 5000; ++i) shape.next(1, 10, 10, 1);
    check(shape.next(1, 10, 10, 1) > .99f, "gate not opening");
    GateShaper linear;
    linear.prepare(1000);
    linear.reset(0.f);
    linear.setTimes(10.f, 20.f);
    for (int n = 0; n < 9; ++n)
        check(linear.next(1.f, 0) < 1.f, "linear attack ended too early");
    approx(linear.next(1.f, 0), 1.f, 1e-7, "linear attack missed its endpoint");
    for (int n = 0; n < 19; ++n)
        check(linear.next(0.f, 0) > 0.f, "linear release ended too early");
    approx(linear.next(0.f, 0), 0.f, 1e-7, "linear release missed its endpoint");
    GateShaper tail; tail.prepare(48000); tail.setTimes(1.f, 200.f);
    tail.reset(1.f);
    for (int i = 0; i < 350000; ++i) {
        const float value = tail.next(0.f, 1);
        check(std::isfinite(value) && std::fpclassify(value) != FP_SUBNORMAL,
              "denormal exponential gate tail");
    }
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
    for (const float t : {0.f, .1f, .5f, .99f, 1.f}) {
        const auto gains = Crossfader::equalPowerGains(t);
        const float combined = .6f * gains.dry + .2f * gains.wet;
        approx(combined, Crossfader::equalPower(.6f, .2f, t), 1e-7, "shared stereo mix coefficients changed gain");
        const auto unity = Crossfader::unityGains(t);
        approx(unity.dry + unity.wet, 1., 1e-6, "unity crossfade changes correlated audio");
        const float input = .95f;
        approx(input * unity.dry + input * unity.wet, input, 1e-6,
               "unity mix clips correlated 0.95-amplitude input");
    }
    const auto nanMix = Crossfader::unityGains(std::numeric_limits<float>::quiet_NaN());
    check(nanMix.dry == 1.f && nanMix.wet == 0.f, "non-finite mix has no safe fallback");
    approx(Crossfader::spliceHold(.7f, -.3f, 0.f), .7f, 1e-7, "splice discontinuity at start");
    approx(Crossfader::spliceHold(.7f, -.3f, 1.f), -.3f, 1e-7, "splice discontinuity at end");
    for (int i = 0; i <= 100; ++i) {
        const float a = Crossfader::spliceHold(.7f, -.3f, i / 100.f);
        check(std::isfinite(a) && a >= -.301f && a <= .701f, "splice generated overshoot");
    }
    TriggerRouter trigger; trigger.prepare(48000); trigger.trigger(.5f, 60);
    check(trigger.active() && trigger.note() == 60 && trigger.velocity() == .5f, "trigger fail");
    TriggerRouter sidechain; sidechain.prepare(48000);
    const auto bad = std::numeric_limits<float>::quiet_NaN();
    check(!sidechain.processSidechain(bad), "NaN sidechain caused false trigger");
    check(!sidechain.processSidechain(std::numeric_limits<float>::infinity()),
          "Inf sidechain caused false trigger");
    bool detected = false;
    for (int i = 0; i < 16; ++i) detected |= sidechain.processSidechain(.95f);
    check(detected && sidechain.active(), "sidechain failed to recover after NaN/Inf");
    sidechain.trigger(bad, 180);
    check(std::isfinite(sidechain.velocity()) && sidechain.note() == 127,
          "non-finite trigger controls were not sanitized");
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
static void stressTest() {
    // Use a real reverse/grain/gate/mix path with known stereo anti-phase input.
    // The input stays below full scale, so unintended +3 dB mix gain would fail.
    Reverser reverse;
    SliceCapture capture;
    capture.prepare(2, 48000);
    capture.onBoundary();
    constexpr int length = 12000;
    for (int i = 0; i < length; ++i) {
        const float x = .55f * std::sin(i * 6.283185307179586 / 97.0);
        capture.write(x, -x);
    }
    capture.onBoundary();
    GateShaper gates[2];
    for (auto& g : gates) { g.prepare(48000); g.reset(1.f); g.setTimes(3.f, 15.f); }
    const auto began = std::chrono::steady_clock::now();
    double checksum = 0.0;
    for (int mode = 0; mode < 4; ++mode) {
        for (float pitch : {-12.f, 0.f, 7.f, 12.f}) {
            const auto context = Reverser::makePitchContext(pitch, 48000);
            for (int i = 0; i < 48000; ++i) {
                const double phase = (i % length) / static_cast<double>(length);
                const float target = (i / 512) % 2 ? 0.f : 1.f;
                const auto gains = Crossfader::unityGains(.5f);
                const float a = reverse.samplePrepared(capture, 0, phase, mode, i / length, context);
                const float b = reverse.samplePrepared(capture, 1, phase, mode, i / length, context);
                const float l = a * gates[0].next(target, 1) * gains.wet + .55f * gains.dry;
                const float r = b * gates[1].next(target, 1) * gains.wet - .55f * gains.dry;
                check(std::isfinite(l) && std::isfinite(r), "stress render produced NaN/Inf");
                check(std::abs(l) <= 1.f && std::abs(r) <= 1.f,
                      "stress render unexpectedly clipped below-unity input");
                check(std::abs(l + r) < 1.e-5f, "reverse processing broke stereo anti-phase coherence");
                checksum += l * .0000001;
            }
        }
    }
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - began).count();
    check(std::isfinite(checksum), "stress checksum was non-finite");
    std::cout << "DSP_STRESS: 768000 stereo frames, "
              << elapsed << " s, checksum=" << checksum << '\n';
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
    try { clockTest(); sliceTest(); gateTest(); bandTest(); deterministicTest(); offlineReproTest(); stressTest(); }
    catch (const std::exception& ex) { std::cerr << "FAIL: " << ex.what() << '\n'; return EXIT_FAILURE; }
    std::cout << "PASS: TempoClock / SliceCapture / Reverser / GateSequencer / GateShaper / BandSplitter / Crossfader / TriggerRouter / seeded randomness\n";
    return EXIT_SUCCESS;
}
