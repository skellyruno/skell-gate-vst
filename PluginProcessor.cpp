#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <limits>

namespace {
using P = juce::AudioProcessorValueTreeState;
using APID = juce::ParameterID;
// Bounded 32-second capture history halves the previous worst-case RAM footprint,
// including both aligned dry and gate histories at 96/192 kHz.
constexpr int maxHoldSeconds = 32;
const juce::StringArray loopLabels {"1/16", "1/8T", "1/8", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars", "8 Bars"};
float raw(const P& state, const char* name) { auto* value = state.getRawParameterValue(name); return value ? value->load() : 0.f; }
void writeParam(P& state, const juce::String& id, float real) {
    if (auto* p = state.getParameter(id)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(real));
        p->endChangeGesture();
    }
}
}

P::ParameterLayout SkellGlitchAudioProcessor::makeLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> v;
    auto boolean = [&](const char* id, const char* name, bool def) {
        v.push_back(std::make_unique<juce::AudioParameterBool>(APID{id, 1}, name, def));
    };
    auto choice = [&](const char* id, const char* name, juce::StringArray labels, int def) {
        v.push_back(std::make_unique<juce::AudioParameterChoice>(APID{id, 1}, name, labels, def));
    };
    auto number = [&](const char* id, const char* name, float lo, float hi, float def, float skew = 1.f, float interval = 0.f) {
        juce::NormalisableRange<float> range(lo, hi, interval, skew);
        v.push_back(std::make_unique<juce::AudioParameterFloat>(APID{id, 1}, name, range, def));
    };
    boolean("power", "Power", false);
    boolean("sync", "Tempo sync", true);
    choice("loopLength", "Loop length", loopLabels, 5);
    number("loopMs", "Free length", 10, 8000, 500);
    choice("mode", "Mode", {"Reverse", "Gate", "Both"}, 0);
    choice("reverseStyle", "Reverse style", {"Classic", "Tape", "PingPong", "Stutter"}, 0);
    number("fadeIn", "Fade in", 0, 500, 0);
    number("fadeOut", "Fade out", 0, 500, 0);
    number("smooth", "Smoothing", 0, 100, 30);
    number("bandLow", "Band low", 20, 20000, 20, .28f);
    number("bandHigh", "Band high", 20, 20000, 20000, .28f);
    boolean("bandInvert", "Invert band", false);
    number("mix", "Mix", 0, 100, 100);
    choice("gateRate", "Gate rate", {"1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T"}, 2);
    number("gateSteps", "Gate steps", 1, 32, 16, 1, 1);
    number("gateAttack", "Gate attack", .1f, 200, 3);
    number("gateRelease", "Gate release", .1f, 200, 15);
    choice("gateCurve", "Gate curve", {"Linear", "Exponential", "S-Curve"}, 1);
    number("gateDepth", "Gate closed level dB", -96, -24, -96);
    number("gateSwing", "Gate swing", 0, 75, 0);
    for (int i = 1; i <= 32; ++i) {
        auto id = "step" + juce::String(i);
        v.push_back(std::make_unique<juce::AudioParameterChoice>(APID{id + "_state", 1},
            "Step " + juce::String(i) + " state", juce::StringArray{"Off", "On", "Accent", "Reverse"}, 1));
        v.push_back(std::make_unique<juce::AudioParameterFloat>(APID{id + "_prob", 1},
            "Step " + juce::String(i) + " probability", juce::NormalisableRange<float>(0.f, 100.f, 1.f), 100.f));
    }
    number("chaos", "Chaos", 0, 100, 0);
    number("seed", "Random seed", 0, 1000000, 271828, 1, 1);
    boolean("seedLock", "Seed lock", true);
    number("reversePitch", "Reverse pitch", -12, 12, 0);
    boolean("freeze", "Freeze", false);
    boolean("trigger", "Manual trigger", false);
    choice("triggerSource", "Trigger source", {"Auto", "MIDI", "Sidechain", "Manual"}, 0);
    choice("latencyMode", "Latency mode", {"Zero", "Aligned"}, 0);
    boolean("deltaListen", "Delta listen", false);
    return {std::make_move_iterator(v.begin()), std::make_move_iterator(v.end())};
}

SkellGlitchAudioProcessor::SkellGlitchAudioProcessor()
 : juce::AudioProcessor(BusesProperties()
       .withInput("Input", juce::AudioChannelSet::stereo(), true)
       .withOutput("Output", juce::AudioChannelSet::stereo(), true)
       .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
   apvts(*this, &undo, "SKELL_GLITCH_STATE", makeLayout()) {
    for (auto& x : waveform) x.store(0.f);
    for (int i = 0; i < 32; ++i) {
        const auto key = "step" + juce::String(i + 1);
        statePointers[static_cast<size_t>(i)] = apvts.getRawParameterValue(key + "_state");
        probabilityPointers[static_cast<size_t>(i)] = apvts.getRawParameterValue(key + "_prob");
    }
    apvts.state.setProperty("windowSize", 1, nullptr);  // 75%: half the size of old 150% screenshot
    apvts.state.setProperty("uiScaleVersion", 2, nullptr);
    apvts.state.setProperty("presetName", "INIT", nullptr);
    apvts.state.setProperty("gateFirst", false, nullptr);
    apvts.state.setProperty("titleJitter", true, nullptr);
}
SkellGlitchAudioProcessor::~SkellGlitchAudioProcessor() { cancelPendingUpdate(); }

bool SkellGlitchAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out || (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())) return false;
    const auto sc = layouts.getChannelSet(true, 1);
    return sc.isDisabled() || sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo();
}

void SkellGlitchAudioProcessor::prepareToPlay(double sr, int) {
    currentSampleRate = std::isfinite(sr) && sr > 0.0 ? sr : 48000.0;
    clock.prepare(currentSampleRate);
    const int maximum = static_cast<int>(std::ceil(currentSampleRate * maxHoldSeconds));
    capture.prepare(getMainBusNumInputChannels(), maximum);
    splitter.prepare(currentSampleRate);
    triggers.prepare(currentSampleRate);
    for (auto& g : shapers) g.prepare(currentSampleRate);
    delayCapacity = maximum + 4096;
    const int activeChannels = std::clamp(getMainBusNumInputChannels(), 1, 2);
    for (auto* array : {&dryDelay, &outsideDelay, &gateDelay})
        for (int ch = 0; ch < 2; ++ch) {
            auto& history = (*array)[static_cast<size_t>(ch)];
            if (ch < activeChannels) history.assign(static_cast<size_t>(delayCapacity), 0.f);
            else { history.clear(); history.shrink_to_fit(); }
        }
    sampleCounter = 0;
    lastSlice = INT64_MIN;
    lastWet.fill(0.f);
    heldWet.fill(0.f);
    mixSmoothed.reset(currentSampleRate, .015);
    mixSmoothed.setCurrentAndTargetValue(raw(apvts, "mix") / 100.f);
    bypassSmoothed.reset(currentSampleRate, .010);
    bypassSmoothed.setCurrentAndTargetValue(raw(apvts, "power") >= .5f ? 1.f : 0.f);
    lowSmoothed.reset(currentSampleRate, .030);
    highSmoothed.reset(currentSampleRate, .030);
    lowSmoothed.setCurrentAndTargetValue(raw(apvts, "bandLow"));
    highSmoothed.setCurrentAndTargetValue(raw(apvts, "bandHigh"));
    desiredLatency.store(0);
    displayLatency.store(0);
    setLatencySamples(0);
}
void SkellGlitchAudioProcessor::releaseResources() {}
void SkellGlitchAudioProcessor::requestLatencyRefresh() { triggerAsyncUpdate(); }
void SkellGlitchAudioProcessor::handleAsyncUpdate() {
    const int latency = desiredLatency.load(std::memory_order_acquire);
    if (latency != getLatencySamples()) setLatencySamples(latency);
    displayLatency.store(getLatencySamples());
}
void SkellGlitchAudioProcessor::publishWaveform() {
    const int count = capture.length();
    if (count <= 0) {
        for (auto& bin : waveform) bin.store(0.f, std::memory_order_relaxed);
        return;
    }
    for (size_t bin = 0; bin < waveform.size(); ++bin) {
        float peak = 0.f;
        for (int k = 0; k < 8; ++k) {
            const double idx = 3.0 + (count - 6.0) * (static_cast<double>(bin) + k / 8.0) / waveform.size();
            peak = std::max(peak, std::abs(capture.readFinished(0, idx)));
        }
        waveform[bin].store(peak, std::memory_order_relaxed);
    }
}

void SkellGlitchAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int samples = buffer.getNumSamples();
    const int channels = std::min(2, getMainBusNumOutputChannels());
    if (samples <= 0 || channels == 0 || delayCapacity == 0) return;
    const bool sync = raw(apvts, "sync") >= .5f;
    const auto sidechain = getBusBuffer(buffer, true, 1);
    const bool hasSidechain = sidechain.getNumChannels() > 0;
    double tempo = 120, ppq = 0;
    int numerator = 4, denominator = 4;
    bool hostValid = false, playing = false;
    if (auto* head = getPlayHead()) if (auto position = head->getPosition()) {
        if (const auto b = position->getBpm()) tempo = *b;
        if (const auto sig = position->getTimeSignature()) { numerator = sig->numerator; denominator = sig->denominator; }
        if (const auto p = position->getPpqPosition()) { ppq = *p; hostValid = true; }
        playing = position->getIsPlaying();
    }
    displayTransport.store(playing ? 1 : 0, std::memory_order_relaxed);
    const int sourceBeforeClock = static_cast<int>(raw(apvts, "triggerSource"));
    int loopIndex = static_cast<int>(raw(apvts, "loopLength"));
    if (sourceBeforeClock == skell::dsp::TriggerRouter::MIDI && triggers.active())
        loopIndex = std::clamp(triggers.note() - 36, 0, 8);
    clock.setHost(tempo, numerator, denominator, ppq, hostValid && playing, sync,
                  loopIndex, raw(apvts, "loopMs"));
    const auto sliceSamples = std::max(16, static_cast<int>(std::round(clock.getSamplesPerSlice())));
    const int requestedLatency = static_cast<int>(raw(apvts, "latencyMode")) == 0 ? 0 : std::min(sliceSamples, delayCapacity - 8);
    if (requestedLatency != desiredLatency.load(std::memory_order_relaxed)) {
        desiredLatency.store(requestedLatency, std::memory_order_release);
        triggerAsyncUpdate();
    }
    const int latency = std::clamp(getLatencySamples(), 0, delayCapacity - 8);
    displayLatency.store(latency, std::memory_order_relaxed);
    const int mode = static_cast<int>(raw(apvts, "mode"));
    const int style = static_cast<int>(raw(apvts, "reverseStyle"));
    const int rate = static_cast<int>(raw(apvts, "gateRate"));
    const int steps = std::clamp(static_cast<int>(raw(apvts, "gateSteps")), 1, 32);
    const float swing = raw(apvts, "gateSwing") / 100.f;
    const float attack = raw(apvts, "gateAttack"), release = raw(apvts, "gateRelease");
    const int curve = static_cast<int>(raw(apvts, "gateCurve"));
    const float closedGain = raw(apvts, "gateDepth") <= -95.f ? 0.f :
         std::pow(10.f, raw(apvts, "gateDepth") / 20.f);
    const float mix = raw(apvts, "mix") * .01f;
    const float smooth = raw(apvts, "smooth") * .01f;
    const float fadeIn = raw(apvts, "fadeIn") * static_cast<float>(currentSampleRate) / 1000.f;
    const float fadeOut = raw(apvts, "fadeOut") * static_cast<float>(currentSampleRate) / 1000.f;
    const float smoothingSamples = smooth * .025f * static_cast<float>(currentSampleRate);
    const float pitch = raw(apvts, "reversePitch");
    const auto pitchContext = skell::dsp::Reverser::makePitchContext(
        pitch, static_cast<int>(currentSampleRate));
    const float chaos = raw(apvts, "chaos");
    const int seed = static_cast<int>(raw(apvts, "seed"));
    const bool seedLock = raw(apvts, "seedLock") >= .5f;
    const bool invert = raw(apvts, "bandInvert") >= .5f;
    const bool freeze = raw(apvts, "freeze") >= .5f;
    const bool delta = raw(apvts, "deltaListen") >= .5f;
    const int source = static_cast<int>(raw(apvts, "triggerSource"));
    const bool manual = raw(apvts, "trigger") >= .5f;
    const bool aligned = latency > 0;
    const bool bothGateFirst = gateFirst.load(std::memory_order_relaxed);
    mixSmoothed.setTargetValue(mix);
    bypassSmoothed.setTargetValue(raw(apvts, "power") >= .5f ? 1.f : 0.f);
    lowSmoothed.setTargetValue(raw(apvts, "bandLow"));
    highSmoothed.setTargetValue(raw(apvts, "bandHigh"));
    for (auto& shaper : shapers) shaper.setTimes(attack, release);
    constexpr double stepQuarters[] = {1.0, .5, .25, .125, 1.0 / 3.0, 1.0 / 6.0};
    const double gateCycleQuarter = stepQuarters[std::clamp(rate, 0, 5)] * steps;
    for (int step = 0; step < 32; ++step) {
        sequencer.setStep(step, static_cast<int>(statePointers[static_cast<size_t>(step)]->load()),
                          probabilityPointers[static_cast<size_t>(step)]->load());
    }
    // JUCE 8 range iterator: no deprecated MidiBuffer::Iterator, and
    // note-on timing remains exact at its sample position within the block.
    auto midiIt = midi.begin();
    const auto midiEnd = midi.end();
    // Gate probability is stable within a step: hash once per step transition,
    // not for every incoming sample and channel.
    int previousGateStep = -1;
    int64_t previousGateCycle = INT64_MIN;
    float cachedGateTarget = 1.f;
    for (int i = 0; i < samples; ++i) {
        while (midiIt != midiEnd && (*midiIt).samplePosition <= i) {
            const auto event = *midiIt;
            ++midiIt;
            if (source == skell::dsp::TriggerRouter::MIDI) {
                const auto message = event.getMessage();
                if (message.isNoteOn()) {
                    triggers.trigger(message.getFloatVelocity(), message.getNoteNumber());
                    loopIndex = std::clamp(message.getNoteNumber() - 36, 0, 8);
                    clock.setHost(tempo, numerator, denominator, ppq, hostValid && playing, sync,
                                  loopIndex, raw(apvts, "loopMs"));
                }
            }
        }
        if (source == skell::dsp::TriggerRouter::Manual && manual && i == 0) triggers.trigger();
        const auto position = clock.tick(i);
        if (position.boundary) {
            // Continue smoothly from the previous rendered wet sample rather
            // than fading every new reverse slice down to absolute silence.
            const bool timelineJump = lastSlice != INT64_MIN && position.sliceIndex != lastSlice + 1;
            heldWet = timelineJump ? std::array<float, 2>{0.f, 0.f} : lastWet;
            if (!freeze) capture.onBoundary();
            if (timelineJump && !freeze) capture.clear();
            lastSlice = position.sliceIndex;
            publishWaveform();
            if (mode != 1) flipCounter.fetch_add(1, std::memory_order_relaxed);
            const auto bar = static_cast<int64_t>(std::floor(position.beat / std::max(1.0, clock.getBarLength())));
            chaosEvent = randomizer.choose(position.sliceIndex, seed, chaos, seedLock, bar);
        }
        const double absoluteGateCycle = position.beat / gateCycleQuarter;
        const int64_t gateCycleIndex = static_cast<int64_t>(std::floor(absoluteGateCycle));
        const int step = sequencer.stepAt(absoluteGateCycle - std::floor(absoluteGateCycle), steps, swing);
        displayStep.store(step, std::memory_order_relaxed);
        displayPhase.store(static_cast<float>(position.phase), std::memory_order_relaxed);
        if (source == skell::dsp::TriggerRouter::Sidechain && hasSidechain)
            triggers.processSidechain(hasSidechain && sidechain.getNumChannels() > 1
                ? .5f * (std::abs(sidechain.getSample(0, i)) + std::abs(sidechain.getSample(1, i)))
                : sidechain.getSample(0, i));
        const bool eventOn = source == skell::dsp::TriggerRouter::Auto || triggers.active() || freeze;
        if (step != previousGateStep || gateCycleIndex != previousGateCycle) {
            cachedGateTarget = sequencer.target(step, gateCycleIndex, seed, closedGain);
            previousGateStep = step;
            previousGateCycle = gateCycleIndex;
        }
        float gateTarget = eventOn ? cachedGateTarget : 1.f;
        if (source == skell::dsp::TriggerRouter::MIDI && eventOn)
            gateTarget = closedGain + (gateTarget - closedGain) * triggers.velocity();
        const bool stepReverse = sequencer.isReverseStep(step) && cachedGateTarget > closedGain;
        const bool needsReverse = eventOn && (mode != 1 || stepReverse ||
            chaosEvent == skell::dsp::GlitchRandomizer::Flip ||
            chaosEvent == skell::dsp::GlitchRandomizer::Stutter ||
            chaosEvent == skell::dsp::GlitchRandomizer::HalfTime);
        displayReverse.store(needsReverse ? 1 : 0, std::memory_order_relaxed);
        float inside[2]{}, outside[2]{}, dry[2]{};
        float shaped[2]{};
        const float low = lowSmoothed.getNextValue();
        const float high = highSmoothed.getNextValue();
        // Filter coefficients only update when a meaningful cutoff change occurs.
        if (i == 0 || (i % 32) == 0) splitter.setCuts(std::min(low, high), std::max(low, high));
        for (int ch = 0; ch < channels; ++ch) {
            const float incoming = buffer.getSample(ch, i);
            // Guard IIR state against invalid upstream samples.
            dry[ch] = std::isfinite(incoming) ? incoming : 0.f;
            splitter.process(ch, dry[ch], inside[ch], outside[ch]);
            if (invert) std::swap(inside[ch], outside[ch]);
            shaped[ch] = shapers[static_cast<size_t>(ch)].next(gateTarget, curve);
        }
        const auto activeSliceSamples = std::max(16.0, clock.getSamplesPerSlice());
        // User-specified fades still operate normally; SMOOTH now performs a
        // real transition from the previous wet output at slice onset.
        const float phaseEnv = skell::dsp::Crossfader::boundaryEnvelope(position.phase, activeSliceSamples,
                      std::min(static_cast<double>(fadeIn), activeSliceSamples * .5),
                      std::min(static_cast<double>(fadeOut), activeSliceSamples * .5), 0.0);
        const double transitionSamples = std::min(static_cast<double>(smoothingSamples), activeSliceSamples * .5);
        const double transitionPhase = transitionSamples > 0.0
            ? position.phase * activeSliceSamples / transitionSamples : 1.0;
        const int delayPos = static_cast<int>(sampleCounter % delayCapacity);
        const int readPos = static_cast<int>((sampleCounter - latency + delayCapacity) % delayCapacity);
        const double phase = chaosEvent == skell::dsp::GlitchRandomizer::HalfTime ? position.phase * .5 : position.phase;
        // Compute the crossfade trigonometry once, then reuse for L/R.
        const auto mixGains = skell::dsp::Crossfader::unityGains(mixSmoothed.getCurrentValue());
        const float bypassAmount = bypassSmoothed.getCurrentValue();
        for (int ch = 0; ch < channels; ++ch) {
             const float delayedDry = latency ? dryDelay[ch][static_cast<size_t>(readPos)] : dry[ch];
            const float delayedOutside = latency ? outsideDelay[ch][static_cast<size_t>(readPos)] : outside[ch];
            dryDelay[ch][static_cast<size_t>(delayPos)] = dry[ch];
            outsideDelay[ch][static_cast<size_t>(delayPos)] = outside[ch];
            const float liveGate = (mode == 1 || mode == 2) ? inside[ch] * shaped[ch] : inside[ch];
            const float delayedGate = latency ? gateDelay[ch][static_cast<size_t>(readPos)] : liveGate;
            gateDelay[ch][static_cast<size_t>(delayPos)] = liveGate;
            float wet = mode == 1 ? delayedGate : inside[ch];
            if (needsReverse && capture.length() > 0) {
                const auto effectiveStyle = chaosEvent == skell::dsp::GlitchRandomizer::Stutter ? 3 : style;
                wet = reverser.samplePrepared(capture, ch, phase, effectiveStyle,
                                               position.sliceIndex, pitchContext) * phaseEnv;
                if (chaosEvent == skell::dsp::GlitchRandomizer::Flip)
                    wet = capture.readFinished(ch, phase * std::max(0, capture.length() - 1)) * phaseEnv;
                if (chaosEvent == skell::dsp::GlitchRandomizer::Skip) wet = 0.f;
                if ((mode == 2 && !bothGateFirst) || mode == 1) wet *= shaped[ch];
            } else if (mode != 1) wet = aligned ? delayedGate : liveGate;
            if (eventOn && chaosEvent == skell::dsp::GlitchRandomizer::Skip) wet = 0.f;
            if (needsReverse && capture.length() > 0 && transitionPhase < 1.0)
                wet = skell::dsp::Crossfader::spliceHold(heldWet[static_cast<size_t>(ch)],
                                                        wet, static_cast<float>(transitionPhase));
            lastWet[static_cast<size_t>(ch)] = wet;
            const float wetFull = delayedOutside + wet;
            const float blended = delayedDry * mixGains.dry + wetFull * mixGains.wet;
            const float processed = delta ? blended - delayedDry : blended;
            buffer.setSample(ch, i, delayedDry + (processed - delayedDry) * bypassAmount);
        }
        const float recL = bothGateFirst && mode == 2 ? inside[0] * shaped[0] : inside[0];
        const float recR = bothGateFirst && mode == 2 ? (channels > 1 ? inside[1] * shaped[1] : recL) :
                                                     (channels > 1 ? inside[1] : recL);
        if (!freeze) capture.write(recL, recR);
        mixSmoothed.getNextValue();
        bypassSmoothed.getNextValue();
        triggers.tick();
        ++sampleCounter;
    }
    clock.advance(samples);
    // Never write into additional input-only buses: a host may expose sidechain channels
    // through read-only memory in the process buffer.
}

void SkellGlitchAudioProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi) {
    // Double-precision host path uses a reusable float buffer, allocated here only if a
    // host specifically enables double processing (not advertised by this processor).
    juce::AudioBuffer<float> tmp(buffer.getNumChannels(), buffer.getNumSamples());
    for (int ch = 0; ch < tmp.getNumChannels(); ++ch)
        for (int i = 0; i < tmp.getNumSamples(); ++i) tmp.setSample(ch, i, static_cast<float>(buffer.getSample(ch, i)));
    processBlock(tmp, midi);
    for (int ch = 0; ch < tmp.getNumChannels(); ++ch)
        for (int i = 0; i < tmp.getNumSamples(); ++i) buffer.setSample(ch, i, tmp.getSample(ch, i));
}

void SkellGlitchAudioProcessor::getStateInformation(juce::MemoryBlock& data) {
    auto snapshot = apvts.copyState();
    snapshot.setProperty("gateFirst", gateFirst.load(), nullptr);
    snapshot.setProperty("titleJitter", titleJitter.load(), nullptr);
    for (int i = 0; i < 2; ++i) if (ab[static_cast<size_t>(i)].isValid()) {
        auto xml = ab[static_cast<size_t>(i)].createXml();
        snapshot.setProperty(i == 0 ? "abA" : "abB", xml->toString(), nullptr);
    }
    if (auto xml = snapshot.createXml()) copyXmlToBinary(*xml, data);
}
void SkellGlitchAudioProcessor::setStateInformation(const void* data, int bytes) {
    if (auto xml = getXmlFromBinary(data, bytes)) {
        const auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid() && tree.hasType(apvts.state.getType())) {
            gateFirst.store(static_cast<bool>(tree.getProperty("gateFirst", false)));
            titleJitter.store(static_cast<bool>(tree.getProperty("titleJitter", true)));
            for (int i = 0; i < 2; ++i) {
                const juce::String slot = tree.getProperty(i == 0 ? "abA" : "abB").toString();
                if (auto slotXml = juce::parseXML(slot)) ab[static_cast<size_t>(i)] = juce::ValueTree::fromXml(*slotXml);
            }
            apvts.replaceState(tree);
        }
    }
}
float SkellGlitchAudioProcessor::parameter(const char* name) const { return raw(apvts, name); }
void SkellGlitchAudioProcessor::setParameter(const char* name, float value) { writeParam(apvts, name, value); }
int SkellGlitchAudioProcessor::getSavedSize() const {
    // Older sessions stored a different size index mapping. Open those at the
    // new compact 75% default instead of unexpectedly reopening at XL.
    if (!apvts.state.hasProperty("uiScaleVersion")) return 1;
    return juce::jlimit(0, 3, static_cast<int>(apvts.state.getProperty("windowSize", 1)));
}
void SkellGlitchAudioProcessor::setSavedSize(int v) {
    apvts.state.setProperty("windowSize", juce::jlimit(0, 3, v), nullptr);
    apvts.state.setProperty("uiScaleVersion", 2, nullptr);
}
juce::String SkellGlitchAudioProcessor::currentPresetName() const { return apvts.state.getProperty("presetName", "INIT").toString(); }
void SkellGlitchAudioProcessor::setPresetName(const juce::String& name) { apvts.state.setProperty("presetName", name, nullptr); }
void SkellGlitchAudioProcessor::setPattern(const std::array<int, 16>& pattern) {
    for (int i = 0; i < 16; ++i) writeParam(apvts, "step" + juce::String(i + 1) + "_state", static_cast<float>(pattern[static_cast<size_t>(i)]));
}
juce::StringArray SkellGlitchAudioProcessor::factoryPresetNames() {
    return {"Classic Reverse Bar", "Half-Time Flip", "Trap Chop", "Triplet Gate", "Rage Stutter",
            "Tape Reverse Swell", "Ping-Pong Glitch", "Offbeat Pump", "Vocal Tail Flip",
            "Drum Buss Chop", "Snare Reverse Lead-In", "INIT"};
}
void SkellGlitchAudioProcessor::loadFactoryPreset(int index) {
    index = juce::jlimit(0, 11, index);
    // Reset all parameters to their declared defaults, then apply the preset changes.
    for (auto* p : getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p)) {
        r->beginChangeGesture();
        r->setValueNotifyingHost(r->getDefaultValue());
        r->endChangeGesture();
    }
    std::array<int, 16> pattern{}; pattern.fill(1);
    if (index != 11) setParameter("power", 1);
    switch (index) {
    case 0: break;
    case 1: setParameter("loopLength", 4); setParameter("smooth", 42); break;
    case 2: setParameter("mode", 2); setParameter("gateRate", 2); for (int i = 0; i < 16; ++i) pattern[i] = i % 4 == 0 ? 2 : (i % 2 ? 0 : 1); break;
    case 3: setParameter("mode", 1); setParameter("gateRate", 4); for (int i = 0; i < 16; ++i) pattern[i] = i % 3 == 0 ? 2 : 0; break;
    case 4: setParameter("mode", 2); setParameter("reverseStyle", 3); setParameter("smooth", 8); break;
    case 5: setParameter("reverseStyle", 1); setParameter("fadeIn", 110); setParameter("fadeOut", 70); break;
    case 6: setParameter("reverseStyle", 2); setParameter("loopLength", 5); break;
    case 7: setParameter("mode", 1); setParameter("gateRate", 1); for (int i = 0; i < 16; ++i) pattern[i] = (i & 1) ? 1 : 0; break;
    case 8: setParameter("loopLength", 2); setParameter("mix", 45); setParameter("fadeIn", 16); setParameter("fadeOut", 60); break;
    case 9: setParameter("mode", 2); setParameter("smooth", 60); setParameter("mix", 55); for (int i = 0; i < 16; ++i) pattern[i] = i % 4 == 0 ? 2 : 1; break;
    case 10: setParameter("loopLength", 1); setParameter("smooth", 15); setParameter("mix", 66); break;
    case 11: setParameter("power", 0); break;
    }
    setPattern(pattern);
    setPresetName(factoryPresetNames()[index]);
}
void SkellGlitchAudioProcessor::snapshotAB(int slot) {
    if (slot < 0 || slot > 1) return;
    ab[static_cast<size_t>(slot)] = apvts.copyState().createCopy();
}
void SkellGlitchAudioProcessor::recallAB(int slot) {
    if (slot < 0 || slot > 1 || !ab[static_cast<size_t>(slot)].isValid()) return;
    apvts.replaceState(ab[static_cast<size_t>(slot)].createCopy());
}
void SkellGlitchAudioProcessor::copyAB(int source, int destination) {
    if (source < 0 || source > 1 || destination < 0 || destination > 1 || !ab[static_cast<size_t>(source)].isValid()) return;
    ab[static_cast<size_t>(destination)] = ab[static_cast<size_t>(source)].createCopy();
}
juce::File SkellGlitchAudioProcessor::userPresetFolder() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Skelly/Skell-Glitch/Presets");
}
bool SkellGlitchAudioProcessor::saveUserPreset(const juce::File& file, const juce::String& name) {
    auto state = apvts.copyState();
    state.setProperty("presetName", name, nullptr);
    if (auto xml = state.createXml()) { file.getParentDirectory().createDirectory(); return xml->writeTo(file); }
    return false;
}
bool SkellGlitchAudioProcessor::loadUserPreset(const juce::File& file) {
    if (auto xml = juce::XmlDocument::parse(file)) {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid() && tree.hasType(apvts.state.getType())) {
            apvts.replaceState(tree);
            return true;
        }
    }
    return false;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SkellGlitchAudioProcessor(); }
