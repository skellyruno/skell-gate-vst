#pragma once
#include "Theme.h"
class SkellLookAndFeel : public juce::LookAndFeel_V4 {
public:
    SkellLookAndFeel();
    void setSkin(skellcore::ui::StudioSkin);
    skellcore::ui::StudioSkin skin() const noexcept { return sharedSkin.skin(); }
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void drawPopupMenuBackground(juce::Graphics&, int, int) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
private:
    // Actual SkellCore palette/appearance component, not a copied skin table.
    skellcore::ui::StudioLookAndFeel sharedSkin;
};
