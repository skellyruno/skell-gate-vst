#include "ModeRow.h"
#include "../PluginProcessor.h"
ModeRow::ModeRow(SkellGlitchAudioProcessor& p, juce::TextButton& syncControl)
    : processor(p), sync(syncControl),
      modes{juce::TextButton("REVERSE"), juce::TextButton("GATE"), juce::TextButton("BOTH")} {
    addAndMakeVisible(sync);
    sync.setTooltip("Follow the host tempo for loop and gate timing.");
    for (int i = 0; i < 3; ++i) {
        addAndMakeVisible(modes[static_cast<size_t>(i)]);
        modes[static_cast<size_t>(i)].onClick = [this, i] {
            if (auto* p = processor.apvts.getParameter("mode")) {
                p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(i))); p->endChangeGesture();
                timerCallback(); // Update the selected mode immediately.
            }
        };
    }
    freeze.setName("PERFORMANCE_FREEZE");
    freeze.setTooltip("Hold the captured slice until Freeze is switched off");
    freeze.setClickingTogglesState(true);
    addAndMakeVisible(freeze);
    freezeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "freeze", freeze);
    timerCallback(); // Correct active mode on first paint.
    startTimerHz(20);
}
void ModeRow::resized() {
    constexpr int gap = 8;
    constexpr int count = 5;
    const int w = (getWidth() - gap * (count - 1)) / count;
    const int start = (getWidth() - (count * w + gap * (count - 1))) / 2;
    sync.setBounds(start, 2, w, 34);
    for (int i = 0; i < 3; ++i)
        modes[static_cast<size_t>(i)].setBounds(start+(i+1)*(w+gap), 2, w, 34);
    freeze.setBounds(start+4*(w+gap), 2, w, 34);
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
    for (int i = 0; i < 3; ++i) modes[static_cast<size_t>(i)].setToggleState(i == mode, juce::dontSendNotification);
}
