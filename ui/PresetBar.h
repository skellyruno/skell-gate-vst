#pragma once
#include "Theme.h"
#include <array>
class SkellGlitchAudioProcessor;

// Compact hardware preset strip. A single cascading popup owns categories,
// Save/Open and legacy A/B snapshots. Dedicated Undo/Redo replace TOOLS.
class PresetBar : public juce::Component, private juce::Timer {
public:
    explicit PresetBar(SkellGlitchAudioProcessor&);
    void resized() override;
    void paint(juce::Graphics&) override;
    void showPresetMenu();
    juce::PopupMenu buildPresetMenu() const;
private:
    SkellGlitchAudioProcessor& processor;
    void timerCallback() override;
    void stepPreset(int direction);
    void executeMenuChoice(int choice);
    void savePreset();
    void openPreset();

    juce::TextButton previous{"<"}, next{">"};
    juce::TextButton presetSelector{"Default  \u25be"};
    juce::TextButton undo{"UNDO"}, redo{"REDO"};
    juce::String lastShownName;
    std::unique_ptr<juce::FileChooser> chooser;
};
