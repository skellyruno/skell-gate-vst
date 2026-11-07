#include "BandSlider.h"
#include "../PluginProcessor.h"
#include <cmath>
BandSlider::BandSlider(SkellGlitchAudioProcessor& p) : processor(p) { startTimerHz(30); setMouseCursor(juce::MouseCursor::PointingHandCursor); }
float BandSlider::frequencyToX(float hz) const { return 14.f + (getWidth() - 28.f) * std::log(std::clamp(hz, 20.f, 20000.f) / 20.f) / std::log(1000.f); }
float BandSlider::xToFrequency(float x) const { return 20.f * std::pow(1000.f, std::clamp((x - 14.f) / (getWidth() - 28.f), 0.f, 1.f)); }
void BandSlider::paint(juce::Graphics& g) {
    const float low = processor.apvts.getRawParameterValue("bandLow")->load();
    const float high = processor.apvts.getRawParameterValue("bandHigh")->load();
    const auto a = frequencyToX(low), b = frequencyToX(high);
    g.setColour(skell::theme::colour(skell::theme::gray));
    g.setFont(skell::theme::regular(12));
    g.drawText("LOW CUT", 8, 0, 100, 19, juce::Justification::centredLeft);
    g.drawText("HIGH CUT", getWidth() - 110, 0, 100, 19, juce::Justification::centredRight);
    g.setColour(skell::theme::colour(0xff203029));
    g.fillRoundedRectangle(14.f, 30.f, getWidth() - 28.f, 8.f, 4.f);
    g.setColour(skell::theme::colour(skell::theme::dim));
    g.fillRoundedRectangle(a, 30.f, std::max(1.f, b - a), 8.f, 4.f);
    g.setColour(skell::theme::colour(skell::theme::neon));
    g.fillRoundedRectangle(a, 32.f, std::max(1.f, b - a), 4.f, 2.f);
    for (const auto pos : {a, b}) {
        g.setColour(skell::theme::colour(skell::theme::bone));
        g.fillRoundedRectangle(pos - 7.f, 22.f, 14.f, 25.f, 5.f);
        g.setColour(skell::theme::colour(skell::theme::deep));
        g.drawRoundedRectangle(pos - 7.f, 22.f, 14.f, 25.f, 5.f, 1.f);
    }
    const auto fmt = [](float f) { return f >= 1000 ? juce::String(f / 1000.f, 1) + " kHz" : juce::String(f, 0) + " Hz"; };
    g.setFont(skell::theme::mono(11));
    g.setColour(skell::theme::colour(skell::theme::bone));
    g.drawText(fmt(low), 8, 49, 100, 16, juce::Justification::centredLeft);
    g.drawText(fmt(high), getWidth() - 110, 49, 100, 16, juce::Justification::centredRight);
    g.setColour(skell::theme::colour(skell::theme::gray));
    g.drawText("BAND", 0, 56, getWidth(), 18, juce::Justification::centred);
}
void BandSlider::change(float x) {
    const char* id = selected ? "bandHigh" : "bandLow";
    const float other = processor.apvts.getRawParameterValue(selected ? "bandLow" : "bandHigh")->load();
    const auto hz = selected ? std::max(other, xToFrequency(x)) : std::min(other, xToFrequency(x));
    if (auto* p = processor.apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(hz));
    repaint();
}
void BandSlider::mouseDown(const juce::MouseEvent& e) {
    const float a = frequencyToX(processor.apvts.getRawParameterValue("bandLow")->load());
    const float b = frequencyToX(processor.apvts.getRawParameterValue("bandHigh")->load());
    selected = std::abs(e.position.x - b) < std::abs(e.position.x - a) ? 1 : 0;
    if (auto* p = processor.apvts.getParameter(selected ? "bandHigh" : "bandLow")) p->beginChangeGesture();
    change(e.position.x);
}
void BandSlider::mouseDrag(const juce::MouseEvent& e) { change(e.position.x); }
void BandSlider::mouseUp(const juce::MouseEvent&) { if (auto* p = processor.apvts.getParameter(selected ? "bandHigh" : "bandLow")) p->endChangeGesture(); }
void BandSlider::mouseDoubleClick(const juce::MouseEvent& e) {
    selected = e.position.x < getWidth() / 2.f ? 0 : 1;
    const char* id = selected ? "bandHigh" : "bandLow";
    if (auto* p = processor.apvts.getParameter(id)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(selected ? 20000.f : 20.f));
        p->endChangeGesture();
    }
}
