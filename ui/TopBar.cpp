#include "TopBar.h"
#include "../PluginProcessor.h"
TopBar::TopBar(SkellGlitchAudioProcessor& p) : processor(p) {
    for (auto* b : {&sync, &power, &settings}) addAndMakeVisible(*b);
    sync.setClickingTogglesState(false);
    power.setClickingTogglesState(false);
    settings.setTooltip("Engine settings and window sizes");
}
void TopBar::resized() {
    sync.setBounds(740, 7, 134, 39);
    power.setBounds(882, 7, 45, 39);
    settings.setBounds(692, 11, 39, 32);
}
void TopBar::paint(juce::Graphics& g) {
    const auto green = skell::theme::colour(skell::theme::neon);
    const auto bone = skell::theme::colour(skell::theme::bone);
    g.setColour(green.withAlpha(.15f)); g.fillEllipse(9, 6, 42, 42);
    g.setColour(green); g.drawEllipse(14.f, 11.f, 32.f, 32.f, 2.f);
    juce::Path hand; hand.startNewSubPath(30, 15); hand.lineTo(30, 27); hand.lineTo(38, 31);
    g.strokePath(hand, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved));
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::translation(static_cast<float>(jitterOffset), 0.f));
        g.setFont(skell::theme::display(29));
        g.setColour(bone); g.drawText("Skell", 58, 8, 82, 40, juce::Justification::centredLeft);
        g.setColour(green); g.drawText("Glitch", 134, 8, 99, 40, juce::Justification::centredLeft);
    }
    g.setColour(skell::theme::colour(skell::theme::gray));
    g.setFont(skell::theme::regular(10));
    g.drawText("CUT IT. FLIP IT.", 246, 22, 158, 16, juce::Justification::centredLeft);
    const auto sr = processor.getSampleRate();
    const auto latency = processor.displayLatency.load();
    g.setFont(skell::theme::mono(12));
    g.drawText(juce::String(sr > 0 ? sr / 1000 : 48.0, 1) + " kHz · " + juce::String(latency) + " samples",
               505, 17, 182, 24, juce::Justification::centredRight);
}
