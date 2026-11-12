#include "WaveDisplay.h"
#include "../PluginProcessor.h"
#include <algorithm>
#include <cmath>
WaveDisplay::WaveDisplay(SkellGlitchAudioProcessor& p) : processor(p) {
    startTimerHz(30);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    setTooltip("Waveform: drag to paint steps; Alt+click cycles the step state; scroll to zoom");
}
void WaveDisplay::timerCallback() {
    if (!isShowing()) return;
    const auto flip = processor.flipCounter.load(std::memory_order_relaxed);
    const int reverse = processor.displayReverse.load(std::memory_order_relaxed);
    const int step = processor.displayStep.load(std::memory_order_relaxed);
    const float phase = processor.displayPhase.load(std::memory_order_relaxed);
    const bool playing = processor.displayTransport.load(std::memory_order_relaxed) != 0;
    if (flip != lastFlip || reverse != lastReverse || step != lastStep
        || (playing && std::abs(phase - lastPhase) >= .002f)) {
        lastFlip = flip; lastReverse = reverse; lastStep = step; lastPhase = phase;
        repaint();
    }
}
void WaveDisplay::paint(juce::Graphics& g) {
    using namespace skell::theme;
    const auto bounds=getLocalBounds().toFloat().reduced(2.f);
    const auto inside=bounds.reduced(4.f);
    // Deep inset glass panel: the outer bezel is machined metal, but the
    // display itself stays nearly black instead of the old washed-out gray.
    const bool black = matteBlack();
    g.setColour(juce::Colours::black.withAlpha(.69f));
    g.fillRoundedRectangle(bounds.translated(0, 4.0f), 18.f);
    if (black) {
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xffa0aba1), bounds.getX(),bounds.getY(),
            juce::Colour(0xff333b34),bounds.getRight(),bounds.getBottom(),false));
        g.fillRoundedRectangle(bounds,18.f);
        g.setColour(juce::Colour(0xff090d0b));
        g.fillRoundedRectangle(bounds.reduced(3.f),16.f);
        const auto glass=inside.reduced(2.2f);
        g.setGradientFill(juce::ColourGradient(
            juce::Colour(0xff070e0b),glass.getCentreX(),glass.getY(),
            juce::Colour(0xff0f2417),glass.getCentreX(),glass.getBottom(),false));
        g.fillRoundedRectangle(glass,12.f);
        g.setColour(colour(neon).withAlpha(.70f));
        g.drawRoundedRectangle(glass.reduced(.7f),12.f,1.2f);
        g.setColour(colour(neon).withAlpha(.07f));
        g.drawRoundedRectangle(glass.reduced(2.f),10.f,3.f);
        g.setColour(juce::Colours::white.withAlpha(.35f));
        g.drawRoundedRectangle(bounds.reduced(1.f),17.f,.8f);
        g.setColour(juce::Colours::white.withAlpha(.09f));
        g.drawLine(glass.getX()+22.f,glass.getY()+3.f,
                   glass.getRight()-22.f,glass.getY()+3.f,1.f);
    } else {
        g.setGradientFill(juce::ColourGradient(colour(tile).brighter(.30f),
            bounds.getX(),bounds.getY(),colour(border).darker(.15f),
            bounds.getRight(),bounds.getBottom(),false));
        g.fillRoundedRectangle(bounds,18.f);
        g.setGradientFill(juce::ColourGradient(colour(deep).darker(.12f),
            inside.getCentreX(),inside.getY(),colour(deep).brighter(.18f),
            inside.getCentreX(),inside.getBottom(),false));
        g.fillRoundedRectangle(inside,14.f);
        g.setColour(colour(border).withAlpha(.6f));
        g.drawRoundedRectangle(inside.reduced(.5f),14.f,1.f);
    }
    for (int beat=1; beat<8; ++beat) {
        const float x=20.f+(getWidth()-40.f)*beat/8.f;
        g.setColour(black ? colour(neon).withAlpha(.075f)
                          : colour(border).withAlpha(.60f));
        g.drawLine(x,31.f,x,getHeight()-24.f,.85f);
    }
    const float phase=std::clamp(processor.displayPhase.load(std::memory_order_relaxed),0.f,1.f);
    const bool reverse=processor.displayReverse.load(std::memory_order_relaxed)!=0;
    const float playX=20.f+(getWidth()-40.f)*(reverse?1.f-phase:phase);
    bool hasWaveform=false;
    for (const auto& peak:processor.waveform)
        if (peak.load(std::memory_order_relaxed)>.001f) { hasWaveform=true;break; }
    const int visible=std::max(1,static_cast<int>(160/zoom));
    if (hasWaveform) {
        for(int bar=0;bar<160;++bar) {
            const auto src=static_cast<size_t>(std::clamp(static_cast<int>(bar*visible/160),0,159));
            const auto index=reverse?159-src:src;
            const float amplitude=std::min(1.f,processor.waveform[index].load(std::memory_order_relaxed)*3.4f);
            const float x=20.f+bar*(getWidth()-40.f)/160.f;
            const float h=std::max(1.3f,amplitude*(getHeight()-70.f)*.5f);
            const bool played=reverse?x>=playX:x<=playX;
            g.setColour(colour(played?neon:dim).withAlpha(played?.97f:.63f));
            g.fillRoundedRectangle(x,getHeight()*.50f-h,2.f,h*2.f,1.f);
        }
        g.setColour(colour(neon).withAlpha(.25f));
        g.drawLine(playX,31.f,playX,getHeight()-23.f,5.f);
        g.setColour(colour(neon));
        g.drawLine(playX,31.f,playX,getHeight()-23.f,2.f);
    } else {
        g.setColour(colour(bone).withAlpha(.87f));
        g.setFont(regular(13.6f));
        g.drawText("WAITING FOR INPUT",getLocalBounds().reduced(26),
                   juce::Justification::centred);
    }
    const int mode=static_cast<int>(processor.apvts.getRawParameterValue("mode")->load());
    if(mode!=0) {
        const int count=std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()),1,32);
        for(int i=0;i<count;++i) {
            const auto p=processor.apvts.getRawParameterValue("step"+juce::String(i+1)+"_state");
            if(p && p->load()<.5f) {
                const float x=20.f+(getWidth()-40.f)*i/count;
                g.setColour(colour(dim).withAlpha(.13f));
                g.fillRect(x,31.f,(getWidth()-40.f)/count,static_cast<float>(getHeight()-55));
            }
        }
    }
    g.setColour(colour(bone));
    g.setFont(mono(10.8f));
    g.drawText("LIVE SLICE  /  "+juce::String(zoom,1)+"x",17,9,149,18,juce::Justification::centredLeft);
    if(reverse) {
        g.setColour(colour(magenta));
        g.drawText("REVERSE",getWidth()-120,9,84,18,juce::Justification::centredRight);
        juce::Path arrow;
        arrow.startNewSubPath(getWidth()-24.f,12.f);
        arrow.lineTo(getWidth()-33.f,18.f);arrow.lineTo(getWidth()-24.f,24.f);
        g.strokePath(arrow,juce::PathStrokeType(1.6f));
    }
}
void WaveDisplay::editAt(const juce::MouseEvent& e) {
    const int steps = std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()), 1, 32);
    const int index = std::clamp(static_cast<int>((e.position.x - 20.f) / (getWidth() - 40.f) * steps), 0, steps - 1);
    if (index == lastEdited) return;
    lastEdited = index;
    const auto key = "step" + juce::String(index + 1) + "_state";
    if (auto* p = processor.apvts.getParameter(key)) {
        const float original = processor.apvts.getRawParameterValue(key)->load();
        const int next = e.mods.isAltDown() ? (static_cast<int>(original) + 1) % 4 : (paintOn ? 1 : 0);
        p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next))); p->endChangeGesture();
    }
}
void WaveDisplay::mouseDown(const juce::MouseEvent& e) {
    lastEdited = -1;
    const int steps = std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()), 1, 32);
    const int idx = std::clamp(static_cast<int>((e.position.x - 20.f) / (getWidth() - 40.f) * steps), 0, steps - 1);
    const auto key = "step" + juce::String(idx + 1) + "_state";
    paintOn = processor.apvts.getRawParameterValue(key)->load() < .5f;
    editAt(e);
}
void WaveDisplay::mouseDrag(const juce::MouseEvent& e) { editAt(e); }
void WaveDisplay::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& d) {
    zoom = std::clamp(zoom + d.deltaY * .7f, 1.f, 4.f); repaint();
}
