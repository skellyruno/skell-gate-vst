#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class TopBar : public juce::Component {
public:
    explicit TopBar(SkellGlitchAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void setJitterOffset(int v) { jitterOffset = v; repaint(); }
    juce::TextButton sync{"SYNC ON"}, power{"⏻"}, settings{"⚙"};
private:
    SkellGlitchAudioProcessor& processor;
    int jitterOffset = 0;
};
