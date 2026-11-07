#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class BandSlider : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    explicit BandSlider(SkellGlitchAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    float frequencyToX(float) const;
    float xToFrequency(float) const;
    void change(float x);
    SkellGlitchAudioProcessor& processor;
    int selected = 0, hovered = -1;
    float dragStartX = 0.f, dragStartFrequency = 20.f;
    float lastLow = -1.f, lastHigh = -1.f;
};
