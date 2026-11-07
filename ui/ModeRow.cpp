#include "ModeRow.h"
#include "../PluginProcessor.h"
ModeRow::ModeRow(SkellGlitchAudioProcessor& p) : processor(p),
    modes{juce::TextButton("REVERSE"), juce::TextButton("GATE"), juce::TextButton("BOTH")} {
    for (int i = 0; i < 3; ++i) {
        addAndMakeVisible(modes[static_cast<size_t>(i)]);
        modes[static_cast<size_t>(i)].onClick = [this, i] {
            if (auto* p = processor.apvts.getParameter("mode")) {
                p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(i))); p->endChangeGesture();
            }
        };
    }
    reverseStyle.addItemList({"Classic", "Tape", "Ping-Pong", "Stutter"}, 1);
    gateRate.addItemList({"1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T"}, 1);
    addAndMakeVisible(reverseStyle); addAndMakeVisible(gateRate);
    reverseAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "reverseStyle", reverseStyle);
    gateAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "gateRate", gateRate);
    startTimerHz(20);
}
void ModeRow::resized() {
    for (int i = 0; i < 3; ++i) modes[static_cast<size_t>(i)].setBounds(116 + i * 106, 0, 101, 42);
    gateRate.setBounds(0, 7, 109, 31);
    reverseStyle.setBounds(439, 7, 102, 31);
}
void ModeRow::paint(juce::Graphics&) {}
void ModeRow::timerCallback() {
    const int mode = static_cast<int>(processor.apvts.getRawParameterValue("mode")->load());
    for (int i = 0; i < 3; ++i) modes[static_cast<size_t>(i)].setToggleState(i == mode, juce::dontSendNotification);
    gateRate.setVisible(mode != 0); reverseStyle.setVisible(mode != 1);
}
