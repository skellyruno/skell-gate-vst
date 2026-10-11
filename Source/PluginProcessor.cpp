#include "PluginProcessor.h"
#include "PluginEditor.h"

SkellgateAudioProcessor::SkellgateAudioProcessor()
     : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    loadPreset(0);
}

SkellgateAudioProcessor::~SkellgateAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout SkellgateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("attack", "Attack", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("release", "Release", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("length", "Length", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("time", "Time", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("reverse", "Reverse", false));

    return { params.begin(), params.end() };
}

void SkellgateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    bufferSize = static_cast<int>(sampleRate * 4.0);
    delayBuffer.resize(bufferSize, 0.0f);
    writeHead = 0;
    currentPhase = 0.0f;
}

void SkellgateAudioProcessor::releaseResources() {}

bool SkellgateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void SkellgateAudioProcessor::nextPreset()
{
    currentPreset = (currentPreset + 1) % 8;
    loadPreset(currentPreset);
}

void SkellgateAudioProcessor::previousPreset()
{
    currentPreset = (currentPreset - 1 + 8) % 8;
    loadPreset(currentPreset);
}

void SkellgateAudioProcessor::loadPreset(int index)
{
    currentPreset = juce::jlimit(0, 7, index);
    curveNodes.clear();

    switch (currentPreset)
    {
        case 0: // Pass-Through
            curveNodes = { {0.0f, 0.0f}, {1.0f, 0.0f} };
            break;
        case 1: // 1/8 Stutter Gate
            curveNodes = { {0.0f, 0.0f}, {0.125f, 0.5f}, {0.25f, 0.0f}, {0.375f, 0.5f}, {0.5f, 0.0f}, {0.625f, 0.5f}, {0.75f, 0.0f}, {0.875f, 0.5f}, {1.0f, 0.0f} };
            break;
        case 2: // Tape Stop
            curveNodes = { {0.0f, 0.0f}, {0.5f, 0.4f}, {1.0f, 1.0f} };
            break;
        case 3: // Half-Time Stretch
            curveNodes = { {0.0f, 0.0f}, {0.5f, 0.5f}, {1.0f, 1.0f} };
            break;
        case 4: // Reverse Sweep
            curveNodes = { {0.0f, 1.0f}, {0.5f, 0.5f}, {1.0f, 0.0f} };
            break;
        case 5: // Vinyl Scratch
            curveNodes = { {0.0f, 0.0f}, {0.2f, 0.8f}, {0.4f, 0.1f}, {0.6f, 0.9f}, {0.8f, 0.2f}, {1.0f, 0.0f} };
            break;
        case 6: // Granular Scatter
            curveNodes = { {0.0f, 0.1f}, {0.2f, 0.7f}, {0.4f, 0.3f}, {0.6f, 0.9f}, {0.8f, 0.2f}, {1.0f, 0.5f} };
            break;
        case 7: // Double-Time Glitch
            curveNodes = { {0.0f, 0.0f}, {0.25f, 0.9f}, {0.5f, 0.0f}, {0.75f, 0.9f}, {1.0f, 0.0f} };
            break;
    }
}

void SkellgateAudioProcessor::updateNodePosition(int index, float newY)
{
    if (index >= 0 && index < static_cast<int>(curveNodes.size()))
    {
        curveNodes[index].y = juce::jlimit(0.0f, 1.0f, newY);
    }
}

float SkellgateAudioProcessor::evaluateCustomCurve(float phase)
{
    if (curveNodes.empty()) return 0.0f;

    int idx = 0;
    for (size_t i = 0; i < curveNodes.size(); ++i)
    {
        if (curveNodes[i].x <= phase)
            idx = static_cast<int>(i);
    }

    int nextIdx = (idx + 1) % curveNodes.size();
    float x1 = curveNodes[idx].x;
    float y1 = curveNodes[idx].y;
    float x2 = curveNodes[nextIdx].x;
    float y2 = curveNodes[nextIdx].y;

    if (x2 < x1) x2 += 1.0f;
    float p = phase;
    if (p < x1) p += 1.0f;

    float factor = (x2 == x1) ? 0.0f : (p - x1) / (x2 - x1);
    float interpolatedY = y1 + factor * (y2 - y1);

    float maxOffsetSamples = static_cast<float>(currentSampleRate * 1.0);
    float offset = interpolatedY * maxOffsetSamples;

    if (*apvts.getRawParameterValue("reverse"))
        offset = -offset;

    return offset;
}

void SkellgateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (*apvts.getRawParameterValue("bypass"))
        return;

    float mixParam = *apvts.getRawParameterValue("mix") / 100.0f;
    float lengthParam = *apvts.getRawParameterValue("length");
    float timeParam = *apvts.getRawParameterValue("time") / 50.0f;

    double loopDurationSecs = (0.25 + (lengthParam / 100.0) * 3.75);

    auto* channelData = buffer.getWritePointer (0);
    int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float inSample = channelData[i];
        delayBuffer[writeHead] = inSample;

        // Clean double-to-float phase calculation avoiding C4244 warnings
        double phaseDouble = std::fmod((static_cast<double>(writeHead) / (currentSampleRate * loopDurationSecs)) * static_cast<double>(timeParam), 1.0);
        currentPhase = static_cast<float>(phaseDouble);

        float delayOffsetSamples = evaluateCustomCurve(currentPhase);

        double exactReadPos = static_cast<double>(writeHead) - static_cast<double>(delayOffsetSamples);
        while (exactReadPos < 0) exactReadPos += bufferSize;
        while (exactReadPos >= bufferSize) exactReadPos -= bufferSize;

        int readIndex1 = static_cast<int>(exactReadPos);
        int readIndex2 = (readIndex1 + 1) % bufferSize;
        float frac = static_cast<float>(exactReadPos - readIndex1);

        float sample1 = delayBuffer[readIndex1];
        float sample2 = delayBuffer[readIndex2];
        float outSample = sample1 + frac * (sample2 - sample1);

        channelData[i] = inSample * (1.0f - mixParam) + outSample * mixParam;

        writeHead = (writeHead + 1) % bufferSize;
    }

    if (totalNumInputChannels > 1)
    {
        auto* rightData = buffer.getWritePointer(1);
        juce::FloatVectorOperations::copy(rightData, channelData, numSamples);
    }
}

juce::AudioProcessorEditor* SkellgateAudioProcessor::createEditor()
{
    return new SkellgateAudioProcessorEditor (*this);
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
