#include "BandSlider.h"
#include "../PluginProcessor.h"
#include <cmath>
BandSlider::BandSlider(SkellGlitchAudioProcessor& p) : processor(p) {
    setTooltip("Drag low/high cut. Hold Shift for fine control. Double-click left/right to reset.");
    startTimerHz(30);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
void BandSlider::timerCallback() {
    const float low = processor.apvts.getRawParameterValue("bandLow")->load();
    const float high = processor.apvts.getRawParameterValue("bandHigh")->load();
    if (low != lastLow || high != lastHigh) { lastLow = low; lastHigh = high; repaint(); }
}
void BandSlider::mouseMove(const juce::MouseEvent& e) {
    const float lowX = frequencyToX(processor.apvts.getRawParameterValue("bandLow")->load());
    const float highX = frequencyToX(processor.apvts.getRawParameterValue("bandHigh")->load());
    const int next = std::abs(e.position.x - highX) < std::abs(e.position.x - lowX) ? 1 : 0;
    if (next != hovered) { hovered = next; repaint(); }
}
void BandSlider::mouseExit(const juce::MouseEvent&) { hovered = -1; repaint(); }
float BandSlider::frequencyToX(float hz) const { return 14.f + (getWidth() - 28.f) * std::log(std::clamp(hz, 20.f, 20000.f) / 20.f) / std::log(1000.f); }
float BandSlider::xToFrequency(float x) const { return 20.f * std::pow(1000.f, std::clamp((x - 14.f) / (getWidth() - 28.f), 0.f, 1.f)); }
void BandSlider::paint(juce::Graphics& g) {
    using namespace skell::theme;
    const float low=processor.apvts.getRawParameterValue("bandLow")->load();
    const float high=processor.apvts.getRawParameterValue("bandHigh")->load();
    const float left=frequencyToX(low),right=frequencyToX(high);
    g.setColour(colour(gray));
    g.setFont(regular(13.f));
    g.drawText("LOW CUT",11,0,120,20,juce::Justification::centredLeft);
    g.drawText("HIGH CUT",getWidth()-136,0,125,20,juce::Justification::centredRight);
    auto rail=juce::Rectangle<float>(13.f,29.f,getWidth()-26.f,10.f);
    g.setColour(juce::Colours::black.withAlpha(.40f));
    g.fillRoundedRectangle(rail.translated(0,2.f),5.f);
    g.setColour(colour(border).darker(.38f));g.fillRoundedRectangle(rail,5.f);
    g.setColour(colour(neon).darker(.5f));
    g.fillRoundedRectangle(left,29.f,std::max(1.f,right-left),10.f,5.f);
    g.setGradientFill(juce::ColourGradient(colour(neon).brighter(.2f),0.f,31.f,
                                          colour(neon).darker(.21f),0.f,38.f,false));
    g.fillRoundedRectangle(left,31.f,std::max(1.f,right-left),6.f,3.f);
    for (const auto freq:{100.f,1000.f,10000.f}) {
        const float x=frequencyToX(freq);
        g.setColour(colour(border).withAlpha(.32f));
        g.drawLine(x,30.f,x,39.f,.8f);
    }
    for(int i=0;i<2;++i) {
        const float x=i==0?left:right;
        auto handle=juce::Rectangle<float>(x-9.f,18.f,18.f,32.f);
        g.setColour(juce::Colours::black.withAlpha(.30f));
        g.fillRoundedRectangle(handle.translated(1.f,2.f),7.f);
        g.setGradientFill(juce::ColourGradient(colour(tile).brighter(.4f),x-8,18,
                            colour(tile).darker(.26f),x+8,50,false));
        g.fillRoundedRectangle(handle,6.f);
        g.setColour(colour(border));g.drawRoundedRectangle(handle,6.f,1.2f);
        g.setColour(juce::Colours::white.withAlpha(.65f));
        g.drawLine(x-6.f,21.f,x+6.f,21.f,1.f);
        g.setColour(colour(neon).darker(.16f));
        g.fillRoundedRectangle(x-1.2f,27.f,2.4f,16.f,1.2f);
    }
    const auto format=[](float f) {
        return f>=1000.f?juce::String(f/1000.f,1)+" kHz":juce::String(f,0)+" Hz";
    };
    g.setColour(colour(bone));g.setFont(mono(12.5f));
    g.drawText(format(low),8,53,128,19,juce::Justification::centredLeft);
    g.drawText(format(high),getWidth()-139,53,131,19,juce::Justification::centredRight);
    g.setColour(colour(gray));
    g.setFont(regular(13.f));
    g.drawText("BAND",0,61,getWidth(),19,juce::Justification::centred);
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
    dragStartX = e.position.x;
    dragStartFrequency = processor.apvts.getRawParameterValue(selected ? "bandHigh" : "bandLow")->load();
    hovered = selected;
    if (auto* p = processor.apvts.getParameter(selected ? "bandHigh" : "bandLow")) p->beginChangeGesture();
    if (!e.mods.isShiftDown()) change(e.position.x);
}
void BandSlider::mouseDrag(const juce::MouseEvent& e) {
    const float x = e.mods.isShiftDown() ? frequencyToX(dragStartFrequency) + (e.position.x - dragStartX) * .18f : e.position.x;
    change(x);
}
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
