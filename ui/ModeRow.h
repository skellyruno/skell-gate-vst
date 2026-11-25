#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class ModeRow : public juce::Component, private juce::Timer {
public:
    ModeRow(SkellGlitchAudioProcessor&, juce::TextButton& syncControl);
    void resized() override;
    void paint(juce::Graphics&) override;
    void selectEffect(int index, bool add);
private:
    void timerCallback() override;
    SkellGlitchAudioProcessor& processor;
    juce::TextButton& sync;
    class EffectButton : public juce::TextButton {
    public:
        explicit EffectButton(const juce::String& name) : juce::TextButton(name) {}
        std::function<void()> onAdd;
        void mouseDown(const juce::MouseEvent& e) override {
            if (e.mods.isPopupMenu()) {
                adding=true;
                if (onAdd) onAdd();
            } else { adding=false; juce::TextButton::mouseDown(e); }
        }
        void mouseUp(const juce::MouseEvent& e) override {
            if (adding) { adding=false; return; }
            juce::TextButton::mouseUp(e);
        }
    private:
        bool adding=false;
    };
    std::array<EffectButton, 3> modes;
    juce::TextButton freeze{"FREEZE"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttach;
    int displayedMode = -1;
};
