#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class WaveDisplay : public juce::Component, private juce::Timer {
public:
    explicit WaveDisplay(SkellGlitchAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    void timerCallback() override { repaint(); }
    void editAt(const juce::MouseEvent&);
    SkellGlitchAudioProcessor& processor;
    float zoom = 1.f;
    bool paintOn = true;
    int lastEdited = -1;
};
