#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

/**
    Skellgate - a time / volume envelope effect in the style of FL Studio's Gross Beat.

    A "pattern" is 2 beats long and locked to the host transport. Two envelope curves run
    over the pattern:
      * TIME   : y is the read position (0..1 of the pattern) inside the audio recorded
                 during the pattern. y == phase is normal playback, flat is a freeze,
                 a falling line is reverse, a repeated saw is a stutter.
      * VOLUME : y is the gain (0..1). Gates, tremolos, fades, sidechain-style ducks...
*/
class SkellgateAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kMaxNodes   = 128;
    static constexpr int kNumPresets = 36;
    static constexpr double kPatternBeats = 2.0;

    struct Node  { float x = 0.0f; float y = 0.0f; };
    struct Curve
    {
        std::array<Node, kMaxNodes> nodes {};
        int count = 0;
    };

    enum ActiveLane { LaneTime = 0, LaneVolume = 1 };

    SkellgateAudioProcessor();
    ~SkellgateAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- Presets (safe to call from the message thread or from processBlock) ----
    void setTimePreset (int index);
    void setVolumePreset (int index);
    int getCurrentTimePreset() const   { return currentTimePreset.load(); }
    int getCurrentVolumePreset() const { return currentVolumePreset.load(); }
    float getCurrentPhase() const      { return currentPhase.load(); }

    // ---- Curve access for the editor (message thread) ----
    Curve getCurve (bool isTimeLane) const;
    void  moveNode (bool isTimeLane, int index, float x, float y);
    int   addNode (bool isTimeLane, float x, float y);   // returns new index or -1
    void  removeNode (bool isTimeLane, int index);

    void setActiveViewLane (ActiveLane lane) { activeLane.store (static_cast<int> (lane)); }
    ActiveLane getActiveViewLane() const     { return static_cast<ActiveLane> (activeLane.load()); }

    static const char* getTimePresetName (int index);
    static const char* getVolumePresetName (int index);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static float evaluateCurve (const Curve& curve, float phase, float tensionExp);

    // Shared (editor-owned) curves, protected by curveLock
    mutable juce::SpinLock curveLock;
    Curve timeCurve, volCurve;
    std::atomic<int> curveVersion { 0 };

    // Audio-thread copies
    Curve audioTimeCurve, audioVolCurve;
    int audioVersion = -1;

    // Cached parameter pointers
    std::atomic<float>* timeMixParam = nullptr;
    std::atomic<float>* volMixParam  = nullptr;
    std::atomic<float>* attParam     = nullptr;
    std::atomic<float>* relParam     = nullptr;
    std::atomic<float>* tnsParam     = nullptr;
    std::atomic<float>* scratchParam = nullptr;
    std::atomic<float>* holdParam    = nullptr;

    // DSP state
    juce::AudioBuffer<float> delayBuffer;
    int bufferLength = 0;
    int writeHead = 0;
    double currentSampleRate = 44100.0;
    double freeRunPpq = 0.0;
    float heldPhase = 0.0f;
    float gainState = 1.0f;

    std::atomic<float> currentPhase { 0.0f };
    std::atomic<int> currentTimePreset { 0 };
    std::atomic<int> currentVolumePreset { 0 };
    std::atomic<int> activeLane { LaneTime };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkellgateAudioProcessor)
};
