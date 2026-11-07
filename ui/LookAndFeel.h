#pragma once
#include "Theme.h"
class SkellLookAndFeel : public juce::LookAndFeel_V4 {
public:
    SkellLookAndFeel();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void drawPopupMenuBackground(juce::Graphics&, int, int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
};
