#include "PluginProcessor.h"
#include <chrono>
#include <cstdio>
#include <random>
#include <cmath>

using P = SkellgateAudioProcessor;
static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("FAIL: %s\n", msg); } else std::printf("ok:   %s\n", msg); } while (0)

static std::vector<float> run (P& p, const std::vector<float>& in, int block = 512)
{
    std::vector<float> out (in.size());
    juce::MidiBuffer midi;
    for (size_t pos = 0; pos < in.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, in.size() - pos);
        juce::AudioBuffer<float> b (2, n);
        for (int i = 0; i < n; ++i) { b.setSample (0, i, in[pos + (size_t) i]); b.setSample (1, i, in[pos + (size_t) i]); }
        p.processBlock (b, midi);
        for (int i = 0; i < n; ++i) out[pos + (size_t) i] = b.getSample (0, i);
    }
    return out;
}

static void setParam (P& p, const char* id, float real)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (real));
}


static double xs (double n, double sr)   // smooth multi-tone test signal, n in samples (may be fractional)
{
    const double t = n / sr, w = 2.0 * juce::MathConstants<double>::pi;
    return 0.2 * std::sin (w * 110.0 * t) + 0.15 * std::sin (w * 330.0 * t + 1.0) + 0.1 * std::sin (w * 1000.0 * t + 2.0);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000.0;
    std::mt19937 rng (1);
    std::uniform_real_distribution<float> d (-0.5f, 0.5f);
    std::vector<float> noise ((size_t) (sr * 4)); for (auto& s : noise) s = d (rng);

    // 1. Empty/Empty = bit-ish perfect passthrough
    {
        P p; p.setRateAndBufferSizeDetails (sr, 512); p.prepareToPlay (sr, 512);
        setParam (p, "att", 0.0f); setParam (p, "rel", 0.0f);
        auto out = run (p, noise);
        double maxErr = 0; for (size_t i = 2000; i < out.size(); ++i) maxErr = std::max (maxErr, (double) std::abs (out[i] - noise[i]));
        std::printf("      passthrough max error %.2e\n", maxErr);
        CHECK (maxErr < 1e-4, "Empty/Empty passes audio through");
    }

    // 2. Stutter: time preset 3 (repeat x2). 120 BPM free-run => pattern = 1.0 s = 48000 samples
    //    second half of each pass plays the audio from 0.5001 patterns earlier
    {
        P p; p.setRateAndBufferSizeDetails (sr, 512); p.prepareToPlay (sr, 512);
        p.setTimePreset (3);
        setParam (p, "att", 0.0f); setParam (p, "rel", 0.0f);
        std::vector<float> in ((size_t) (sr * 4)); for (size_t i = 0; i < in.size(); ++i) in[i] = (float) xs ((double) i, sr);
        auto out = run (p, in);
        const double lag = 0.5001 * 48000.0;
        double maxErr = 0;
        for (int k = 400; k < 23500; ++k) maxErr = std::max (maxErr, std::abs ((double) out[(size_t) (72000 + k)] - xs (72000 + k - lag, sr)));
        std::printf("      stutter error %.2e\n", maxErr);
        CHECK (maxErr < 5e-3, "Basic 3: 2nd half of the pattern repeats the 1st half at normal speed");
        double passErr = 0;
        for (int k = 100; k < 23500; ++k) passErr = std::max (passErr, std::abs ((double) out[(size_t) (48000 + k)] - xs (48000 + k, sr)));
        CHECK (passErr < 5e-3, "Basic 3: 1st half of the pattern is untouched");
    }

    // 3. Reverse: time preset 9. y = 1 - phase  =>  delay = (2*phase - 1) * P, wrapped into 0..P
    {
        P p; p.setRateAndBufferSizeDetails (sr, 512); p.prepareToPlay (sr, 512);
        p.setTimePreset (9);
        setParam (p, "att", 0.0f); setParam (p, "rel", 0.0f);
        std::vector<float> in ((size_t) (sr * 4)); for (size_t i = 0; i < in.size(); ++i) in[i] = (float) xs ((double) i, sr);
        auto out = run (p, in);
        double maxErr = 0;
        for (int k = 1000; k < 47000; k += 3)
        {
            if (std::abs (k - 24000) < 200) continue;   // the read position wraps here (crossfade region)
            const double ph = k / 48000.0;
            double delay = (2.0 * ph - 1.0) * 48000.0; if (delay < 0) delay += 48000.0;
            maxErr = std::max (maxErr, std::abs ((double) out[(size_t) (96000 + k)] - xs (96000 + k - delay, sr)));
        }
        std::printf("      reverse error %.2e\n", maxErr);
        CHECK (maxErr < 2e-2, "Basic 9 reads the pattern backwards");
    }

    // 4. Gate: volume preset 5 (8 pulses / 2 beats, 50% duty)
    {
        P p; p.setRateAndBufferSizeDetails (sr, 512); p.prepareToPlay (sr, 512);
        p.setVolumePreset (5);
        setParam (p, "att", 0.0f); setParam (p, "rel", 0.0f);
        std::vector<float> ones (48000 * 2, 0.5f);
        auto out = run (p, ones);
        int silent = 0, loud = 0;
        for (size_t i = 48000; i < 96000; ++i) { if (std::abs (out[i]) < 1e-3f) ++silent; else if (out[i] > 0.45f) ++loud; }
        const double frac = (double) silent / 48000.0;
        std::printf("      gate silent fraction %.3f loud fraction %.3f\n", frac, (double) loud / 48000.0);
        CHECK (frac > 0.42 && frac < 0.58, "1/4 Bt Gate is silent about half the time");
    }

    // 5. Declick: sine through stutter, biggest sample-to-sample step should stay small
    {
        P p; p.setRateAndBufferSizeDetails (sr, 512); p.prepareToPlay (sr, 512);
        p.setTimePreset (6);   // 1/16 stutter: lots of jumps
        setParam (p, "att", 0.0f); setParam (p, "rel", 0.0f);
        std::vector<float> sine ((size_t) (sr * 3));
        for (size_t i = 0; i < sine.size(); ++i) sine[i] = 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr);
        auto out = run (p, sine);
        double maxStep = 0; for (size_t i = 60000; i < out.size(); ++i) maxStep = std::max (maxStep, (double) std::abs (out[i] - out[i - 1]));
        std::printf("      max step with crossfade %.3f (unfaded jump would be up to ~1.0)\n", maxStep);
        CHECK (maxStep < 0.35, "time jumps are crossfaded (no hard clicks)");
    }

    // 6. Every preset combination is finite and bounded, with random node edits
    {
        P p; p.setRateAndBufferSizeDetails (sr, 512); p.prepareToPlay (sr, 512);
        bool ok = true;
        std::vector<float> in ((size_t) (sr * 2.1)); for (auto& s : in) s = d (rng);
        for (int t = 0; t < 36 && ok; ++t)
            for (int v = 0; v < 36 && ok; v += 5)
            {
                p.setTimePreset (t); p.setVolumePreset (v);
                if ((t + v) % 3 == 0) { p.addNode (true, 0.33f, 0.9f); p.moveNode (false, 1, 0.2f, 0.4f); p.removeNode (true, 1); }
                auto out = run (p, in);
                for (float s : out) if (! std::isfinite (s) || std::abs (s) > 1.01f) { ok = false; std::printf("      bad sample t=%d v=%d\n", t, v); break; }
            }
        CHECK (ok, "all 36 time x volume presets stay finite and in range");
    }

    // 7. Node API invariants
    {
        P p; p.setTimePreset (3);
        auto c0 = p.getCurve (true);
        CHECK (std::abs (c0.nodes[0].x) < 1e-6f && std::abs (c0.nodes[(size_t) c0.count - 1].x - 1.0f) < 1e-6f, "curve spans x = 0..1");
        p.addNode (true, 2.0f, 0.5f);   // far right must land BEFORE the end node
        auto c1 = p.getCurve (true);
        bool sorted = true; for (int i = 1; i < c1.count; ++i) if (c1.nodes[(size_t) i].x < c1.nodes[(size_t) i - 1].x) sorted = false;
        CHECK (sorted && std::abs (c1.nodes[(size_t) c1.count - 1].x - 1.0f) < 1e-6f, "addNode keeps nodes sorted and the end point last");
        for (int i = 0; i < 400; ++i) p.addNode (false, (float) i / 400.0f, 0.5f);
        CHECK (p.getCurve (false).count <= P::kMaxNodes, "node count is capped");
    }

    // 8. State round trip
    {
        P a; a.setTimePreset (12); a.setVolumePreset (7); a.addNode (true, 0.4f, 0.3f);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        P b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        auto ca = a.getCurve (true), cb = b.getCurve (true);
        bool same = ca.count == cb.count && b.getCurrentTimePreset() == 12 && b.getCurrentVolumePreset() == 7;
        for (int i = 0; same && i < ca.count; ++i) same = std::abs (ca.nodes[(size_t) i].x - cb.nodes[(size_t) i].x) < 1e-4f && std::abs (ca.nodes[(size_t) i].y - cb.nodes[(size_t) i].y) < 1e-4f;
        CHECK (same, "state save / load restores presets and edited curves");
    }

    // 9. Speed
    {
        P p; p.setRateAndBufferSizeDetails (sr, 512); p.prepareToPlay (sr, 512);
        p.setTimePreset (14); p.setVolumePreset (13);
        std::vector<float> in ((size_t) (sr * 10)); for (auto& s : in) s = d (rng);
        auto t0 = std::chrono::steady_clock::now();
        run (p, in);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf("      10 s of stereo audio processed in %.1f ms (%.2f%% of one core)\n", secs * 1000, secs / 10.0 * 100.0);
        CHECK (secs < 2.0, "runs comfortably faster than real time");
    }

    std::printf ("\n%s (%d failures)\n", fails == 0 ? "ALL PASSED" : "FAILURES", fails);
    return fails == 0 ? 0 : 1;
}
