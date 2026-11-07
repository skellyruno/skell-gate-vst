#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class PresetBar : public juce::Component {
public:
    explicit PresetBar(SkellGlitchAudioProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    SkellGlitchAudioProcessor& processor;
    juce::ComboBox presets;
    std::array<juce::TextButton, 8> actions;
    std::unique_ptr<juce::FileChooser> chooser;
};
