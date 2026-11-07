#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class GateGrid : public juce::Component, private juce::Timer {
public:
    explicit GateGrid(SkellGlitchAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void resized() override;
private:
    void timerCallback() override { repaint(); }
    void edit(const juce::MouseEvent&);
    void applyPattern(int operation);
    void setStep(int index, int value);
    void loadPattern(int index);
    SkellGlitchAudioProcessor& processor;
    std::array<juce::TextButton, 5> commands;
    juce::ComboBox pattern;
    int edited = -1, drawValue = 1;
    bool probabilityMode = false;
};
