#include "LookAndFeel.h"
#include <cmath>
SkellLookAndFeel::SkellLookAndFeel() {
    setColour(juce::ComboBox::textColourId, skell::theme::colour(skell::theme::bone));
    setColour(juce::ComboBox::backgroundColourId, skell::theme::colour(skell::theme::tile));
    setColour(juce::ComboBox::outlineColourId, skell::theme::colour(skell::theme::border));
    setColour(juce::PopupMenu::backgroundColourId, skell::theme::colour(skell::theme::tile));
    setColour(juce::PopupMenu::textColourId, skell::theme::colour(skell::theme::bone));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, skell::theme::colour(skell::theme::dim));
    setColour(juce::Slider::thumbColourId, skell::theme::colour(skell::theme::neon));
    setColour(juce::Slider::rotarySliderFillColourId, skell::theme::colour(skell::theme::neon));
    setColour(juce::Slider::textBoxTextColourId, skell::theme::colour(skell::theme::bone));
    setColour(juce::Slider::textBoxBackgroundColourId, skell::theme::colour(skell::theme::tile));
    setColour(juce::TextEditor::textColourId, skell::theme::colour(skell::theme::bone));
    setColour(juce::TextEditor::backgroundColourId, skell::theme::colour(skell::theme::tile));
    setColour(juce::TextEditor::highlightColourId, skell::theme::colour(skell::theme::dim));
}
void SkellLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(1.f);
    const bool on = b.getToggleState();
    const auto fill = on ? skell::theme::colour(skell::theme::neon) :
                     skell::theme::colour(skell::theme::tile).brighter(hover ? .13f : 0.f);
    g.setColour(on ? fill.withAlpha(.12f) : fill);
    if (on) g.fillRoundedRectangle(r.expanded(1.f), 9);
    g.setColour(fill.darker(down ? .2f : 0.f));
    g.fillRoundedRectangle(r, 9);
    g.setColour(on ? skell::theme::colour(skell::theme::pale) : skell::theme::colour(skell::theme::border));
    g.drawRoundedRectangle(r, 9, 1.f);
}
void SkellLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) {
    g.setColour(skell::theme::colour(b.getToggleState() ? skell::theme::deep : skell::theme::bone));
    g.setFont(skell::theme::display(b.getHeight() > 40 ? 15.f : 12.f));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(4), juce::Justification::centred, 1);
}
void SkellLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                     float pos, float start, float end, juce::Slider& slider) {
    const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h));
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * .5f - 11.f;
    const auto center = bounds.getCentre();
    const auto angle = start + pos * (end - start);
    g.setColour(skell::theme::colour(skell::theme::tile));
    g.fillEllipse(center.x - radius + 7, center.y - radius + 7, (radius - 7) * 2, (radius - 7) * 2);
    juce::Path track;
    track.addCentredArc(center.x, center.y, radius, radius, 0.f, start, end, true);
    g.setColour(skell::theme::colour(0xff243329));
    g.strokePath(track, juce::PathStrokeType(8.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path arc;
    arc.addCentredArc(center.x, center.y, radius, radius, 0.f, start, angle, true);
    g.setColour(skell::theme::colour(skell::theme::dim).withAlpha(.7f));
    g.strokePath(arc, juce::PathStrokeType(11.f));
    g.setColour(skell::theme::colour(skell::theme::neon));
    g.strokePath(arc, juce::PathStrokeType(6.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto thumb = center.getPointOnCircumference(radius, angle);
    g.setColour(skell::theme::colour(skell::theme::pale)); g.fillEllipse(thumb.x - 5, thumb.y - 5, 10, 10);
    g.setColour(skell::theme::colour(skell::theme::bone));
    // APVTS slider attachments may override JUCE's text conversion with
    // floating-point representations like 30.000019. Format display values
    // independently so the knob always shows a stable readable value.
    const auto name = slider.getName();
    const double value = slider.getValue();
    const juce::String display = (name == "FADE IN" || name == "FADE OUT")
        ? (value < 0.5 ? juce::String("Inst") : juce::String(juce::roundToInt(value)) + " ms")
        : juce::String(juce::roundToInt(value)) + "%";
    g.setFont(skell::theme::display(19.f));
    g.drawFittedText(display,
         juce::Rectangle<int>(static_cast<int>(center.x - radius + 12), static_cast<int>(center.y - 12),
                        static_cast<int>(radius * 2 - 24), 24), juce::Justification::centred, 1);
}
void SkellLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) {
    skell::theme::roundTile(g, juce::Rectangle<float>(0, 0, static_cast<float>(w), static_cast<float>(h)), 6);
    g.setColour(skell::theme::colour(skell::theme::neon));
    juce::Path p; p.addTriangle(static_cast<float>(w - 17), h * .45f,
                                static_cast<float>(w - 9), h * .45f, static_cast<float>(w - 13), h * .60f);
    g.fillPath(p);
}
void SkellLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int w, int h) {
    g.fillAll(skell::theme::colour(skell::theme::tile));
    g.setColour(skell::theme::colour(skell::theme::border)); g.drawRect(0, 0, w, h);
}
juce::Font SkellLookAndFeel::getComboBoxFont(juce::ComboBox&) { return skell::theme::regular(13); }
