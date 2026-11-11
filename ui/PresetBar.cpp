#include "PresetBar.h"
#include "../PluginProcessor.h"
PresetBar::PresetBar(SkellGlitchAudioProcessor& p) : processor(p),
    actions{juce::TextButton("UNDO"), juce::TextButton("REDO"), juce::TextButton("A+"),
            juce::TextButton("A USE"), juce::TextButton("B+"), juce::TextButton("B USE"),
            juce::TextButton("SAVE"), juce::TextButton("OPEN")} {
    presets.addItemList(SkellGlitchAudioProcessor::factoryPresetNames(), 1);
    presets.setSelectedId(12, juce::dontSendNotification);
    presets.onChange = [this] { if (presets.getSelectedId() > 0) processor.loadFactoryPreset(presets.getSelectedId() - 1); };
    presets.setTooltip("Choose a factory preset");
    addAndMakeVisible(presets);
    const std::array<const char*, 8> hints{
        "Undo", "Redo", "Capture preset snapshot A", "Recall snapshot A",
        "Capture preset snapshot B", "Recall snapshot B",
        "Export user preset to XML", "Import user preset from XML"
    };
    for (size_t i = 0; i < actions.size(); ++i) {
        actions[i].setTooltip(hints[i]);
        addAndMakeVisible(actions[i]);
    }
    actions[0].onClick = [this] { processor.undo.undo(); };
    actions[1].onClick = [this] { processor.undo.redo(); };
    actions[2].onClick = [this] { processor.snapshotAB(0); };
    actions[3].onClick = [this] { processor.recallAB(0); };
    actions[4].onClick = [this] { processor.snapshotAB(1); };
    actions[5].onClick = [this] { processor.recallAB(1); };
    actions[6].onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Export Skell-Glitch preset", SkellGlitchAudioProcessor::userPresetFolder().getChildFile("My Preset.xml"), "*.xml");
        juce::Component::SafePointer<PresetBar> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
           [safe](const juce::FileChooser& c) {
               if (safe != nullptr && c.getResult() != juce::File{})
                   safe->processor.saveUserPreset(c.getResult(), c.getResult().getFileNameWithoutExtension());
           });
    };
    actions[7].onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Import Skell-Glitch preset", SkellGlitchAudioProcessor::userPresetFolder(), "*.xml");
        juce::Component::SafePointer<PresetBar> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
           [safe](const juce::FileChooser& c) {
               if (safe != nullptr && c.getResult() != juce::File{})
                   safe->processor.loadUserPreset(c.getResult());
           });
    };
}
void PresetBar::resized() {
    // One baseline, consistent 4px gaps, and no button-on-button borders.
    constexpr int selectorWidth = 166, controlsStart = 172, gap = 4;
    presets.setBounds(0, 0, selectorWidth, getHeight());
    const int available = getWidth() - controlsStart - gap * 7;
    const int baseWidth = available / 8;
    int px = controlsStart;
    for (size_t i = 0; i < actions.size(); ++i) {
        const int width = i == 7 ? getWidth() - px : baseWidth;
        actions[i].setBounds(px, 0, width, getHeight());
        px += width + gap;
    }
}
void PresetBar::paint(juce::Graphics&) {}
