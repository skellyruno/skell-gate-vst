#include "GlitchKnob.h"
GlitchKnob::GlitchKnob(const juce::String& label) : caption(label) {
    setName(label);
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters(juce::MathConstants<float>::pi * 1.24f,
                         juce::MathConstants<float>::pi * 2.76f, true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setTooltip("Drag to adjust " + caption + ". Hold Shift for precision; double-click to reset." +
               ((caption == "FADE IN" || caption == "FADE OUT")
                   ? juce::String(" The small dot is the actual value marker.") : juce::String()));
    setPopupDisplayEnabled(false, false, nullptr);
}
void GlitchKnob::mouseDoubleClick(const juce::MouseEvent& e) {
    juce::Slider::mouseDoubleClick(e);
}

void GlitchKnob::mouseEnter(const juce::MouseEvent& e) {
    hovered = true;
    juce::Slider::mouseEnter(e);
    repaint();
}
void GlitchKnob::mouseExit(const juce::MouseEvent& e) {
    hovered = false;
    juce::Slider::mouseExit(e);
    repaint();
}
void GlitchKnob::mouseDown(const juce::MouseEvent& e) {
    dragging = true;
    juce::Slider::mouseDown(e);
    repaint();
}
void GlitchKnob::mouseDrag(const juce::MouseEvent& e) {
    juce::Slider::mouseDrag(e);
    repaint();
}
void GlitchKnob::mouseUp(const juce::MouseEvent& e) {
    juce::Slider::mouseUp(e);
    dragging = false;
    repaint();
}
void GlitchKnob::paintOverChildren(juce::Graphics& g) {
    if (!hovered && !dragging) return;
    const double value = getValue();
    const bool fade = caption == "FADE IN" || caption == "FADE OUT";
    const auto text = fade
        ? (value < .5 ? juce::String("INST") : juce::String(juce::roundToInt(value))+" ms")
        : juce::String(juce::roundToInt(value))+"%";
    const auto accent = skell::theme::colour(skell::theme::neon);
    const auto w = std::min(93.f, float(getWidth())-6.f);
    const auto x = (getWidth()-w)*.5f;
    const auto pill = juce::Rectangle<float>(x,0.f,w,23.f);
    g.setColour(juce::Colours::black.withAlpha(.70f));
    g.fillRoundedRectangle(pill.translated(0,2.f),8.f);
    g.setGradientFill(juce::ColourGradient(
        skell::theme::colour(skell::theme::deep).brighter(.18f),x,0.f,
        skell::theme::colour(skell::theme::background),x,23.f,false));
    g.fillRoundedRectangle(pill,8.f);
    g.setColour(accent.withAlpha(.91f));
    g.drawRoundedRectangle(pill.reduced(.6f),8.f,1.15f);
    g.setFont(skell::theme::mono(12.5f));
    g.setColour(skell::theme::colour(skell::theme::bone));
    g.drawText(text,pill.toNearestInt(),juce::Justification::centred);
}
