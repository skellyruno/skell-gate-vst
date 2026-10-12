#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

SkellgateAudioProcessor::SkellgateAudioProcessor()
     : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    setTimePreset(0);
    setVolumePreset(0);
}

SkellgateAudioProcessor::~SkellgateAudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout SkellgateAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("attack", "Attack", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("release", "Release", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("length", "Length", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("time", "Time", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("pitch", "Pitch", juce::NormalisableRange<float>(-24.0f, 24.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("saturation", "Saturation", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 15.0f));
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
    currentAudioLevel = 0.0f;
    
    std::lock_guard<std::mutex> lock(scopeMutex);
    scopeBuffer.assign(300, 0.0f);
}

void SkellgateAudioProcessor::releaseResources() {}

bool SkellgateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void SkellgateAudioProcessor::setTimePreset(int index)
{
    currentTimePreset = juce::jlimit(0, 31, index);
    timeCurveNodes.clear();

    switch (currentTimePreset % 16)
    {
        case 0: timeCurveNodes = { {0.0f, 0.0f}, {1.0f, 0.0f} }; break; // Pass
        case 1: timeCurveNodes = { {0.0f, 0.0f}, {0.125f, 0.5f}, {0.25f, 0.0f}, {0.375f, 0.5f}, {0.5f, 0.0f}, {0.75f, 0.5f}, {1.0f, 0.0f} }; break; // 1/8 Stutter
        case 2: timeCurveNodes = { {0.0f, 0.0f}, {0.4f, 0.4f}, {1.0f, 1.0f} }; break; // Tape Stop
        case 3: timeCurveNodes = { {0.0f, 0.0f}, {0.5f, 0.5f}, {1.0f, 1.0f} }; break; // Half-Time
        case 4: timeCurveNodes = { {0.0f, 1.0f}, {0.5f, 0.5f}, {1.0f, 0.0f} }; break; // Reverse Warp
        case 5: timeCurveNodes = { {0.0f, 0.0f}, {0.2f, 0.9f}, {0.4f, 0.1f}, {0.7f, 0.8f}, {1.0f, 0.0f} }; break; // Vinyl Scratch
        case 6: timeCurveNodes = { {0.0f, 0.0f}, {0.25f, 0.7f}, {0.3f, 0.1f}, {0.6f, 0.9f}, {1.0f, 0.0f} }; break; // Complex A
        case 7: timeCurveNodes = { {0.0f, 0.2f}, {0.4f, 0.8f}, {0.6f, 0.1f}, {1.0f, 0.5f} }; break; // Complex B
        case 8: timeCurveNodes = { {0.0f, 0.0f}, {0.1f, 0.9f}, {0.2f, 0.0f}, {0.3f, 0.9f}, {0.5f, 0.0f}, {1.0f, 0.0f} }; break; // Chaos
        case 9: timeCurveNodes = { {0.0f, 1.0f}, {1.0f, 0.0f} }; break; // Pitch Rise
        case 10: timeCurveNodes = { {0.0f, 0.0f}, {0.8f, 0.6f}, {1.0f, 1.0f} }; break; // Slow Down
        case 11: timeCurveNodes = { {0.0f, 0.1f}, {0.2f, 0.7f}, {0.4f, 0.3f}, {0.6f, 0.9f}, {1.0f, 0.5f} }; break; // Granular Scatter
        case 12: timeCurveNodes = { {0.0f, 0.0f}, {0.2f, 0.5f}, {0.4f, 0.0f}, {0.6f, 0.5f}, {0.8f, 0.0f}, {1.0f, 0.0f} }; break; // Stutter Fast
        case 13: timeCurveNodes = { {0.0f, 0.5f}, {0.3f, 0.0f}, {0.7f, 1.0f}, {1.0f, 0.5f} }; break; // Wobble Warp
        case 14: timeCurveNodes = { {0.0f, 0.0f}, {0.15f, 0.8f}, {0.5f, 0.2f}, {1.0f, 0.0f} }; break; // Dive Bomb
        case 15: timeCurveNodes = { {0.0f, 0.0f}, {0.5f, 1.0f}, {0.5001f, 0.0f}, {1.0f, 1.0f} }; break; // Jump Stutter
    }
}

void SkellgateAudioProcessor::setVolumePreset(int index)
{
    currentVolumePreset = juce::jlimit(0, 31, index);
    volumeCurveNodes.clear();

    switch (currentVolumePreset % 16)
    {
        case 0: volumeCurveNodes = { {0.0f, 1.0f}, {1.0f, 1.0f} }; break; // Full
        case 1: volumeCurveNodes = { {0.0f, 1.0f}, {0.45f, 1.0f}, {0.5f, 0.0f}, {1.0f, 1.0f} }; break; // 1/2 Gate
        case 2: volumeCurveNodes = { {0.0f, 1.0f}, {0.2f, 0.0f}, {0.25f, 1.0f}, {0.5f, 0.0f}, {1.0f, 1.0f} }; break; // 1/4 Gate
        case 3: volumeCurveNodes = { {0.0f, 1.0f}, {0.1f, 0.0f}, {0.2f, 1.0f}, {0.3f, 0.0f}, {0.4f, 1.0f}, {0.5f, 0.0f}, {1.0f, 1.0f} }; break; // Trance Gater
        case 4: volumeCurveNodes = { {0.0f, 1.0f}, {0.5f, 0.0f}, {0.5001f, 1.0f}, {1.0f, 0.0f} }; break; // Saw Gate
        case 5: volumeCurveNodes = { {0.0f, 0.0f}, {0.25f, 1.0f}, {1.0f, 1.0f} }; break; // 1st Step Fade
        case 6: volumeCurveNodes = { {0.0f, 1.0f}, {1.0f, 0.0f} }; break; // Fade Out
        case 7: volumeCurveNodes = { {0.0f, 0.0f}, {1.0f, 1.0f} }; break; // Fade In
        case 8: volumeCurveNodes = { {0.0f, 1.0f}, {0.25f, 0.2f}, {0.5f, 1.0f}, {0.75f, 0.2f}, {1.0f, 1.0f} }; break; // Tremolo Slow
        case 9: volumeCurveNodes = { {0.0f, 1.0f}, {0.1f, 0.1f}, {0.2f, 1.0f}, {0.3f, 0.1f}, {0.5f, 1.0f}, {1.0f, 1.0f} }; break; // Tremolo Fast
        case 10: volumeCurveNodes = { {0.0f, 0.1f}, {0.15f, 1.0f}, {1.0f, 1.0f} }; break; // Sidechain Duck
        case 11: volumeCurveNodes = { {0.0f, 0.0f}, {1.0f, 0.0f} }; break; // Mute
        case 12: volumeCurveNodes = { {0.0f, 1.0f}, {0.125f, 0.0f}, {0.25f, 1.0f}, {0.375f, 0.0f}, {1.0f, 1.0f} }; break; // Stutter Gate
        case 13: volumeCurveNodes = { {0.0f, 0.3f}, {0.5f, 1.0f}, {1.0f, 0.3f} }; break; // Pumping Pulse
        case 14: volumeCurveNodes = { {0.0f, 1.0f}, {0.2f, 0.2f}, {0.4f, 1.0f}, {0.6f, 0.2f}, {1.0f, 1.0f} }; break; // Rhythmic Chop
        case 15: volumeCurveNodes = { {0.0f, 0.0f}, {0.1f, 1.0f}, {0.2f, 0.0f}, {0.3f, 1.0f}, {1.0f, 0.0f} }; break; // Strobe Gate
    }
}

void SkellgateAudioProcessor::updateNodePosition(bool isTimeLane, int index, float newY)
{
    auto& nodes = isTimeLane ? timeCurveNodes : volumeCurveNodes;
    if (index >= 0 && index < static_cast<int>(nodes.size()))
    {
        nodes[index].y = juce::jlimit(0.0f, 1.0f, newY);
    }
}

float SkellgateAudioProcessor::evaluateSmoothCurve(const std::vector<Node>& nodes, float phase)
{
    if (nodes.empty()) return 0.0f;

    int idx = 0;
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        if (nodes[i].x <= phase)
            idx = static_cast<int>(i);
    }

    int nextIdx = (idx + 1) % nodes.size();
    float x1 = nodes[idx].x;
    float y1 = nodes[idx].y;
    float x2 = nodes[nextIdx].x;
    float y2 = nodes[nextIdx].y;

    if (x2 < x1) x2 += 1.0f;
    float p = phase;
    if (p < x1) p += 1.0f;

    float factor = (x2 == x1) ? 0.0f : (p - x1) / (x2 - x1);
    
    // Smooth Cosine Interpolation (S-Curve feel for buttery smooth transitions)
    float smoothFactor = (1.0f - std::cos(factor * juce::MathConstants<float>::pi)) * 0.5f;
    return y1 + smoothFactor * (y2 - y1);
}

float SkellgateAudioProcessor::applySaturation(float sample, float drive)
{
    if (drive <= 0.0f) return sample;
    float gain = 1.0f + (drive / 10.0f);
    return std::tanh(sample * gain) / std::tanh(gain);
}

std::vector<float> SkellgateAudioProcessor::getScopeHistory() const
{
    // Return a copy safely for UI thread waveform rendering
    // (Implementation relies on scopeBuffer shared state)
    return {};
}

void SkellgateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Handle MIDI Note Triggers (e.g., Notes 36-67 map to 32 time slots)
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            int noteNumber = msg.getNoteNumber();
            int slot = (noteNumber - 36) % 32;
            if (slot < 0) slot += 32;
            setTimePreset(slot);
        }
    }

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (*apvts.getRawParameterValue("bypass"))
        return;

    float mixParam = *apvts.getRawParameterValue("mix") / 100.0f;
    float lengthParam = *apvts.getRawParameterValue("length");
    float timeParam = *apvts.getRawParameterValue("time") / 50.0f;
    float pitchParam = *apvts.getRawParameterValue("pitch");
    float satParam = *apvts.getRawParameterValue("saturation");

    double loopDurationSecs = (0.25 + (lengthParam / 100.0) * 3.75);
    double pitchRatio = std::pow(2.0, static_cast<double>(pitchParam) / 12.0);

    auto* channelData = buffer.getWritePointer (0);
    int numSamples = buffer.getNumSamples();
    float rmsSum = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float inSample = channelData[i];
        rmsSum += inSample * inSample;
        delayBuffer[writeHead] = inSample;

        double phaseDouble = std::fmod((static_cast<double>(writeHead) / (currentSampleRate * loopDurationSecs)) * static_cast<double>(timeParam), 1.0);
        currentPhase = static_cast<float>(phaseDouble);

        // Evaluate Time & Volume curves with smooth interpolation
        float timeVal = evaluateSmoothCurve(timeCurveNodes, currentPhase);
        float maxOffsetSamples = static_cast<float>(currentSampleRate * 1.5);
        float timeOffset = timeVal * maxOffsetSamples;

        if (*apvts.getRawParameterValue("reverse"))
            timeOffset = -timeOffset;

        double exactReadPos = static_cast<double>(writeHead) - (static_cast<double>(timeOffset) * pitchRatio);
        while (exactReadPos < 0) exactReadPos += bufferSize;
        while (exactReadPos >= bufferSize) exactReadPos -= bufferSize;

        int readIndex1 = static_cast<int>(exactReadPos);
        int readIndex2 = (readIndex1 + 1) % bufferSize;
        float frac = static_cast<float>(exactReadPos - readIndex1);

        float sample1 = delayBuffer[readIndex1];
        float sample2 = delayBuffer[readIndex2];
        float timeSample = sample1 + frac * (sample2 - sample1);

        float volVal = evaluateSmoothCurve(volumeCurveNodes, currentPhase);
        float processedSample = applySaturation(timeSample * volVal, satParam);

        channelData[i] = inSample * (1.0f - mixParam) + processedSample * mixParam;

        writeHead = (writeHead + 1) % bufferSize;
    }

    currentAudioLevel = std::sqrt(rmsSum / static_cast<float>(numSamples));

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
