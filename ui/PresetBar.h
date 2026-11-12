#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class PresetBar : public juce::Component, private juce::Timer {
public:
    explicit PresetBar(SkellGlitchAudioProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
    // Menu routes all legacy preset actions without a cluttered third row.
    void showToolsMenu();
    void runAction(int index);
private:
    SkellGlitchAudioProcessor& processor;
    void timerCallback() override;
    void stepPreset(int direction);
    juce::ComboBox presets;
    juce::TextButton previous{"<"}, next{">"}, tools{"TOOLS"};
    juce::String lastShownName;
    std::array<juce::TextButton, 8> actions;
    std::unique_ptr<juce::FileChooser> chooser;
};
