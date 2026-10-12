#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

static const char* kTimeNames[36] = {
    "Empty", "Basic 1", "Basic 2", "Basic 3",
    "Basic 4", "Basic 5", "Basic 6", "Basic 7",
    "Basic 8", "Basic 9", "Basic 10", "Basic 11",
    "Complex 1", "Complex 2", "Complex 3", "Complex 4",
    "Complex 5", "Complex 6", "Complex 7", "Complex 8",
    "Complex 9", "Complex 10", "Complex 11", "Complex 12",
    "Chaos 1", "Chaos 2", "Chaos 3", "Chaos 4",
    "Chaos 5", "Chaos 6", "Chaos 7", "Chaos 8",
    "Chaos 9", "Chaos 10", "Chaos 11", "Chaos 12"
};

static const char* kVolNames[36] = {
    "Empty", "2 Beat Gate", "1 Beat Gate", "1/2 Bt Gate",
    "1/3 Bt Gate", "1/4 Bt Gate", "1/6 Bt Gate", "1/8 Bt Gate",
    "Trance Gt 1", "Trance Gt 2", "Trance Gt 3", "Trance Gt 4",
    "Saw Gate 1", "Saw Gate 2", "1st Step", "1st Stp Fade",
    "2 Step Gate", "Off Beat", "1/4 Dynamic", "1/4 Swing",
    "1/4 Gate Out", "1/4 Gate In", "Spd Up Gate", "Slw Dn Gate",
    "End Fade", "End Gate", "Tremelo Slw", "Tremelo Fst",
    "Sidechain", "Drum Loop", "Copter", "AM",
    "Fade In", "Fade Out", "Fade Out In", "Mute"
};

const char* SkellgateAudioProcessor::getTimePresetName(int index) { return kTimeNames[juce::jlimit(0, 35, index)]; }
const char* SkellgateAudioProcessor::getVolumePresetName(int index) { return kVolNames[juce::jlimit(0, 35, index)]; }

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

    params.push_back(std::make_unique<juce::AudioParameterFloat>("timeMix", "Time Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("volMix", "Volume Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("att", "Attack", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("rel", "Release", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("tns", "Tension", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("scratch", "Scratch", juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("hold", "Hold", false));

    return { params.begin(), params.end() };
}

void SkellgateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate = sampleRate;
    bufferSize = static_cast<int>(sampleRate * 4.0); // 2-bar rolling buffer
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

void SkellgateAudioProcessor::setTimePreset(int index)
{
    currentTimePreset = juce::jlimit(0, 35, index);
    timeCurveNodes.clear();

    switch (currentTimePreset)
    {
        case 0: timeCurveNodes = { {0.0f, 0.0f}, {1.0f, 0.0f} }; break; // Empty Pass
        case 1: timeCurveNodes = { {0.0f, 0.0f}, {0.5f, 0.5f}, {1.0f, 1.0f} }; break; // Basic 1 (1/2 Speed)
        case 2: timeCurveNodes = { {0.0f, 0.0f}, {0.25f, 0.25f}, {0.5f, 0.5f}, {0.75f, 0.75f}, {1.0f, 1.0f} }; break; // Basic 2
        case 3: timeCurveNodes = { {0.0f, 0.0f}, {0.125f, 0.5f}, {0.25f, 0.0f}, {0.375f, 0.5f}, {0.5f, 0.0f}, {0.75f, 0.5f}, {1.0f, 0.0f} }; break; // Basic 3 (1/8 Stutter)
        case 7: timeCurveNodes = { {0.0f, 0.0f}, {0.125f, 0.125f}, {0.25f, 0.0f}, {0.375f, 0.375f}, {0.5f, 0.0f}, {0.625f, 0.625f}, {0.75f, 0.0f}, {0.875f, 0.875f}, {1.0f, 0.0f} }; break; // Basic 7
        default:
            timeCurveNodes = { {0.0f, 0.0f}, {0.25f, 0.6f}, {0.5f, 0.1f}, {0.75f, 0.8f}, {1.0f, 0.0f} }; break;
    }
}

void SkellgateAudioProcessor::setVolumePreset(int index)
{
    currentVolumePreset = juce::jlimit(0, 35, index);
    volumeCurveNodes.clear();

    switch (currentVolumePreset)
    {
        case 0: volumeCurveNodes = { {0.0f, 1.0f}, {1.0f, 1.0f} }; break; // Empty (Full Vol)
        case 1: volumeCurveNodes = { {0.0f, 1.0f}, {0.9f, 1.0f}, {0.9001f, 0.0f}, {1.0f, 0.0f} }; break; // 2 Beat Gate
        case 2: volumeCurveNodes = { {0.0f, 1.0f}, {0.45f, 1.0f}, {0.5f, 0.0f}, {1.0f, 0.0f} }; break; // 1 Beat Gate
        case 3: volumeCurveNodes = { {0.0f, 1.0f}, {0.22f, 1.0f}, {0.25f, 0.0f}, {0.47f, 1.0f}, {0.5f, 0.0f}, {1.0f, 0.0f} }; break; // 1/2 Bt Gate
        case 4: // 1/3 Bt Gate (Triplets)
            for (int i = 0; i < 24; ++i) {
                float x1 = i * (1.0f / 24.0f);
                float yVal = (i % 2 == 0) ? 1.0f : 0.0f;
                volumeCurveNodes.push_back({ x1, yVal });
            }
            break;
        case 5: // 1/4 Bt Gate
            for (int i = 0; i < 16; ++i) {
                float x1 = i * (1.0f / 16.0f);
                float yVal = (i % 2 == 0) ? 1.0f : 0.0f;
                volumeCurveNodes.push_back({ x1, yVal });
            }
            break;
        case 7: // 1/8 Bt Gate
            for (int i = 0; i < 32; ++i) {
                float x1 = i * (1.0f / 32.0f);
                float yVal = (i % 2 == 0) ? 1.0f : 0.0f;
                volumeCurveNodes.push_back({ x1, yVal });
            }
            break;
        default:
            volumeCurveNodes = { {0.0f, 1.0f}, {0.25f, 0.0f}, {0.5f, 1.0f}, {0.75f, 0.0f}, {1.0f, 1.0f} }; break;
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

float SkellgateAudioProcessor::evaluateCurve(const std::vector<Node>& nodes, float phase)
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
    return y1 + factor * (y2 - y1);
}

void SkellgateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Gross Beat MIDI Slot Switching Rules (C1-B3 = Vol, C4+ = Time)
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            int note = msg.getNoteNumber();
            if (note >= 24 && note < 60) setVolumePreset(note - 24);
            else if (note >= 60) setTimePreset(note - 60);
        }
    }

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    float tMix = *apvts.getRawParameterValue("timeMix") / 100.0f;
    float vMix = *apvts.getRawParameterValue("volMix") / 100.0f;
    float scratch = *apvts.getRawParameterValue("scratch");
    bool isHold = *apvts.getRawParameterValue("hold");

    double loopDurationSecs = 2.0; // Standard 2-bar Gross Beat duration
    auto* channelData = buffer.getWritePointer (0);
    int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float inSample = channelData[i];
        delayBuffer[writeHead] = inSample;

        if (!isHold)
        {
            double phaseDouble = std::fmod((static_cast<double>(writeHead) / (currentSampleRate * loopDurationSecs)) + static_cast<double>(scratch), 1.0);
            if (phaseDouble < 0.0) phaseDouble += 1.0;
            currentPhase = static_cast<float>(phaseDouble);
        }

        // Time Warp Processing
        float timeVal = evaluateCurve(timeCurveNodes, currentPhase);
        float timeOffsetSamples = timeVal * static_cast<float>(currentSampleRate * 2.0);

        double exactReadPos = static_cast<double>(writeHead) - static_cast<double>(timeOffsetSamples);
        while (exactReadPos < 0) exactReadPos += bufferSize;
        while (exactReadPos >= bufferSize) exactReadPos -= bufferSize;

        int r1 = static_cast<int>(exactReadPos);
        int r2 = (r1 + 1) % bufferSize;
        float frac = static_cast<float>(exactReadPos - r1);

        float timeSample = delayBuffer[r1] + frac * (delayBuffer[r2] - delayBuffer[r1]);
        float wetTimeSample = inSample * (1.0f - tMix) + timeSample * tMix;

        // Volume Envelope Gate Processing
        float volVal = evaluateCurve(volumeCurveNodes, currentPhase);
        float finalSample = wetTimeSample * ((1.0f - vMix) + volVal * vMix);

        channelData[i] = finalSample;
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
