#include "PluginProcessor.h"
#include "PluginEditor.h"

SkellgateAudioProcessor::SkellgateAudioProcessor()
     : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
{
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
    params.push_back(std::make_unique<juce::AudioParameterFloat>("output", "Output", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 75.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("reverse", "Reverse", false));

    return { params.begin(), params.end() };
}

void SkellgateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    bufferSize = static_cast<int>(sampleRate * 4.0); // 4 seconds buffer memory
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
    currentPreset = (currentPreset + 1) % 4; // Cycles through 4 unique rhythmic presets
}

SkellgateAudioProcessor::CurvePoint SkellgateAudioProcessor::evaluateCurve(float phase)
{
    float targetOffset = 0.0f;
    float targetGain = 1.0f;
    float timeMultiplier = *apvts.getRawParameterValue("time") / 50.0f;
    bool isReverse = *apvts.getRawParameterValue("reverse");

    float adjustedPhase = std::fmod(phase * timeMultiplier, 1.0f);

    if (currentPreset == 1) {
        // Stutter Gate Pattern
        targetOffset = std::floor(adjustedPhase * 8.0f) * 2000.0f;
        targetGain = (std::fmod(adjustedPhase * 8.0f, 1.0f) < 0.7f) ? 1.0f : 0.0f;
    } else if (currentPreset == 2) {
        // Tape Stop / Pitch Warp
        targetOffset = (adjustedPhase * adjustedPhase) * 15000.0f;
    } else if (currentPreset == 3) {
        // Octave Jump / Reverse Stutter
        targetOffset = (1.0f - adjustedPhase) * 8000.0f;
        targetGain = 0.8f + 0.2f * std::sin(adjustedPhase * 30.0f);
    }

    if (isReverse) {
        targetOffset = -targetOffset;
    }

    return { targetOffset, targetGain };
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
    float outParam = *apvts.getRawParameterValue("output") / 100.0f;
    float lengthParam = *apvts.getRawParameterValue("length");
    
    // Calculate loop duration in samples based on Length knob (0-100 mapped to 0.25 to 4 seconds)
    double loopDurationSecs = 0.25 + (lengthParam / 100.0f) * 3.75;

    auto* channelData = buffer.getWritePointer (0);
    int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float inSample = channelData[i];
        delayBuffer[writeHead] = inSample;

        currentPhase = static_cast<float>(std::fmod(static_cast<double>(writeHead) / (currentSampleRate * loopDurationSecs), 1.0));
        CurvePoint cp = evaluateCurve(currentPhase);

        // Linear interpolation for smooth, artifact-free buffer reading
        double exactReadPos = static_cast<double>(writeHead) - static_cast<double>(cp.offset);
        while (exactReadPos < 0) exactReadPos += bufferSize;
        while (exactReadPos >= bufferSize) exactReadPos -= bufferSize;

        int readIndex1 = static_cast<int>(exactReadPos);
        int readIndex2 = (readIndex1 + 1) % bufferSize;
        float frac = static_cast<float>(exactReadPos - readIndex1);

        float sample1 = delayBuffer[readIndex1];
        float sample2 = delayBuffer[readIndex2];
        float interpolatedSample = sample1 + frac * (sample2 - sample1);

        float outSample = interpolatedSample * cp.gain;

        channelData[i] = (inSample * (1.0f - mixParam) + outSample * mixParam) * (outParam * 1.5f);

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
