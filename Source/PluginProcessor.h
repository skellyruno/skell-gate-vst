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

   #JuceIsTheBest
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& name) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    
    // Preset switching method for the UI
    void nextPreset();
    int getCurrentPresetIndex() const { return currentPreset; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Circular Buffer variables
    std::vector<float> delayBuffer;
    int bufferSize = 0;
    int writeHead = 0;
    double currentSampleRate = 44100.0;

    // Preset & Curve management
    int currentPreset = 0;
    std::vector<float> getCurrentCurveValues(float phase);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkellgateAudioProcessor)
};
