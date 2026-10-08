#include "PluginProcessor.h"
#include "PluginEditor.h"

SkellgateAudioProcessor::SkellgateAudioProcessor()
     : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

SkellgateAudioProcessor::~SkellgateAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SkellgateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // All parameters scaled 0 to 100
    params.push_back(std::make_unique<juce::AudioParameterFloat>("attack", "Attack", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("release", "Release", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("length", "Length", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("time", "Time", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("output", "Output", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("reverse", "Reverse", false));

    return { params.begin(), params.end() };
}

void SkellgateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    // Allocate 4 seconds of buffer space for time stretching/delay history
    bufferSize = static_cast<int>(sampleRate * 4.0);
    delayBuffer.resize(bufferSize, 0.0f);
    writeHead = 0;
}

void SkellgateAudioProcessor::releaseResources()
{
}

bool SkellgateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void SkellgateAudioProcessor::nextPreset()
{
    currentPreset = (currentPreset + 1) % 3; // Cycles through 3 presets (0 = Default Pass-through)
}

std::vector<float> SkellgateAudioProcessor::getCurrentCurveValues(float phase)
{
    // Preset 0: Default flat pass-through (keeps signal identical)
    // Preset 1: Stutter effect
    // Preset 2: Jump/Warp effect
    float targetOffset = 0.0f;
    float targetGain = 1.0f;

    if (currentPreset == 1) {
        // Simple stutter gate pattern
        targetOffset = (std::fmod(phase * 4.0f, 1.0f)) * 5000.0f;
    } else if (currentPreset == 2) {
        // Reverse warp pattern
        targetOffset = (1.0f - phase) * 20000.0f;
    }

    return { targetOffset, targetGain };
}

void SkellgateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    jucescopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    bool isBypassed = *apvts.getRawParameterValue("bypass");
    if (isBypassed)
        return;

    float mixParam = *apvts.getRawParameterValue("mix") / 100.0f;
    float outParam = *apvts.getRawParameterValue("output") / 100.0f;

    auto* channelData = buffer.getWritePointer (0);
    int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float inSample = channelData[i];

        // Write to circular buffer
        delayBuffer[writeHead] = inSample;

        // Calculate a basic phase loop (mock 1-bar cycle)
        float phase = std::fmod((float)writeHead / (currentSampleRate * 1.0f), 1.0f);
        auto curve = getCurrentCurveValues(phase);

        int readHead = (writeHead - static_cast<int>(curve[0]) + bufferSize) % bufferSize;
        float outSample = delayBuffer[readHead] * curve[1];

        // Apply mix and output gain
        channelData[i] = (inSample * (1.0f - mixParam) + outSample * mixParam) * (outParam * 2.0f);

        writeHead = (writeHead + 1) % bufferSize;
    }

    // Mirror to right channel if stereo
    if (totalNumInputChannels > 1)
    {
        auto* rightData = buffer.getWritePointer(1);
        juce::FloatVectorOperations::copy(rightData, channelData, numSamples);
    }
}

void SkellgateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SkellgateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SkellgateAudioProcessor();
}
