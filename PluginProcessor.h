#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "dsp/TempoClock.h"
#include "dsp/SliceCapture.h"
#include "dsp/Reverser.h"
#include "dsp/GateSequencer.h"
#include "dsp/GateShaper.h"
#include "dsp/BandSplitter.h"
#include "dsp/Crossfader.h"
#include "dsp/TriggerRouter.h"
#include "dsp/GlitchRandomizer.h"

class SkellGlitchAudioProcessor final : public juce::AudioProcessor, private juce::AsyncUpdater {
public:
    SkellGlitchAudioProcessor();
    ~SkellGlitchAudioProcessor() override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Skell-Glitch"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "User"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;
    std::array<std::atomic<float>, 160> waveform{};
    std::atomic<float> displayPhase{0.f};
    std::atomic<int> displayStep{0}, displayReverse{0}, displayTransport{0}, displayLatency{0};
    std::atomic<uint64_t> flipCounter{0};
    std::atomic<bool> gateFirst{false};
    std::atomic<bool> titleJitter{true};
    int getSavedSize() const;
    void setSavedSize(int);
    juce::String currentPresetName() const;
    void setPresetName(const juce::String&);
    void loadFactoryPreset(int index);
    static juce::StringArray factoryPresetNames();
    void snapshotAB(int slot);
    void recallAB(int slot);
    void copyAB(int source, int destination);
    bool saveUserPreset(const juce::File&, const juce::String&);
    bool loadUserPreset(const juce::File&);
    static juce::File userPresetFolder();
    void requestLatencyRefresh();

private:
    void handleAsyncUpdate() override;
    void publishWaveform();
    float parameter(const char*) const;
    void setParameter(const char*, float);
    void setPattern(const std::array<int, 16>& pattern);
    skell::dsp::TempoClock clock;
    skell::dsp::SliceCapture capture;
    skell::dsp::Reverser reverser;
    skell::dsp::GateSequencer sequencer;
    std::array<skell::dsp::GateShaper, 2> shapers;
    skell::dsp::BandSplitter splitter;
    skell::dsp::TriggerRouter triggers;
    skell::dsp::GlitchRandomizer randomizer;
    std::array<std::atomic<float>*, 32> statePointers{};
    std::array<std::atomic<float>*, 32> probabilityPointers{};
    std::array<std::vector<float>, 2> dryDelay, outsideDelay, gateDelay;
    int delayCapacity = 0;
    int64_t sampleCounter = 0;
    int64_t lastSlice = INT64_MIN;
    skell::dsp::GlitchRandomizer::Event chaosEvent = skell::dsp::GlitchRandomizer::Normal;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed, bypassSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> lowSmoothed, highSmoothed;
    std::atomic<int> desiredLatency{0};
    double currentSampleRate = 48000.0;
    std::array<juce::ValueTree, 2> ab;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellGlitchAudioProcessor)
};
