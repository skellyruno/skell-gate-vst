#pragma once
#include "Theme.h"
class SkellGlitchAudioProcessor;
class SkellGlyphButton final : public juce::TextButton {
public:
    enum class Kind { Settings, Power };
    explicit SkellGlyphButton(Kind kind) : icon(kind) {}
    void paintButton(juce::Graphics&, bool hovered, bool pressed) override;
private:
    Kind icon;
};
class TopBar : public juce::Component {
public:
    explicit TopBar(SkellGlitchAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void setJitterOffset(int v) { if (v != jitterOffset) { jitterOffset = v; repaint(); } }
    juce::TextButton sync{"SYNC ON"};
    SkellGlyphButton power{SkellGlyphButton::Kind::Power};
    SkellGlyphButton settings{SkellGlyphButton::Kind::Settings};
private:
    SkellGlitchAudioProcessor& processor;
    int jitterOffset = 0;
};
