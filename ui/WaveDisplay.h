#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class WaveDisplay : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    explicit WaveDisplay(SkellGlitchAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void timerCallback() override;
    void editAt(const juce::MouseEvent&);
    SkellGlitchAudioProcessor& processor;
    float zoom = 1.f;
    bool paintOn = true;
    int lastEdited = -1;
    uint64_t lastFlip = UINT64_MAX;
    int lastReverse = -1, lastStep = -1;
    float lastPhase = -1.f;
};
