#include "LoopRow.h"
#include "../PluginProcessor.h"
LoopRow::LoopRow(SkellGlitchAudioProcessor& p) : processor(p) {
    const juce::StringArray text{"1/16", "1/8T", "1/8", "1/4", "1/2", "1 Bar", "2 Bars", "4 Bars", "8 Bars"};
    for (int i = 0; i < 9; ++i) {
        buttons[static_cast<size_t>(i)] = std::make_unique<juce::TextButton>(text[i]);
        addAndMakeVisible(*buttons[static_cast<size_t>(i)]);
        buttons[static_cast<size_t>(i)]->onClick = [this, i] {
            if (auto* param = processor.apvts.getParameter("loopLength")) {
                param->beginChangeGesture(); param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(i))); param->endChangeGesture();
            }
        };
    }
    startTimerHz(20);
}
void LoopRow::resized() {
    const int gap = 8;
    const int w = (getWidth() - gap * 8) / 9;
    for (int i = 0; i < 9; ++i) buttons[static_cast<size_t>(i)]->setBounds(i * (w + gap), 0, w, 52);
}
void LoopRow::paint(juce::Graphics& g) {
    g.setColour(skell::theme::colour(skell::theme::gray)); g.setFont(skell::theme::regular(12));
    g.drawText("LOOP LENGTH", 0, 54, getWidth(), 18, juce::Justification::centred);
}
void LoopRow::timerCallback() {
    const int value = static_cast<int>(processor.apvts.getRawParameterValue("loopLength")->load());
    for (int i = 0; i < 9; ++i) buttons[static_cast<size_t>(i)]->setToggleState(i == value, juce::dontSendNotification);
}
