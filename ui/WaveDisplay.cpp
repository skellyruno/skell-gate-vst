#include "WaveDisplay.h"
#include "../PluginProcessor.h"
#include <algorithm>
WaveDisplay::WaveDisplay(SkellGlitchAudioProcessor& p) : processor(p) {
    startTimerHz(60);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    setTooltip("Waveform: drag to paint steps; Alt+click cycles the step state; scroll to zoom");
}
void WaveDisplay::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(1);
    g.setColour(skell::theme::colour(skell::theme::deep)); g.fillRoundedRectangle(r, 16);
    g.setColour(skell::theme::colour(skell::theme::border)); g.drawRoundedRectangle(r, 16, 1.4f);
    const auto mode = static_cast<int>(processor.apvts.getRawParameterValue("mode")->load());
    const float phase = processor.displayPhase.load();
    const bool reverse = processor.displayReverse.load() != 0;
    const float playX = 20.f + (getWidth() - 40.f) * (reverse ? 1.f - phase : phase);
    g.setColour(skell::theme::colour(skell::theme::border));
    for (int beat = 1; beat < 8; ++beat) {
        const auto x = (getWidth() - 40.f) * beat / 8.f + 20.f;
        g.drawLine(x, 28.f, x, getHeight() - 22.f, .9f);
    }
    const int visible = std::max(1, static_cast<int>(160 / zoom));
    for (int bar = 0; bar < 160; ++bar) {
        const auto src = static_cast<size_t>(std::clamp(static_cast<int>(bar * visible / 160), 0, 159));
        const auto index = reverse ? 159 - src : src;
        const float amp = std::min(1.f, processor.waveform[static_cast<size_t>(index)].load(std::memory_order_relaxed) * 3.4f);
        const float x = 20.f + bar * (getWidth() - 40.f) / 160.f;
        const float height = std::max(1.5f, amp * (getHeight() - 67.f) * .5f);
        const bool played = reverse ? x >= playX : x <= playX;
        g.setColour(skell::theme::colour(played ? skell::theme::neon : skell::theme::dim).withAlpha(played ? .98f : .8f));
        g.fillRoundedRectangle(x, getHeight() * .50f - height, 2.f, height * 2.f, 1.f);
    }
    if (mode != 0) {
        const int steps = std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()), 1, 32);
        for (int step = 0; step < steps; ++step) {
            const auto p = processor.apvts.getRawParameterValue("step" + juce::String(step + 1) + "_state");
            if (p && p->load() < .5f) {
                const float x = 20.f + (getWidth() - 40.f) * step / steps;
                const float w = (getWidth() - 40.f) / steps;
                g.setColour(skell::theme::colour(skell::theme::cyan).withAlpha(.09f));
                g.fillRect(x, 26.f, w, static_cast<float>(getHeight() - 48));
            }
        }
    }
    if (reverse) {
        g.setColour(skell::theme::colour(skell::theme::magenta));
        juce::Path arrow; arrow.startNewSubPath(getWidth() - 38.f, 13.f);
        arrow.lineTo(getWidth() - 46.f, 18.f); arrow.lineTo(getWidth() - 38.f, 23.f);
        g.strokePath(arrow, juce::PathStrokeType(1.6f));
        g.setFont(skell::theme::mono(11));
        g.drawText("REVERSE", getWidth() - 121, 8, 75, 19, juce::Justification::centredRight);
    }
    g.setColour(skell::theme::colour(skell::theme::pale).withAlpha(.20f));
    g.drawLine(playX, 28.f, playX, getHeight() - 22.f, 6.f);
    g.setColour(skell::theme::colour(skell::theme::neon));
    g.drawLine(playX, 28.f, playX, getHeight() - 22.f, 2.f);
    g.setColour(skell::theme::colour(skell::theme::gray));
    g.setFont(skell::theme::mono(10));
    g.drawText("LIVE SLICE  /  " + juce::String(zoom, 1) + "x", 16, 7, 120, 17, juce::Justification::centredLeft);
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
