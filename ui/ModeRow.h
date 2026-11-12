#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class ModeRow : public juce::Component, private juce::Timer {
public:
    ModeRow(SkellGlitchAudioProcessor&, juce::TextButton& syncControl);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override;
    SkellGlitchAudioProcessor& processor;
    juce::TextButton& sync;
    std::array<juce::TextButton, 3> modes;
    juce::TextButton freeze{"FREEZE"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach;
    int displayedMode = -1;
};
