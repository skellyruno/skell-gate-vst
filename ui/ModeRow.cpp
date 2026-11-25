#include "ModeRow.h"
#include "../PluginProcessor.h"
ModeRow::ModeRow(SkellGlitchAudioProcessor& p, juce::TextButton& syncControl)
    : processor(p), sync(syncControl),
      modes{juce::TextButton("REVERSE"), juce::TextButton("GATE"), juce::TextButton("GLITCH")} {
    addAndMakeVisible(sync);
    sync.setTooltip("Follow the host tempo for loop and gate timing.");
    for (int i = 0; i < 3; ++i) {
        auto& button=modes[static_cast<size_t>(i)];
        addAndMakeVisible(button);
        button.setTooltip(i==2?"GLITCH: sound reference pending":"Click to select; Shift-click to combine Reverse and Gate");
        if (i<2) button.onClick=[this,i] { selectEffect(i,juce::ModifierKeys::getCurrentModifiers().isShiftDown()); };
    }
    // Reserve the new effect slot without repurposing the old Mode=Both
    // automation choice or shipping an invented reference effect.
    modes[2].setEnabled(false);
    freeze.setName("PERFORMANCE_FREEZE");
    freeze.setTooltip("Hold the captured slice until Freeze is switched off");
    freeze.setClickingTogglesState(true);
    addAndMakeVisible(freeze);
    freezeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "freeze", freeze);
    timerCallback(); // Correct active mode on first paint.
    startTimerHz(20);
}
void ModeRow::selectEffect(int index, bool add) {
    if (index<0 || index>1) return;
    const int current=static_cast<int>(processor.apvts.getRawParameterValue("mode")->load());
    const int next=add && (current==2 || current!=index) ? 2 : index;
    if (auto* p=processor.apvts.getParameter("mode")) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next)));
        p->endChangeGesture();
        timerCallback();
    }
}
void ModeRow::resized() {
    constexpr int gap = 11, count = 5, w = 92;
    const int start = (getWidth() - (count*w+(count-1)*gap))/2;
    sync.setBounds(start, 5, w, 29);
    for (int i=0; i<3; ++i)
        modes[static_cast<size_t>(i)].setBounds(start+(i+1)*(w+gap),5,w,29);
    freeze.setBounds(start+4*(w+gap),5,w,29);
}
void ModeRow::paint(juce::Graphics&) {}
void ModeRow::timerCallback() {
    const int mode = static_cast<int>(processor.apvts.getRawParameterValue("mode")->load());
    if (mode == displayedMode) {
        for (auto& button : modes)
            if (button.getToggleState()) button.repaint();
        if (sync.getToggleState()) sync.repaint();
        if (freeze.getToggleState()) freeze.repaint();
        return;
    }
    displayedMode = mode;
    for (int i=0;i<2;++i) modes[static_cast<size_t>(i)].setToggleState(i==mode || mode==2,juce::dontSendNotification);
}
