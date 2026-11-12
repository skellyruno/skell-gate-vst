#include "PresetBar.h"
#include "../PluginProcessor.h"
PresetBar::PresetBar(SkellGlitchAudioProcessor& p) : processor(p),
    actions{juce::TextButton("UNDO"), juce::TextButton("REDO"), juce::TextButton("A+"),
            juce::TextButton("A USE"), juce::TextButton("B+"), juce::TextButton("B USE"),
            juce::TextButton("SAVE"), juce::TextButton("OPEN")} {
    setName("HEADER_PRESET_BAR");
    presets.addItemList(SkellGlitchAudioProcessor::factoryPresetNames(), 1);
    presets.setSelectedId(12, juce::dontSendNotification);
    presets.onChange = [this] { if (presets.getSelectedId() > 0) processor.loadFactoryPreset(presets.getSelectedId() - 1); };
    presets.setTooltip("Choose a factory preset");
    addAndMakeVisible(presets);
    tools.setButtonText("...");
    tools.setName("PRESET_TOOLS_MENU");
    tools.setTooltip("Undo, redo, snapshots A/B, save and open preset");
    tools.onClick = [this] { showToolsMenu(); };
    addAndMakeVisible(tools);
    const std::array<const char*, 8> hints{
        "Undo", "Redo", "Capture preset snapshot A", "Recall snapshot A",
        "Capture preset snapshot B", "Recall snapshot B",
        "Export user preset to XML", "Import user preset from XML"
    };
    for (size_t i = 0; i < actions.size(); ++i) {
        actions[i].setTooltip(hints[i]);
        // Actions remain callable by menu ID; they are not another row of widgets.
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
    constexpr int gap = 5, toolsWidth = 39;
    presets.setBounds(0, 0, getWidth() - toolsWidth - gap, getHeight());
    tools.setBounds(getWidth() - toolsWidth, 0, toolsWidth, getHeight());
}
void PresetBar::runAction(int index) {
    if (index < 0 || index >= static_cast<int>(actions.size())) return;
    // Reuse the existing tested callbacks so no A/B, import or undo feature
    // is lost when replacing the visible strip with a compact tools menu.
    if (actions[static_cast<size_t>(index)].onClick)
        actions[static_cast<size_t>(index)].onClick();
}
void PresetBar::showToolsMenu() {
    juce::PopupMenu menu;
    menu.addSectionHeader("PRESET TOOLS");
    menu.addItem(1, "Undo");
    menu.addItem(2, "Redo");
    menu.addSeparator();
    menu.addItem(3, "Capture A");
    menu.addItem(4, "Recall A");
    menu.addItem(5, "Capture B");
    menu.addItem(6, "Recall B");
    menu.addSeparator();
    menu.addItem(7, "Save preset...");
    menu.addItem(8, "Open preset...");
    juce::Component::SafePointer<PresetBar> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&tools),
        [safe](int choice) {
            if (safe != nullptr && choice >= 1 && choice <= 8)
                safe->runAction(choice - 1);
        });
}
void PresetBar::paint(juce::Graphics&) {}
