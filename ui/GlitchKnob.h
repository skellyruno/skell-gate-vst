#pragma once
#include "Theme.h"
class GlitchKnob : public juce::Slider {
public:
    explicit GlitchKnob(const juce::String& label);
    const juce::String& getCaption() const { return caption; }
    void mouseDoubleClick(const juce::MouseEvent&) override;
private:
    juce::String caption;
};
