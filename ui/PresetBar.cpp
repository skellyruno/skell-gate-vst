#include "PresetBar.h"
#include "../PluginProcessor.h"
PresetBar::PresetBar(SkellGlitchAudioProcessor& p) : processor(p),
    actions{juce::TextButton("UNDO"), juce::TextButton("REDO"), juce::TextButton("SAVE A"),
            juce::TextButton("LOAD A"), juce::TextButton("SAVE B"), juce::TextButton("LOAD B"),
            juce::TextButton("EXPORT"), juce::TextButton("IMPORT")} {
    presets.addItemList(SkellGlitchAudioProcessor::factoryPresetNames(), 1);
    presets.setSelectedId(12, juce::dontSendNotification);
    presets.onChange = [this] { if (presets.getSelectedId() > 0) processor.loadFactoryPreset(presets.getSelectedId() - 1); };
    addAndMakeVisible(presets);
    for (auto& b : actions) addAndMakeVisible(b);
    actions[0].onClick = [this] { processor.undo.undo(); };
    actions[1].onClick = [this] { processor.undo.redo(); };
    actions[2].onClick = [this] { processor.snapshotAB(0); };
    actions[3].onClick = [this] { processor.recallAB(0); };
    actions[4].onClick = [this] { processor.snapshotAB(1); };
    actions[5].onClick = [this] { processor.recallAB(1); };
    actions[6].onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Export Skell-Glitch preset", SkellGlitchAudioProcessor::userPresetFolder().getChildFile("My Preset.xml"), "*.xml");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
           [this](const juce::FileChooser& c) { if (c.getResult() != juce::File{}) processor.saveUserPreset(c.getResult(), c.getResult().getFileNameWithoutExtension()); });
    };
    actions[7].onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Import Skell-Glitch preset", SkellGlitchAudioProcessor::userPresetFolder(), "*.xml");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
           [this](const juce::FileChooser& c) { if (c.getResult() != juce::File{}) processor.loadUserPreset(c.getResult()); });
    };
}
void PresetBar::resized() {
    presets.setBounds(0, 0, 167, getHeight());
    const int width = (getWidth() - 174) / 8;
    for (size_t i = 0; i < actions.size(); ++i)
        actions[i].setBounds(174 + static_cast<int>(i) * width, 0, width - 3, getHeight());
}
void PresetBar::paint(juce::Graphics&) {}
