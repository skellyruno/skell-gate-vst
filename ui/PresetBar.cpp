#include "PresetBar.h"
#include "../PluginProcessor.h"
#include <algorithm>
#include <array>

namespace {
// This is a presentation order, NOT new preset IDs. Factory ID 0..27
// and existing FL Studio session/state identifiers remain stable.
constexpr std::array<int,28> presetOrder{{
    11,
    0,1,5,6,8,10,12,13,14,15,16,
    3,7,17,18,19,20,21,
    2,4,9,22,23,24,25,26,27
}};
constexpr int saveId = 1001;
constexpr int openId = 1002;
constexpr int captureAId = 2001;
constexpr int recallAId = 2002;
constexpr int captureBId = 2003;
constexpr int recallBId = 2004;
}

PresetBar::PresetBar(SkellGlitchAudioProcessor& p) : processor(p) {
    setName("HEADER_PRESET_BAR");
    previous.setName("PRESET_PREVIOUS");
    next.setName("PRESET_NEXT");
    presetSelector.setName("HEADER_PRESET_SELECTION");
    undo.setName("PRESET_UNDO");
    redo.setName("PRESET_REDO");

    previous.setTooltip("Previous factory preset");
    next.setTooltip("Next factory preset");
    presetSelector.setTooltip("Factory presets by category, and Save/Open");
    undo.setTooltip("Undo the last automatable parameter change");
    redo.setTooltip("Redo the previous change");

    previous.onClick = [this] { stepPreset(-1); };
    next.onClick = [this] { stepPreset(1); };
    presetSelector.onClick = [this] { showPresetMenu(); };
    undo.onClick = [this] { if (processor.undo.canUndo()) processor.undo.undo(); };
    redo.onClick = [this] { if (processor.undo.canRedo()) processor.undo.redo(); };
    for (auto* button : {&previous,&presetSelector,&next,&undo,&redo})
        addAndMakeVisible(*button);

    timerCallback();
    startTimerHz(8);
}

void PresetBar::resized() {
    constexpr int gap=4, arrowWidth=29, historyWidth=45;
    const int nameWidth=std::max(58,getWidth()-2*arrowWidth-2*historyWidth-4*gap);
    int x=0;
    previous.setBounds(x,2,arrowWidth,getHeight()-4); x+=arrowWidth+gap;
    presetSelector.setBounds(x,0,nameWidth,getHeight()); x+=nameWidth+gap;
    next.setBounds(x,2,arrowWidth,getHeight()-4); x+=arrowWidth+gap;
    undo.setBounds(x,2,historyWidth,getHeight()-4); x+=historyWidth+gap;
    redo.setBounds(x,2,historyWidth,getHeight()-4);
}

void PresetBar::stepPreset(int direction) {
    const auto names=SkellGlitchAudioProcessor::factoryPresetNames();
    const auto currentId=names.indexOf(processor.currentPresetName());
    const auto it=std::find(presetOrder.begin(),presetOrder.end(),currentId);
    const int count=static_cast<int>(presetOrder.size());
    const int position=it==presetOrder.end() ? 0 :
        static_cast<int>(std::distance(presetOrder.begin(),it));
    const int nextPosition=(position + (direction >= 0 ? 1 : count-1)) % count;
    processor.loadFactoryPreset(presetOrder[static_cast<size_t>(nextPosition)]);
    timerCallback();
}

void PresetBar::timerCallback() {
    const auto name=processor.currentPresetName();
    if (name!=lastShownName) {
        lastShownName=name;
        presetSelector.setButtonText(name+"  \u25be");
    }
    const bool canUndo=processor.undo.canUndo();
    const bool canRedo=processor.undo.canRedo();
    if (undo.isEnabled()!=canUndo) undo.setEnabled(canUndo);
    if (redo.isEnabled()!=canRedo) redo.setEnabled(canRedo);
}

void PresetBar::showPresetMenu() {
    juce::PopupMenu menu;
    const auto names=SkellGlitchAudioProcessor::factoryPresetNames();
    const auto selected=processor.currentPresetName();
    menu.addSectionHeader("SKELLGLITCH PRESETS");
    menu.addItem(12,"Default",true,selected=="Default");
    auto subMenu=[&](int begin,int end) {
        juce::PopupMenu submenu;
        for (int p=begin;p<end;++p) {
            const int factoryId=presetOrder[static_cast<size_t>(p)];
            submenu.addItem(factoryId+1,names[factoryId],
                            true,selected==names[factoryId]);
        }
        return submenu;
    };
    menu.addSubMenu("Reverse",subMenu(1,12));
    menu.addSubMenu("Gate",subMenu(12,19));
    menu.addSubMenu("Both",subMenu(19,28));
    menu.addSeparator();
    menu.addSectionHeader("PRESET FILES");
    menu.addItem(saveId,"Save preset...");
    menu.addItem(openId,"Open preset...");
    juce::PopupMenu snapshots;
    snapshots.addItem(captureAId,"Capture A");
    snapshots.addItem(recallAId,"Recall A");
    snapshots.addItem(captureBId,"Capture B");
    snapshots.addItem(recallBId,"Recall B");
    menu.addSubMenu("A / B snapshots",snapshots);
    juce::Component::SafePointer<PresetBar> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetSelector),
        [safe](int choice) {
            if (safe!=nullptr) safe->executeMenuChoice(choice);
        });
}

void PresetBar::executeMenuChoice(int choice) {
    const auto names=SkellGlitchAudioProcessor::factoryPresetNames();
    if (choice>=1 && choice<=names.size()) {
        processor.loadFactoryPreset(choice-1);
        timerCallback();
        return;
    }
    switch(choice) {
        case saveId: savePreset(); break;
        case openId: openPreset(); break;
        case captureAId: processor.snapshotAB(0); break;
        case recallAId: processor.recallAB(0); break;
        case captureBId: processor.snapshotAB(1); break;
        case recallBId: processor.recallAB(1); break;
        default: break;
    }
}

void PresetBar::savePreset() {
    chooser=std::make_unique<juce::FileChooser>(
        "Save SkellGlitch preset",
        SkellGlitchAudioProcessor::userPresetFolder().getChildFile("My Preset.xml"),
        "*.xml");
    juce::Component::SafePointer<PresetBar> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                         juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& c) {
            if (safe!=nullptr && c.getResult()!=juce::File{}) {
                const auto target=c.getResult();
                safe->processor.saveUserPreset(target,target.getFileNameWithoutExtension());
            }
        });
}

void PresetBar::openPreset() {
    chooser=std::make_unique<juce::FileChooser>(
        "Open SkellGlitch preset",
        SkellGlitchAudioProcessor::userPresetFolder(),"*.xml");
    juce::Component::SafePointer<PresetBar> safe(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                         juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser& c) {
            if (safe!=nullptr && c.getResult()!=juce::File{})
                safe->processor.loadUserPreset(c.getResult());
        });
}

void PresetBar::paint(juce::Graphics&) {}
