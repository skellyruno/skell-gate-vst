#include "GlitchKnob.h"
GlitchKnob::GlitchKnob(const juce::String& label) : caption(label) {
    setName(label);
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters(juce::MathConstants<float>::pi * 1.24f,
                         juce::MathConstants<float>::pi * 2.76f, true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setPopupDisplayEnabled(false, false, nullptr);
}
