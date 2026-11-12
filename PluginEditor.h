#pragma once
#include <JuceHeader.h>
#include "ui/LookAndFeel.h"
#include "ui/TopBar.h"
#include "ui/LoopRow.h"
#include "ui/WaveDisplay.h"
#include "ui/GateGrid.h"
#include "ui/ModeRow.h"
#include "ui/BandSlider.h"
#include "ui/GlitchKnob.h"
#include "ui/PresetBar.h"
#include "ui/GlitchPulse.h"
class SettingsPanel;
class SkellGlitchAudioProcessor;
class SkellGlitchAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit SkellGlitchAudioProcessorEditor(SkellGlitchAudioProcessor&);
    ~SkellGlitchAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void applyWindowSize(int index);
private:
    void timerCallback() override;
    SkellGlitchAudioProcessor& processor;
    SkellLookAndFeel look;
    juce::Component canvas;
    std::unique_ptr<juce::Component> background;
    TopBar top;
    LoopRow loops;
    WaveDisplay waveform;
    GateGrid gate;
    ModeRow modes;
    PresetBar presets;
    BandSlider band;
    GlitchKnob fadeIn{"FADE IN"}, fadeOut{"FADE OUT"}, smooth{"SMOOTH"}, mix{"MIX"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fadeInAttach, fadeOutAttach, smoothAttach, mixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach, powerAttach;
    std::unique_ptr<SettingsPanel> settings;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    float uiScale = 1.f;
    uint64_t lastFlip = 0;
    GlitchPulse visualPulse{static_cast<std::uint32_t>(juce::Time::getMillisecondCounter())};
    int lastPulseIntensity = 0;
    int lastHeaderLatency = -1;
    double lastHeaderSampleRate = -1.0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkellGlitchAudioProcessorEditor)
};
