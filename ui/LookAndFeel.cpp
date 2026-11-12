#include "LookAndFeel.h"
#include <cmath>
#include <algorithm>
SkellLookAndFeel::SkellLookAndFeel() : sharedSkin(skellcore::ui::StudioSkinStore::file().existsAsFile()
        ? skellcore::ui::StudioSkinStore::load()
        : skellcore::ui::StudioSkin::Obsidian) {
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
    // All four selected skin accents need dark text for legibility.
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xff131811));
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
    if (on && !skell::theme::matteBlack()) {
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
    g.setColour(b.getToggleState() && !skell::theme::matteBlack()
                   ? juce::Colour(0xff121910)
                   : skell::theme::colour(skell::theme::bone));
    const float h = static_cast<float>(b.getHeight());
    g.setFont(skell::theme::display(h >= 40.f ? 15.3f : h >= 29.f ? 12.7f : 11.f));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(3,2),
                     juce::Justification::centred, 1, .75f);
}
void SkellLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float start, float end, juce::Slider& slider) {
    if (w < 24 || h < 24) return;
    const auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h);
    const auto c = bounds.getCentre();
    const float size = std::min(bounds.getWidth(), bounds.getHeight());
    const float outer = size * .455f;
    const float fraction = std::clamp(std::isfinite(pos) ? pos : 0.f, 0.f, 1.f);
    const float angle = start + fraction * (end - start);
    const auto neon = skell::theme::colour(skell::theme::neon);
    const auto border = skell::theme::colour(skell::theme::border);
    const bool darkSkin = skell::theme::matteBlack();
    const auto bronze = darkSkin
        ? juce::Colour(0xff28312a) : skell::theme::colour(skell::theme::tile);
    const auto ink = skell::theme::colour(skell::theme::bone);
    const bool fade = slider.getName() == "FADE IN" || slider.getName() == "FADE OUT";
    const auto ellipse = [&](float radius) {
        return juce::Rectangle<float>(c.x-radius,c.y-radius,2.f*radius,2.f*radius);
    };
    // A soft cast shadow, not the old hard gray crescent.
    for (int i=5; i>=1; --i) {
        g.setColour(juce::Colours::black.withAlpha(.017f + .009f*(5-i)));
        g.fillEllipse(ellipse(outer + i*.72f).translated(0,3.6f));
    }
    // Secondary cast shadow makes the barrel stand proud of the faceplate.
    if (darkSkin) {
        g.setColour(juce::Colours::black.withAlpha(.36f));
        g.fillEllipse(ellipse(outer+1.1f).translated(.8f,5.f));
        g.setColour(juce::Colour(0xff060a07));
        g.fillEllipse(ellipse(outer+.8f).translated(.3f,2.f));
    }
    // Multi-depth steel barrel: a dark chassis inset, knurled side grip,
    // machined rim, luminous instrument scale and a high-contrast brushed cap.
    g.setColour(border.darker(.42f));
    g.fillEllipse(ellipse(outer+.6f));
    g.setGradientFill(juce::ColourGradient(
        bronze.brighter(darkSkin ? .16f : .42f),c.x-outer*.74f,c.y-outer*.9f,
        bronze.darker(darkSkin ? .39f : .29f),c.x+outer*.84f,c.y+outer*.92f,false));
    g.fillEllipse(ellipse(outer-.9f));
    // Alternating notches on the barrel make the dial look grippable and
    // physically heavy instead of like a flat gray disc. Keep them outside
    // the green value track so they never obscure the automation marker.
    if (darkSkin) {
        for (int tooth=0;tooth<88;++tooth) {
            const float a=juce::MathConstants<float>::twoPi * float(tooth) / 88.f;
            const auto inner=c.getPointOnCircumference(outer-6.6f,a);
            const auto outerPoint=c.getPointOnCircumference(outer-1.6f,a+.009f);
            const float lit=std::sin(a+1.1f)*.5f+.5f;
            g.setColour((tooth%2 ? juce::Colours::black : juce::Colour(0xffbcc4b7))
                         .withAlpha(tooth%2 ? .50f : .15f+.16f*lit));
            g.drawLine(inner.x,inner.y,outerPoint.x,outerPoint.y,
                       tooth%2 ? 1.6f : .9f);
        }
    }
    g.setColour(juce::Colours::white.withAlpha(skell::theme::ivory() ? .56f : .24f));
    g.drawEllipse(ellipse(outer-2.5f),1.25f);
    if (darkSkin) {
        juce::Path rimHighlight;
        rimHighlight.addCentredArc(c.x,c.y,outer-2.3f,outer-2.3f,0.f,
              juce::MathConstants<float>::pi*1.03f,
              juce::MathConstants<float>::pi*1.79f,true);
        g.setColour(juce::Colour(0xffb3c3b4).withAlpha(.55f));
        g.strokePath(rimHighlight,juce::PathStrokeType(1.25f));
        juce::Path lowRim;
        lowRim.addCentredArc(c.x,c.y,outer-2.9f,outer-2.9f,0.f,
             .25f,2.72f,true);
        g.setColour(juce::Colours::black.withAlpha(.69f));
        g.strokePath(lowRim,juce::PathStrokeType(2.f));
    }
    // A single recessed bronze shoulder gives the knob its machined depth,
    // without the stack of thin 'washer' lines from the earlier renderer.
    g.setColour(darkSkin ? juce::Colour(0xff111913) : border.darker(.32f));
    g.fillEllipse(ellipse(outer-5.2f));
    g.setGradientFill(juce::ColourGradient(
        darkSkin ? juce::Colour(0xff737d70) : bronze.brighter(.34f),
        c.x-outer*.61f, c.y-outer*.71f,
        darkSkin ? juce::Colour(0xff101812) : bronze.darker(.30f),
        c.x+outer*.69f, c.y+outer*.86f, false));
    g.fillEllipse(ellipse(outer-7.0f));

    // Fine radial machining marks sit inside the outer metal lip, never
    // over the value arc or the knob's centre cap.
    for (int tick = 0; tick < 48; ++tick) {
        const float a = juce::MathConstants<float>::twoPi * static_cast<float>(tick) / 48.f;
        const auto from = c.getPointOnCircumference(outer - 5.6f, a);
        const auto to = c.getPointOnCircumference(outer - 4.1f, a);
        g.setColour(juce::Colours::white.withAlpha(darkSkin ? .13f : .20f));
        g.drawLine(from.x, from.y, to.x, to.y, .65f);
    }
    const float ringRadius = outer * .78f;
    juce::Path track, progress;
    track.addCentredArc(c.x,c.y,ringRadius,ringRadius,0.f,start,end,true);
    progress.addCentredArc(c.x,c.y,ringRadius,ringRadius,0.f,start,angle,true);
    const juce::PathStrokeType roundTrack(5.4f,juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded);
    const juce::PathStrokeType lightTrack(4.4f,juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded);
    // The dark unlit track has the deep olive / bronze treatment in the
    // reference, rather than an oversized gray band.
    g.setColour(darkSkin ? juce::Colour(0xff020702) : border.darker(.67f));
    g.strokePath(track,roundTrack);
    g.setColour(skell::theme::colour(skell::theme::dim).darker(.15f).withAlpha(.82f));
    g.strokePath(track,juce::PathStrokeType(3.5f,juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
    // The fade knob's short neon arc is an illuminated backplate. The small
    // moving dot remains the honest automation position at Inst / 0 ms.
    juce::Path illuminated = progress;
    if (fade) {
        illuminated.clear();
        illuminated.addCentredArc(c.x,c.y,ringRadius,ringRadius,0.f,
                                 start,start+(end-start)*.715f,true);
    }
    g.setColour(neon.darker(.46f).withAlpha(.65f));
    g.strokePath(illuminated,juce::PathStrokeType(7.f,juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    g.setColour(neon);
    g.strokePath(illuminated,lightTrack);
    g.setColour(neon.brighter(.3f).withAlpha(.63f));
    g.strokePath(illuminated,juce::PathStrokeType(1.15f,juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

    // The dial is a raised ceramic-metal cap with an engraved outer groove.
    const float cap = outer*.69f;
    // A pronounced cast shadow under the raised center is the key part of
    // the reference's retro knob: it must look like a cap, not a flat washer.
    g.setColour(juce::Colours::black.withAlpha(darkSkin ? .76f : .33f));
    g.fillEllipse(ellipse(cap+3.f).translated(1.5f,3.7f));
    if (darkSkin) {
        g.setColour(juce::Colour(0xff0a100c));
        g.fillEllipse(ellipse(cap+2.0f));
    }
    g.setColour(border.darker(.37f));
    g.fillEllipse(ellipse(cap+1.f));
    g.setColour(darkSkin ? juce::Colour(0xff707d71) : bronze.brighter(.32f));
    g.fillEllipse(ellipse(cap));
    g.setGradientFill(juce::ColourGradient(
        darkSkin ? juce::Colour(0xff85958a) : bronze.brighter(.48f),
        c.x-cap*.77f,c.y-cap*.86f,
        darkSkin ? juce::Colour(0xff141d18) : bronze.darker(.14f),
        c.x+cap*.74f,c.y+cap*.94f,false));
    g.fillEllipse(ellipse(cap-.8f));
    if (darkSkin) {
        // Concentric machining is subtle enough to preserve a clean value
        // readout, but gives the steel cap real radial material complexity.
        for (int groove=0;groove<17;++groove) {
            const float r=cap-3.f-float(groove)*1.25f;
            if (r <= 9.f) break;
            g.setColour((groove%2 ? juce::Colours::black : juce::Colours::white)
                         .withAlpha(groove%2 ? .058f : .047f));
            g.drawEllipse(ellipse(r),.43f);
        }
        // Tiny upper-left specular sweep, not a broad gray fog over the dial.
        g.setColour(juce::Colours::white.withAlpha(.23f));
        juce::Path highlight;
        highlight.addCentredArc(c.x,c.y,cap-1.7f,cap-1.7f,0.f,
             juce::MathConstants<float>::pi*1.04f,
             juce::MathConstants<float>::pi*1.75f,true);
        g.strokePath(highlight,juce::PathStrokeType(1.7f));
        g.setColour(juce::Colours::white.withAlpha(.095f));
        g.fillEllipse(c.x-cap*.53f,c.y-cap*.72f,cap*.66f,cap*.20f);
    }
    g.setColour(juce::Colours::white.withAlpha(skell::theme::ivory() ? .59f : .21f));
    g.drawEllipse(ellipse(cap-2.0f),1.12f);
    g.setColour(border.withAlpha(.15f));
    g.drawEllipse(ellipse(cap-3.3f),.52f);
    // The inlaid pointer rotates with the actual value. Its direction
    // matches the ring marker, so the dial never appears lopsided or fake.
    const auto pointerStart = c.getPointOnCircumference(cap * .56f, angle);
    const auto pointerEnd = c.getPointOnCircumference(cap * .88f, angle);
    g.setColour(juce::Colours::black.withAlpha(.65f));
    g.drawLine(pointerStart.x + .7f, pointerStart.y + .9f,
               pointerEnd.x + .7f, pointerEnd.y + .9f, 4.f);
    g.setColour(neon.withAlpha(darkSkin ? 1.f : .83f));
    g.drawLine(pointerStart.x, pointerStart.y, pointerEnd.x, pointerEnd.y, 2.1f);
    // Position pin always follows the real parameter, including at the
    // Instant fade setting; it never follows the decorative illumination.
    const auto tip=c.getPointOnCircumference(ringRadius,angle);
    g.setColour(border.darker(.47f));
    g.fillEllipse(tip.x-4.8f,tip.y-3.9f,9.6f,9.6f);
    g.setColour(neon.brighter(.1f));
    g.fillEllipse(tip.x-3.8f,tip.y-3.3f,7.6f,7.6f);
    const double value = slider.getValue();
    const auto text = fade
        ? (value < .5 ? juce::String("Inst") : juce::String(juce::roundToInt(value))+" ms")
        : juce::String(juce::roundToInt(value))+"%";
    g.setColour(ink.withAlpha(.97f));
    g.setFont(skell::theme::display(size > 130 ? 18.6f : 17.1f));
    g.drawFittedText(text,juce::Rectangle<int>((int)(c.x-cap+6),(int)(c.y-13),
        (int)(2*cap-12),26),juce::Justification::centred,1);
}
void SkellLookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) {
    skell::theme::raised(g,juce::Rectangle<float>(1.f,1.f,static_cast<float>(w-2),static_cast<float>(h-4)),7.f);
    g.setColour(skell::theme::colour(skell::theme::matteBlack() ? skell::theme::neon : skell::theme::dim));
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
