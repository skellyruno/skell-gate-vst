#include "LookAndFeel.h"
#include <cmath>
#include <algorithm>
SkellLookAndFeel::SkellLookAndFeel() : sharedSkin(skellcore::ui::StudioSkinStore::load()) {
    setSkin(sharedSkin.skin());
}
void SkellLookAndFeel::setSkin(skellcore::ui::StudioSkin chosen) {
    sharedSkin.setSkin(chosen);
    skell::theme::setSkin(chosen);
    const auto fore = skell::theme::colour(skell::theme::bone);
    const auto bg = skell::theme::colour(skell::theme::tile);
    const auto line = skell::theme::colour(skell::theme::border);
    const auto accent = skell::theme::colour(skell::theme::neon);
    setColour(juce::ComboBox::textColourId, fore);
    setColour(juce::ComboBox::backgroundColourId, bg);
    setColour(juce::ComboBox::outlineColourId, line);
    setColour(juce::ComboBox::arrowColourId, accent);
    setColour(juce::PopupMenu::backgroundColourId, bg);
    setColour(juce::PopupMenu::textColourId, fore);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour(juce::PopupMenu::highlightedTextColourId, skell::theme::colour(skell::theme::bone));
    setColour(juce::Slider::thumbColourId, accent);
    setColour(juce::Slider::rotarySliderFillColourId, accent);
    setColour(juce::Slider::textBoxTextColourId, fore);
    setColour(juce::Slider::textBoxBackgroundColourId, bg);
    setColour(juce::TextEditor::textColourId, fore);
    setColour(juce::TextEditor::backgroundColourId, bg);
    setColour(juce::TextEditor::highlightColourId, accent.withAlpha(.35f));
    setColour(juce::TextButton::textColourOffId, fore);
    setColour(juce::TextButton::textColourOnId, fore);
}
void SkellLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) {
    const auto r = b.getLocalBounds().toFloat().reduced(1.6f, 2.f);
    const bool on = b.getToggleState();
    if (on) {
        g.setColour(skell::theme::colour(skell::theme::neon).withAlpha(.20f));
        g.fillRoundedRectangle(r.expanded(2.f), 10.f);
    }
    skell::theme::raised(g, r, std::min(11.f,r.getHeight()*.29f), on, down);
    if (hover && !on) {
        g.setColour(skell::theme::colour(skell::theme::pale).withAlpha(.13f));
        g.fillRoundedRectangle(r.reduced(2.f), 8.f);
    }
}
void SkellLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) {
    g.setColour(skell::theme::colour(skell::theme::bone));
    const float h = static_cast<float>(b.getHeight());
    g.setFont(skell::theme::display(h >= 40.f ? 15.3f : h >= 29.f ? 12.7f : 11.f));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(3,2),
                     juce::Justification::centred, 1, .75f);
}
void SkellLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float start, float end, juce::Slider& slider) {
    if (w < 20 || h < 20) return;
    const auto b = juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),
                                           static_cast<float>(w),static_cast<float>(h));
    const float size = std::min(b.getWidth(),b.getHeight());
    const float outer = std::max(15.f,size*.43f);
    const auto c = b.getCentre();
    const float angle = start + std::clamp(std::isfinite(pos) ? pos : 0.f,0.f,1.f)*(end-start);
    const auto accent = skell::theme::colour(skell::theme::neon);
    const auto line = skell::theme::colour(skell::theme::border);
    const auto face = skell::theme::colour(skell::theme::tile);
    // Recessed shadow, three machined metallic rims, inner ceramic face.
    g.setColour(juce::Colours::black.withAlpha(.24f));
    g.fillEllipse(c.x-outer-3.f,c.y-outer+5.f,2*(outer+3.f),2*(outer+3.f));
    g.setGradientFill(juce::ColourGradient(face.brighter(.24f),c.x-outer,c.y-outer,
                                          line.darker(.25f),c.x+outer,c.y+outer,false));
    g.fillEllipse(c.x-outer,c.y-outer,2*outer,2*outer);
    g.setColour(line.withAlpha(.95f)); g.drawEllipse(c.x-outer,c.y-outer,2*outer,2*outer,1.1f);
    g.setColour(juce::Colours::white.withAlpha(.58f));
    g.drawEllipse(c.x-outer+3,c.y-outer+3,2*(outer-3),2*(outer-3),1.2f);
    const float trackR = outer*.82f;
    juce::Path track, active;
    track.addCentredArc(c.x,c.y,trackR,trackR,0.f,start,end,true);
    active.addCentredArc(c.x,c.y,trackR,trackR,0.f,start,angle,true);
    g.setColour(line.darker(.26f)); g.strokePath(track,juce::PathStrokeType(7.2f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setColour(accent.darker(.6f)); g.strokePath(active,juce::PathStrokeType(8.f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setColour(accent); g.strokePath(active,juce::PathStrokeType(5.1f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    const float dial = outer*.65f;
    g.setColour(line.darker(.22f)); g.fillEllipse(c.x-dial-2,c.y-dial-1,2*(dial+2),2*(dial+2));
    g.setGradientFill(juce::ColourGradient(face.brighter(.22f), c.x-dial*.5f,c.y-dial*.8f,
                                          face.darker(.15f),c.x+dial*.68f,c.y+dial*.93f,true));
    g.fillEllipse(c.x-dial,c.y-dial,2*dial,2*dial);
    g.setColour(juce::Colours::white.withAlpha(.40f));
    g.drawEllipse(c.x-dial+2,c.y-dial+2,2*(dial-2),2*(dial-2),1.25f);
    const auto tip = c.getPointOnCircumference(trackR,angle);
    g.setColour(accent.darker(.7f)); g.fillEllipse(tip.x-5.2f,tip.y-5.2f,10.4f,10.4f);
    g.setColour(accent.brighter(.12f)); g.fillEllipse(tip.x-3.8f,tip.y-3.8f,7.6f,7.6f);
    const auto name = slider.getName();
    const double value = slider.getValue();
    const juce::String text = (name=="FADE IN" || name=="FADE OUT")
        ? (value < .5 ? juce::String("Inst") : juce::String(juce::roundToInt(value))+" ms")
        : juce::String(juce::roundToInt(value))+"%";
    g.setColour(skell::theme::colour(skell::theme::bone));
    g.setFont(skell::theme::display(outer > 53.f ? 21.f : 17.f));
    g.drawFittedText(text, juce::Rectangle<int>(static_cast<int>(c.x-dial+7),
       static_cast<int>(c.y-13),static_cast<int>((dial-7)*2),26),juce::Justification::centred,1);
}
void SkellLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) {
    skell::theme::raised(g,juce::Rectangle<float>(1.f,1.f,static_cast<float>(w-2),static_cast<float>(h-4)),7.f);
    g.setColour(skell::theme::colour(skell::theme::dim));
    juce::Path triangle;
    triangle.addTriangle(static_cast<float>(w-18),h*.41f,static_cast<float>(w-8),h*.41f,
                         static_cast<float>(w-13),h*.62f);
    g.fillPath(triangle);
}
void SkellLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int w, int h) {
    g.fillAll(skell::theme::colour(skell::theme::tile));
    g.setColour(skell::theme::colour(skell::theme::border));
    g.drawRect(0,0,w,h);
}
juce::Font SkellLookAndFeel::getComboBoxFont(juce::ComboBox&) { return skell::theme::regular(13.f); }
