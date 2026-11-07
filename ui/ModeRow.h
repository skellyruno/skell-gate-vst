#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class ModeRow : public juce::Component, private juce::Timer {
public:
    explicit ModeRow(SkellGlitchAudioProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    void timerCallback() override;
    SkellGlitchAudioProcessor& processor;
    std::array<juce::TextButton, 3> modes;
    juce::ComboBox reverseStyle, gateRate;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> reverseAttach, gateAttach;
};
