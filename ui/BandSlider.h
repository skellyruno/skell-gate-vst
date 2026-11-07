#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class BandSlider : public juce::Component, private juce::Timer {
public:
    explicit BandSlider(SkellGlitchAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    float frequencyToX(float) const;
    float xToFrequency(float) const;
    void change(float x);
    SkellGlitchAudioProcessor& processor;
    int selected = 0;
};
