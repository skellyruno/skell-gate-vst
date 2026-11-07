#pragma once
#include "Theme.h"
#include <array>
class SkellGlitchAudioProcessor;
class LoopRow : public juce::Component, private juce::Timer {
public:
    explicit LoopRow(SkellGlitchAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    SkellGlitchAudioProcessor& processor;
    std::array<std::unique_ptr<juce::TextButton>, 9> buttons;
};
