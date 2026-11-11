#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class PresetBar : public juce::Component {
public:
    explicit PresetBar(SkellGlitchAudioProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
    // Menu routes all legacy preset actions without a cluttered third row.
    void showToolsMenu();
    void runAction(int index);
private:
    SkellGlitchAudioProcessor& processor;
    juce::ComboBox presets;
    juce::TextButton tools{"TOOLS"};
    std::array<juce::TextButton, 8> actions;
    std::unique_ptr<juce::FileChooser> chooser;
};
