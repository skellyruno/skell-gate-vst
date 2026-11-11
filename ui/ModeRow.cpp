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
                timerCallback(); // Update selected mode and combo visibility on click.
            }
        };
    }
    reverseStyle.addItemList({"Classic", "Tape", "Ping-Pong", "Stutter"}, 1);
    gateRate.addItemList({"1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T"}, 1);
    reverseStyle.setTooltip("Select the reverse tape, classic, ping-pong or stutter algorithm");
    gateRate.setTooltip("Choose rhythmic gate step duration");
    addAndMakeVisible(reverseStyle); addAndMakeVisible(gateRate);
    reverseAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "reverseStyle", reverseStyle);
    gateAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "gateRate", gateRate);
    timerCallback(); // Correct active mode and associated combo on first paint.
    startTimerHz(20);
}
void ModeRow::resized() {
    sync.setBounds(0, 2, 100, 40);
    modes[0].setBounds(107, 1, 101, 41);
    modes[1].setBounds(214, 1, 98, 41);
    modes[2].setBounds(318, 1, 98, 41);
    reverseStyle.setBounds(422, 7, 107, 32);
    gateRate.setBounds(536, 7, 99, 32);
}
void ModeRow::paint(juce::Graphics&) {}
void ModeRow::timerCallback() {
    const int mode = static_cast<int>(processor.apvts.getRawParameterValue("mode")->load());
    if (mode == displayedMode) return;
    displayedMode = mode;
    for (int i = 0; i < 3; ++i) modes[static_cast<size_t>(i)].setToggleState(i == mode, juce::dontSendNotification);
    gateRate.setVisible(mode != 0); reverseStyle.setVisible(mode != 1);
}
