#pragma once
#include "Theme.h"
#include <atomic>
class SkellGlitchAudioProcessor;
class GateGrid : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    explicit GateGrid(SkellGlitchAudioProcessor&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void resized() override;
private:
    void timerCallback() override;
    void edit(const juce::MouseEvent&);
    void finishProbabilityGesture();
    void applyPattern(int operation);
    void setStep(int index, int value);
    void loadPattern(int index);
    SkellGlitchAudioProcessor& processor;
    std::array<juce::TextButton, 5> commands;
    juce::ComboBox pattern;
    std::array<std::atomic<float>*, 32> stateValues{}, probabilityValues{};
    std::array<float, 32> lastStates{}, lastProbabilities{};
    int lastPaintedStep = -1, lastStepCount = -1;
    int edited = -1, drawValue = 1, hoveredStep = -1;
    int activeProbabilityStep = -1;
    bool probabilityMode = false, probabilityOnBody = false;
};
