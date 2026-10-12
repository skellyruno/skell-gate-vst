#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <deque>

class SkellgateAudioProcessor : public juce::AudioProcessor
{
public:
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
    
    void setTimePreset(int index);
    void setVolumePreset(int index);
    int getCurrentTimePreset() const { return currentTimePreset; }
    int getCurrentVolumePreset() const { return currentVolumePreset; }
    float getCurrentPhase() const { return currentPhase; }
    float getCurrentAudioLevel() const { return currentAudioLevel; }

    struct Node { float x; float y; };
    std::vector<Node>& getTimeCurveNodes() { return timeCurveNodes; }
    std::vector<Node>& getVolumeCurveNodes() { return volumeCurveNodes; }
    void updateNodePosition(bool isTimeLane, int index, float newY);

    enum ActiveLane { LaneTime, LaneVolume };
    ActiveLane activeViewLane = LaneTime;
    void setActiveViewLane(ActiveLane lane) { activeViewLane = lane; }
    ActiveLane getActiveViewLane() const { return activeViewLane; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::vector<float> delayBuffer;
    int bufferSize = 0;
    int writeHead = 0;
    double currentSampleRate = 44100.0;
    float currentPhase = 0.0f;
    float currentAudioLevel = 0.0f;

    int currentTimePreset = 0;
    int currentVolumePreset = 0;

    std::vector<Node> timeCurveNodes;
    std::vector<Node> volumeCurveNodes;

    float evaluateSmoothCurve(const std::vector<Node>& nodes, float phase);
    float applySaturation(float sample, float drive);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkellgateAudioProcessor)
};
