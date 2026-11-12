#pragma once
#include "Theme.h"
class GlitchKnob : public juce::Slider {
public:
    explicit GlitchKnob(const juce::String& label);
    const juce::String& getCaption() const { return caption; }
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void paintOverChildren(juce::Graphics&) override;
private:
    juce::String caption;
    bool hovered = false, dragging = false;
};
