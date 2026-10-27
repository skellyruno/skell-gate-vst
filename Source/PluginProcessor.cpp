#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>
#include <random>

static const char* kTimeNames[36] = {
    "Empty", "Basic 1", "Basic 2", "Basic 3",
    "Basic 4", "Basic 5", "Basic 6", "Basic 7",
    "Basic 8", "Basic 9", "Basic 10", "Basic 11",
    "Complex 1", "Complex 2", "Complex 3", "Complex 4",
    "Complex 5", "Complex 6", "Complex 7", "Complex 8",
    "Complex 9", "Complex 10", "Complex 11", "Complex 12",
    "Chaos 1", "Chaos 2", "Chaos 3", "Chaos 4",
    "Chaos 5", "Chaos 6", "Chaos 7", "Chaos 8",
    "Chaos 9", "Chaos 10", "Chaos 11", "Chaos 12"
};

static const char* kVolNames[36] = {
    "Empty", "2 Beat Gate", "1 Beat Gate", "1/2 Bt Gate",
    "1/3 Bt Gate", "1/4 Bt Gate", "1/6 Bt Gate", "1/8 Bt Gate",
    "Trance Gt 1", "Trance Gt 2", "Trance Gt 3", "Trance Gt 4",
    "Saw Gate 1", "Saw Gate 2", "1st Step", "1st Stp Fade",
    "2 Step Gate", "Off Beat", "1/4 Dynamic", "1/4 Swing",
    "1/4 Gate Out", "1/4 Gate In", "Spd Up Gate", "Slw Dn Gate",
    "End Fade", "End Gate", "Tremelo Slw", "Tremelo Fst",
    "Sidechain", "Drum Loop", "Copter", "AM",
    "Fade In", "Fade Out", "Fade Out In", "Mute"
};

const char* SkellgateAudioProcessor::getTimePresetName (int index)   { return kTimeNames[juce::jlimit (0, 35, index)]; }
const char* SkellgateAudioProcessor::getVolumePresetName (int index) { return kVolNames[juce::jlimit (0, 35, index)]; }

//==============================================================================
// Preset generation (allocation free, so it can run on the audio thread too)
//==============================================================================
namespace
{
using Curve = SkellgateAudioProcessor::Curve;
constexpr float kEps = 1.0e-4f;

struct Builder
{
    Curve& c;

    Builder (Curve& curve, float y0) : c (curve)
    {
        c.count = 0;
        add (0.0f, y0);
    }

    float lastX() const { return c.nodes[(size_t) c.count - 1].x; }
    float lastY() const { return c.nodes[(size_t) c.count - 1].y; }

    void add (float x, float y)
    {
        if (c.count >= SkellgateAudioProcessor::kMaxNodes)
            return;
        c.nodes[(size_t) c.count++] = { juce::jlimit (0.0f, 1.0f, x), juce::jlimit (0.0f, 1.0f, y) };
    }

    // Instant change of value at x (hold the old value up to x, then jump)
    void jump (float x, float y)
    {
        const float ly = lastY();
        const float lx = lastX();
        if (std::abs (ly - y) < 1.0e-6f)
            return;
        if (x > lx + kEps)
            add (x, ly);
        else
            x = lx;
        add (x + kEps, y);
    }

    void finish()
    {
        if (lastX() >= 1.0f)
            return;
        if (c.count < SkellgateAudioProcessor::kMaxNodes)
            add (1.0f, lastY());
        else
            c.nodes[(size_t) c.count - 1].x = 1.0f;
    }
};

void buildTime (int idx, Curve& c)
{
    // y = n repeats of the first 1/n of the pattern (classic stutter)
    auto repeat = [&] (int n)
    {
        Builder b (c, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const float s = (float) i / (float) n;
            const float e = (float) (i + 1) / (float) n;
            b.jump (s, 0.0f);
            b.add (e, e - b.lastX());           // slope exactly 1 = normal speed, no drift
        }
        b.finish();
    };

    // y = n full-range saws (n x speed playback)
    auto fast = [&] (int n)
    {
        Builder b (c, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            b.jump ((float) i / (float) n, 0.0f);
            b.add ((float) (i + 1) / (float) n, 1.0f);
        }
        b.finish();
    };

    // each of n slices played backwards
    auto reverse = [&] (int n)
    {
        Builder b (c, 1.0f / (float) n);
        for (int i = 0; i < n; ++i)
        {
            const float s = (float) i / (float) n;
            const float e = (float) (i + 1) / (float) n;
            b.jump (s, e);
            b.add (e, b.lastX());                    // slope exactly -1 (reverse at normal speed)
        }
        b.finish();
    };

    auto stairs = [&] (int n)
    {
        Builder b (c, 0.0f);
        for (int i = 1; i < n; ++i)
            b.jump ((float) i / (float) n, (float) i / (float) n);
        b.finish();
    };

    switch (idx)
    {
        case 0:  { Builder b (c, 0.0f); b.add (1.0f, 1.0f); break; }              // Empty: y = phase, passthrough

        // ---- Basic ----
        case 1:  { Builder b (c, 0.0f); b.add (1.0f, 0.5f); break; }              // half speed
        case 2:  { Builder b (c, 0.0f); b.add (1.0f, 0.25f); break; }             // quarter speed
        case 3:  repeat (2);  break;
        case 4:  repeat (4);  break;
        case 5:  repeat (8);  break;
        case 6:  repeat (16); break;
        case 7:  repeat (3);  break;
        case 8:  repeat (6);  break;
        case 9:  reverse (1); break;
        case 10: reverse (2); break;
        case 11: reverse (4); break;

        // ---- Complex ----
        case 12: fast (2);   break;
        case 13: fast (3);   break;
        case 14: fast (4);   break;
        case 15:                                                                   // slowing down (y = phase^2)
        {
            Builder b (c, 0.0f);
            for (int i = 1; i <= 8; ++i) { const float p = (float) i / 8.0f; b.add (p, p * p); }
            break;
        }
        case 16:                                                                   // speeding up (y = sqrt(phase))
        {
            Builder b (c, 0.0f);
            for (int i = 1; i <= 8; ++i) { const float p = (float) i / 8.0f; b.add (p, std::sqrt (p)); }
            break;
        }
        case 17: { Builder b (c, 0.0f); b.add (0.5f, 1.0f); b.add (1.0f, 0.0f); break; }                                   // scratch
        case 18: { Builder b (c, 0.0f); b.add (0.25f, 1.0f); b.add (0.5f, 0.0f); b.add (0.75f, 1.0f); b.add (1.0f, 0.0f); break; }
        case 19: stairs (4); break;
        case 20: stairs (8); break;
        case 21: { Builder b (c, 0.0f); b.add (0.25f, 0.25f); b.add (0.5f, 0.0f); b.add (0.75f, 0.25f); b.add (1.0f, 0.0f); break; } // ping-pong
        case 22: { Builder b (c, 0.0f); b.add (0.5f, 0.5f); b.add (1.0f, 0.5f); break; }                                   // play then freeze
        case 23:                                                                   // tape stop (speed ramps 1 -> 0)
        {
            Builder b (c, 0.0f);
            for (int i = 1; i <= 8; ++i) { const float p = (float) i / 8.0f; b.add (p, p - 0.5f * p * p); }
            break;
        }

        // ---- Chaos: deterministic pseudo-random patterns ----
        default:
        {
            const int k = idx - 24;                       // 0..11
            std::mt19937 rng (1000u + (unsigned) k);
            std::uniform_real_distribution<float> dist (0.0f, 1.0f);
            const int n = (k % 3 == 0) ? 8 : (k % 3 == 1) ? 16 : 12;
            const bool ramp = (k % 2 == 1);

            Builder b (c, dist (rng));
            for (int i = 1; i < n; ++i)
            {
                const float x = (float) i / (float) n;
                const float y = dist (rng);
                if (ramp) b.add (x, y); else b.jump (x, y);
            }
            b.finish();
            break;
        }
    }
}

struct Pulse { float s, e, lvl; };

void buildVolume (int idx, Curve& c)
{
    // n pulses per pattern, each "on" for duty/n of its slot
    auto gate = [&] (int n, float duty, const float* levels, int numLevels, float swing)
    {
        Builder b (c, levels != nullptr ? levels[0] : 1.0f);
        for (int i = 0; i < n; ++i)
        {
            const float lvl = levels != nullptr ? levels[i % numLevels] : 1.0f;
            const float s = (float) i / (float) n + ((i % 2 == 1) ? swing / (float) n : 0.0f);
            b.jump (s, lvl);
            b.jump (s + duty / (float) n, 0.0f);
        }
        b.finish();
    };

    // 16-step on/off patterns
    auto steps = [&] (const char* pat, float onFrac)
    {
        const int n = (int) std::strlen (pat);
        Builder b (c, pat[0] == '1' ? 1.0f : 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const float s = (float) i / (float) n;
            if (pat[i] == '1')
            {
                b.jump (s, 1.0f);
                b.jump (s + onFrac / (float) n, 0.0f);
            }
            else
                b.jump (s, 0.0f);
        }
        b.finish();
    };

    auto saw = [&] (int n)
    {
        Builder b (c, 1.0f);
        for (int i = 0; i < n; ++i)
        {
            b.jump ((float) i / (float) n, 1.0f);
            b.add ((float) (i + 1) / (float) n, 0.0f);
        }
        b.finish();
    };

    auto tremolo = [&] (int cycles, float lo)
    {
        Builder b (c, 1.0f);
        for (int k = 1; k <= 2 * cycles; ++k)
            b.add ((float) k / (float) (2 * cycles), (k % 2 == 1) ? lo : 1.0f);
        b.finish();
    };

    // pulses that get shorter and shorter (or the mirror image of that)
    auto accelerating = [&] (bool mirrored)
    {
        std::array<Pulse, 40> list {};
        int num = 0;
        float s = 0.0f, p = 0.25f;
        while (s < 0.98f && num < (int) list.size())
        {
            list[(size_t) num++] = { s, s + p * 0.5f, 1.0f };
            s += p;
            p *= 0.75f;
        }

        Builder b (c, 0.0f);
        for (int i = 0; i < num; ++i)
        {
            const Pulse& q = mirrored ? list[(size_t) (num - 1 - i)] : list[(size_t) i];
            const float s0 = mirrored ? 1.0f - q.e : q.s;
            const float e0 = mirrored ? 1.0f - q.s : q.e;
            b.jump (s0, q.lvl);
            b.jump (e0, 0.0f);
        }
        b.finish();
    };

    static const float kDynamic[4]  = { 1.0f, 0.5f, 0.8f, 0.4f };
    static const float kGateOut[8]  = { 1.0f, 0.85f, 0.7f, 0.55f, 0.4f, 0.3f, 0.2f, 0.1f };
    static const float kGateIn[8]   = { 0.1f, 0.2f, 0.3f, 0.4f, 0.55f, 0.7f, 0.85f, 1.0f };

    switch (idx)
    {
        case 0:  { Builder b (c, 1.0f); b.finish(); break; }                       // Empty = full volume
        case 1:  gate (1,  0.5f, nullptr, 0, 0.0f); break;
        case 2:  gate (2,  0.5f, nullptr, 0, 0.0f); break;
        case 3:  gate (4,  0.5f, nullptr, 0, 0.0f); break;
        case 4:  gate (6,  0.5f, nullptr, 0, 0.0f); break;
        case 5:  gate (8,  0.5f, nullptr, 0, 0.0f); break;
        case 6:  gate (12, 0.5f, nullptr, 0, 0.0f); break;
        case 7:  gate (16, 0.5f, nullptr, 0, 0.0f); break;

        case 8:  steps ("1110111011101110", 0.7f); break;
        case 9:  steps ("1101101011011010", 0.7f); break;
        case 10: steps ("1011011010110110", 0.7f); break;
        case 11: steps ("1001100110011001", 0.7f); break;

        case 12: saw (2); break;
        case 13: saw (4); break;
        case 14: { Builder b (c, 1.0f); b.jump (0.125f, 0.0f); b.finish(); break; }                 // 1st Step
        case 15: { Builder b (c, 1.0f); b.add (0.125f, 0.0f); b.finish(); break; }                  // 1st Step Fade
        case 16: gate (2, 0.25f, nullptr, 0, 0.0f); break;                                          // 2 Step Gate

        case 17:                                                                                    // Off Beat
        {
            Builder b (c, 0.0f);
            for (int i = 0; i < 2; ++i)
            {
                const float s = (float) i * 0.5f;
                b.jump (s + 0.25f, 1.0f);
                b.jump (s + 0.499f, 0.0f);
            }
            b.finish();
            break;
        }

        case 18: gate (8, 0.5f, kDynamic, 4, 0.0f); break;
        case 19: gate (8, 0.4f, nullptr, 0, 0.3f);  break;
        case 20: gate (8, 0.5f, kGateOut, 8, 0.0f); break;
        case 21: gate (8, 0.5f, kGateIn, 8, 0.0f);  break;
        case 22: accelerating (false); break;
        case 23: accelerating (true);  break;

        case 24: { Builder b (c, 1.0f); b.add (0.75f, 1.0f); b.add (1.0f, 0.0f); break; }           // End Fade
        case 25:                                                                                    // End Gate
        {
            Builder b (c, 1.0f);
            for (int i = 0; i < 8; ++i)
                b.jump (0.75f + (float) i * 0.03125f, (i % 2 == 0) ? 1.0f : 0.0f);
            b.finish();
            break;
        }

        case 26: tremolo (2, 0.15f);  break;
        case 27: tremolo (8, 0.15f);  break;

        case 28:                                                                                    // Sidechain pump
        {
            Builder b (c, 0.0f);
            for (int i = 0; i < 2; ++i)
            {
                const float s = (float) i * 0.5f;
                b.jump (s, 0.0f);
                b.add (s + 0.1f, 1.0f);
            }
            b.finish();
            break;
        }

        case 29:                                                                                    // Drum Loop
        {
            static const int hits[6] = { 0, 3, 6, 8, 11, 14 };
            Builder b (c, 1.0f);
            for (int k = 0; k < 6; ++k)
            {
                b.jump ((float) hits[k] / 16.0f, 1.0f);
                b.add (k + 1 < 6 ? (float) hits[k + 1] / 16.0f : 1.0f, 0.1f);
            }
            break;
        }

        case 30: gate (12, 0.5f, nullptr, 0, 0.0f); break;                                          // Copter
        case 31: tremolo (40, 0.0f); break;                                                         // AM

        case 32: { Builder b (c, 0.0f); b.add (1.0f, 1.0f); break; }                                // Fade In
        case 33: { Builder b (c, 1.0f); b.add (1.0f, 0.0f); break; }                                // Fade Out
        case 34: { Builder b (c, 1.0f); b.add (0.5f, 0.0f); b.add (1.0f, 1.0f); break; }            // Fade Out In
        default: { Builder b (c, 0.0f); b.finish(); break; }                                        // Mute
    }
}

juce::String curveToString (const Curve& c)
{
    juce::String s;
    for (int i = 0; i < c.count; ++i)
    {
        if (i > 0) s << ";";
        s << juce::String (c.nodes[(size_t) i].x, 6) << "," << juce::String (c.nodes[(size_t) i].y, 6);
    }
    return s;
}

bool stringToCurve (const juce::String& str, Curve& c)
{
    juce::StringArray pairs;
    pairs.addTokens (str, ";", "");
    pairs.removeEmptyStrings();
    if (pairs.size() < 2 || pairs.size() > SkellgateAudioProcessor::kMaxNodes)
        return false;

    c.count = 0;
    float lastX = 0.0f;
    for (auto& p : pairs)
    {
        juce::StringArray xy;
        xy.addTokens (p, ",", "");
        if (xy.size() != 2) return false;
        const float x = juce::jlimit (lastX, 1.0f, xy[0].getFloatValue());
        const float y = juce::jlimit (0.0f, 1.0f, xy[1].getFloatValue());
        c.nodes[(size_t) c.count++] = { x, y };
        lastX = x;
    }
    return true;
}
} // namespace

//==============================================================================
SkellgateAudioProcessor::SkellgateAudioProcessor()
     : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    timeMixParam = apvts.getRawParameterValue ("timeMix");
    volMixParam  = apvts.getRawParameterValue ("volMix");
    attParam     = apvts.getRawParameterValue ("att");
    relParam     = apvts.getRawParameterValue ("rel");
    tnsParam     = apvts.getRawParameterValue ("tns");
    scratchParam = apvts.getRawParameterValue ("scratch");
    holdParam    = apvts.getRawParameterValue ("hold");

    setTimePreset (0);
    setVolumePreset (0);
    setActiveViewLane (LaneTime);
}

SkellgateAudioProcessor::~SkellgateAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout SkellgateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "timeMix", 1 }, "Time Mix",   juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "volMix", 1 },  "Volume Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "att", 1 },     "Attack",     juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "rel", 1 },     "Release",    juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "tns", 1 },     "Tension",    juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "scratch", 1 }, "Offset", juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "hold", 1 },    "Hold", false));

    return { params.begin(), params.end() };
}

void SkellgateAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    bufferLength = static_cast<int> (sampleRate * 10.0);   // plenty for a 2-beat pattern at >= 30 BPM (4 s)
    delayBuffer.setSize (2, bufferLength);
    delayBuffer.clear();
    writeHead = 0;
    freeRunPpq = 0.0;
    heldPhase = 0.0f;
    gainState = 1.0f;
    fadeLen = juce::jmax (16, static_cast<int> (sampleRate * 0.002));   // 2 ms crossfade
    fadeCount = 0;
    fadeOldDelay = 0.0f;
    prevDelay = 0.0f;
    audioVersion = -1;
    currentPhase.store (0.0f);
}

void SkellgateAudioProcessor::releaseResources() {}

bool SkellgateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

//==============================================================================
void SkellgateAudioProcessor::setTimePreset (int index)
{
    index = juce::jlimit (0, kNumPresets - 1, index);
    Curve c;
    buildTime (index, c);
    {
        const juce::SpinLock::ScopedLockType sl (curveLock);
        timeCurve = c;
    }
    currentTimePreset.store (index);
    setActiveViewLane (LaneTime);
    ++curveVersion;
}

void SkellgateAudioProcessor::setVolumePreset (int index)
{
    index = juce::jlimit (0, kNumPresets - 1, index);
    Curve c;
    buildVolume (index, c);
    {
        const juce::SpinLock::ScopedLockType sl (curveLock);
        volCurve = c;
    }
    currentVolumePreset.store (index);
    setActiveViewLane (LaneVolume);
    ++curveVersion;
}

float SkellgateAudioProcessor::getTensionExponent() const
{
    // knob 50 = straight lines; lower = ease-in (slow start), higher = ease-out
    return std::pow (4.0f, (50.0f - tnsParam->load()) / 50.0f);
}

SkellgateAudioProcessor::Curve SkellgateAudioProcessor::getCurve (bool isTimeLane) const
{
    const juce::SpinLock::ScopedLockType sl (curveLock);
    return isTimeLane ? timeCurve : volCurve;
}

void SkellgateAudioProcessor::moveNode (bool isTimeLane, int index, float x, float y)
{
    {
        const juce::SpinLock::ScopedLockType sl (curveLock);
        Curve& c = isTimeLane ? timeCurve : volCurve;
        if (index < 0 || index >= c.count)
            return;

        y = juce::jlimit (0.0f, 1.0f, y);
        if (index == 0 || index == c.count - 1)
            x = c.nodes[(size_t) index].x;              // end points stay at 0 and 1
        else
            x = juce::jlimit (c.nodes[(size_t) index - 1].x, c.nodes[(size_t) index + 1].x, x);

        c.nodes[(size_t) index] = { x, y };
    }
    ++curveVersion;
}

int SkellgateAudioProcessor::addNode (bool isTimeLane, float x, float y)
{
    int result = -1;
    {
        const juce::SpinLock::ScopedLockType sl (curveLock);
        Curve& c = isTimeLane ? timeCurve : volCurve;
        if (c.count >= kMaxNodes)
            return -1;

        x = juce::jlimit (0.0f, 1.0f, x);
        y = juce::jlimit (0.0f, 1.0f, y);

        int pos = 1;
        while (pos < c.count - 1 && c.nodes[(size_t) pos].x <= x)   // never past the end point at x = 1
            ++pos;

        for (int i = c.count; i > pos; --i)
            c.nodes[(size_t) i] = c.nodes[(size_t) i - 1];

        c.nodes[(size_t) pos] = { x, y };
        ++c.count;
        result = pos;
    }
    ++curveVersion;
    return result;
}

void SkellgateAudioProcessor::removeNode (bool isTimeLane, int index)
{
    {
        const juce::SpinLock::ScopedLockType sl (curveLock);
        Curve& c = isTimeLane ? timeCurve : volCurve;
        if (c.count <= 2 || index <= 0 || index >= c.count - 1)
            return;

        for (int i = index; i < c.count - 1; ++i)
            c.nodes[(size_t) i] = c.nodes[(size_t) i + 1];
        --c.count;
    }
    ++curveVersion;
}

//==============================================================================
float SkellgateAudioProcessor::evaluateCurve (const Curve& c, float phase, float tensionExp)
{
    if (c.count == 0)
        return 0.0f;

    for (int i = 0; i < c.count - 1; ++i)
    {
        const Node& a = c.nodes[(size_t) i];
        const Node& b = c.nodes[(size_t) i + 1];

        if (phase < b.x)
        {
            const float w = b.x - a.x;
            if (w <= 1.0e-6f)
                return b.y;

            float f = juce::jlimit (0.0f, 1.0f, (phase - a.x) / w);
            if (std::abs (tensionExp - 1.0f) > 1.0e-4f)
                f = std::pow (f, tensionExp);
            return a.y + f * (b.y - a.y);
        }
    }
    return c.nodes[(size_t) c.count - 1].y;
}

void SkellgateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (bufferLength == 0)
        return;

    // MIDI preset switching: C1-B3 (24..59) = volume slot, C4+ (60..95) = time slot
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            if (note >= 24 && note < 60)       setVolumePreset (note - 24);
            else if (note >= 60 && note < 96)  setTimePreset (note - 60);
        }
    }

    // Pull in any curve edits
    {
        const int v = curveVersion.load();
        if (v != audioVersion)
        {
            const juce::SpinLock::ScopedLockType sl (curveLock);
            audioTimeCurve = timeCurve;
            audioVolCurve  = volCurve;
            audioVersion = v;
        }
    }

    // Parameters
    const float tMix  = timeMixParam->load() * 0.01f;
    const float vMix  = volMixParam->load()  * 0.01f;
    const float scratch = scratchParam->load();
    const bool  hold  = holdParam->load() > 0.5f;
    const float tensionExp = getTensionExponent();

    // ATT / REL: 0.2 ms floor (so hard gates never click) up to ~50 ms
    auto smoothingCoef = [this] (float amount)
    {
        const float a  = amount * 0.01f;
        const float ms = 0.2f + 50.0f * a * a;
        return std::exp (-1.0f / (ms * 0.001f * static_cast<float> (currentSampleRate)));
    };
    const float attCoef = smoothingCoef (attParam->load());
    const float relCoef = smoothingCoef (relParam->load());

    // Transport
    double bpm = 120.0;
    double ppq = freeRunPpq;
    if (auto* transport = getPlayHead())
    {
        if (auto pos = transport->getPosition())
        {
            if (auto b = pos->getBpm())
                bpm = *b;
            if (pos->getIsPlaying())
                if (auto p = pos->getPpqPosition())
                    ppq = *p;
        }
    }
    bpm = juce::jlimit (30.0, 300.0, bpm);
    currentBpm.store (bpm);

    const double ppqPerSample = bpm / 60.0 / currentSampleRate;
    const float patternSamples = static_cast<float> ((60.0 / bpm) * kPatternBeats * currentSampleRate);

    const int numCh = juce::jmin (numIn, numOut, 2);
    float* data[2] = { nullptr, nullptr };
    float* delay[2] = { nullptr, nullptr };
    for (int ch = 0; ch < numCh; ++ch)
    {
        data[ch]  = buffer.getWritePointer (ch);
        delay[ch] = delayBuffer.getWritePointer (ch);
    }

    float phase = heldPhase;

    for (int i = 0; i < numSamples; ++i)
    {
        if (! hold)
        {
            double ph = std::fmod ((ppq + i * ppqPerSample) / kPatternBeats + static_cast<double> (scratch), 1.0);
            if (ph < 0.0) ph += 1.0;
            phase = juce::jmin (static_cast<float> (ph), 0.99999f);
            heldPhase = phase;
        }
        else
            phase = heldPhase;

        // --- time lane: read position inside the pattern ---
        const float timeY = evaluateCurve (audioTimeCurve, phase, tensionExp);
        float delaySamples = (phase - timeY) * patternSamples;
        if (delaySamples < 0.0f)
            delaySamples += patternSamples;                  // "future" positions come from the previous pass
        delaySamples = juce::jlimit (0.0f, static_cast<float> (bufferLength - 2), delaySamples);

        // A normal curve moves the delay by (1 - slope) samples per sample, so only a jump
        // (slope > ~64x) trips this. Crossfade from where we were reading so it doesn't click.
        if (fadeCount == 0 && std::abs (delaySamples - prevDelay) > 64.0f)
        {
            fadeCount = fadeLen;
            fadeOldDelay = prevDelay;
        }
        prevDelay = delaySamples;

        const float fadeMix = fadeCount > 0 ? 1.0f - static_cast<float> (fadeCount) / static_cast<float> (fadeLen) : 1.0f;

        auto readDelayed = [this, wh = writeHead] (const float* buf, float delaySmp)
        {
            double readPos = static_cast<double> (wh) - static_cast<double> (delaySmp);
            if (readPos < 0.0) readPos += bufferLength;
            const int r1 = juce::jmin (static_cast<int> (readPos), bufferLength - 1);
            const int r2 = (r1 + 1) % bufferLength;
            const float frac = static_cast<float> (readPos - r1);
            return buf[r1] + frac * (buf[r2] - buf[r1]);
        };

        // --- volume lane ---
        const float volY = evaluateCurve (audioVolCurve, phase, tensionExp);
        const float targetGain = (1.0f - vMix) + volY * vMix;
        const float coef = targetGain > gainState ? attCoef : relCoef;
        gainState = targetGain + coef * (gainState - targetGain);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float dry = data[ch][i];
            delay[ch][writeHead] = dry;

            float warped = readDelayed (delay[ch], delaySamples);
            if (fadeCount > 0)
            {
                const float old = readDelayed (delay[ch], fadeOldDelay);
                warped = old + fadeMix * (warped - old);
            }
            const float timeOut = dry * (1.0f - tMix) + warped * tMix;
            data[ch][i] = timeOut * gainState;
        }

        if (fadeCount > 0)
            --fadeCount;

        if (++writeHead >= bufferLength)
            writeHead = 0;
    }

    currentPhase.store (phase);
    freeRunPpq = ppq + numSamples * ppqPerSample;
}

//==============================================================================
juce::AudioProcessorEditor* SkellgateAudioProcessor::createEditor()
{
    return new SkellgateAudioProcessorEditor (*this);
}

void SkellgateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("timePreset", currentTimePreset.load(), nullptr);
    state.setProperty ("volPreset",  currentVolumePreset.load(), nullptr);
    {
        const juce::SpinLock::ScopedLockType sl (curveLock);
        state.setProperty ("timeCurve", curveToString (timeCurve), nullptr);
        state.setProperty ("volCurve",  curveToString (volCurve), nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SkellgateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState == nullptr || ! xmlState->hasTagName (apvts.state.getType()))
        return;

    const auto tree = juce::ValueTree::fromXml (*xmlState);
    apvts.replaceState (tree);

    currentTimePreset.store (juce::jlimit (0, kNumPresets - 1, (int) tree.getProperty ("timePreset", 0)));
    currentVolumePreset.store (juce::jlimit (0, kNumPresets - 1, (int) tree.getProperty ("volPreset", 0)));

    {
        const juce::SpinLock::ScopedLockType sl (curveLock);
        Curve c;
        if (stringToCurve (tree.getProperty ("timeCurve").toString(), c))
            timeCurve = c;
        if (stringToCurve (tree.getProperty ("volCurve").toString(), c))
            volCurve = c;
    }
    ++curveVersion;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SkellgateAudioProcessor();
}
