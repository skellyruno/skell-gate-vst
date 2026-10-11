#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

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

    bool acceptsMidi() const override { return false; }
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
    
    void nextPreset();
    void previousPreset();
    void loadPreset(int index);
    int getCurrentPresetIndex() const { return currentPreset; }
    float getCurrentPhase() const { return currentPhase; }
    float getCurrentAudioLevel() const { return currentAudioLevel; }

    struct Node { float x; float y; };
    std::vector<Node>& getCurveNodes() { return curveNodes; }
    void updateNodePosition(int index, float newY);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::vector<float> delayBuffer;
    int bufferSize = 0;
    int writeHead = 0;
    double currentSampleRate = 44100.0;
    float currentPhase = 0.0f;
    float currentAudioLevel = 0.0f;

    int currentPreset = 0;
    std::vector<Node> curveNodes;
    float evaluateCustomCurve(float phase);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkellgateAudioProcessor)
};
