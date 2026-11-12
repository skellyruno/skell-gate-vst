#pragma once
#include "Theme.h"
#include <cstdint>
class SkellGlitchAudioProcessor;
class SkellGlyphButton final : public juce::TextButton {
public:
    enum class Kind { Skull, Settings, Power };
    explicit SkellGlyphButton(Kind which) : icon(which) {}
    void paintButton(juce::Graphics&, bool hovered, bool pressed) override;
    void setGlitchBurst(int intensity, std::uint32_t variant) {
        if (glitchIntensity != intensity || glitchVariant != variant) {
            glitchIntensity = intensity;
            glitchVariant = variant;
            repaint();
        }
    }
private:
    Kind icon;
    int glitchIntensity = 0;
    std::uint32_t glitchVariant = 0;
};
class TopBar : public juce::Component {
public:
    explicit TopBar(SkellGlitchAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;
    void setJitterOffset(int v) { if (v != jitterOffset) { jitterOffset = v; repaint(); } }
    void setGlitchBurst(int intensity, std::uint32_t variant) {
        if (glitchIntensity != intensity || glitchVariant != variant) {
            glitchIntensity = intensity;
            glitchVariant = variant;
            skull.setGlitchBurst(intensity, variant);
            repaint();
        }
    }
    SkellGlyphButton skull{SkellGlyphButton::Kind::Skull};
    juce::TextButton sync{"SYNC ON"};
    SkellGlyphButton power{SkellGlyphButton::Kind::Power};
    SkellGlyphButton settings{SkellGlyphButton::Kind::Settings};
private:
    SkellGlitchAudioProcessor& processor;
    int jitterOffset = 0;
    int glitchIntensity = 0;
    std::uint32_t glitchVariant = 0;
};
