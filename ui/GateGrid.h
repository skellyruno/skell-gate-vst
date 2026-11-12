#pragma once
#include "Theme.h"
#include <cstdint>
#include <atomic>
class SkellGlitchAudioProcessor;
class GateGrid : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    explicit GateGrid(SkellGlitchAudioProcessor&);
    void paint(juce::Graphics&) override;
    void setGlitchBurst(int intensity, std::uint32_t variant) {
        if (glitchIntensity != intensity || glitchVariant != variant) {
            glitchIntensity = intensity;
            glitchVariant = variant;
            repaint();
        }
    }
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
    void showSequencerMenu();
    void applyPattern(int operation);
    void setStep(int index, int value);
    void loadPattern(int index);
    SkellGlitchAudioProcessor& processor;
    int glitchIntensity = 0;
    std::uint32_t glitchVariant = 0;
    std::array<std::atomic<float>*, 32> stateValues{}, probabilityValues{};
    std::array<float, 32> lastStates{}, lastProbabilities{};
    int lastPaintedStep = -1, lastStepCount = -1;
    int edited = -1, drawValue = 1, hoveredStep = -1;
    int activeProbabilityStep = -1;
    bool probabilityMode = false, probabilityOnBody = false;
};
