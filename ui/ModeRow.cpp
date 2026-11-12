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
    timerCallback(); // Correct active mode and associated combo on first paint.
    startTimerHz(20);
}
void ModeRow::resized() {
    // Four hardware switches align with the 545px step-sequencer lane.
    constexpr int gap = 10;
    const int itemWidth = (getWidth() - 3 * gap) / 4;
    sync.setBounds(0, 2, itemWidth, 41);
    for (int i = 0; i < 3; ++i)
        modes[static_cast<size_t>(i)].setBounds((i + 1) * (itemWidth + gap), 2, itemWidth, 41);
}
void ModeRow::paint(juce::Graphics&) {}
void ModeRow::timerCallback() {
    const int mode = static_cast<int>(processor.apvts.getRawParameterValue("mode")->load());
    if (mode == displayedMode) return;
    displayedMode = mode;
    for (int i = 0; i < 3; ++i) modes[static_cast<size_t>(i)].setToggleState(i == mode, juce::dontSendNotification);
}
