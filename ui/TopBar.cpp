#include "TopBar.h"
#include "../PluginProcessor.h"
#include <cmath>
void SkellGlyphButton::paintButton(juce::Graphics& g, bool hover, bool down) {
    auto r = getLocalBounds().toFloat().reduced(2.f);
    const auto neon = skell::theme::colour(skell::theme::neon);
    const auto frame = skell::theme::colour(skell::theme::border);
    const auto face = skell::theme::colour(skell::theme::tile);
    auto c = r.getCentre();
    const float radius = std::min(r.getWidth(),r.getHeight())*.50f;
    g.setColour(juce::Colours::black.withAlpha(.24f));
    g.fillEllipse(c.x-radius,c.y-radius+3.f,2*radius,2*radius);
    g.setGradientFill(juce::ColourGradient(face.brighter(.26f),c.x-radius,c.y-radius,
                                          frame.darker(.08f),c.x+radius,c.y+radius,true));
    g.fillEllipse(c.x-radius,c.y-radius,2*radius,2*radius);
    g.setColour(frame);g.drawEllipse(c.x-radius,c.y-radius,2*radius,2*radius,1.4f);
    g.setColour(juce::Colours::white.withAlpha(.57f));
    g.drawEllipse(c.x-radius+2,c.y-radius+2,2*(radius-2),2*(radius-2),1.f);
    g.setColour(neon);
    const float inner = radius * (icon==Kind::Skull ? .73f : .72f);
    g.drawEllipse(c.x-inner,c.y-inner,2*inner,2*inner,icon==Kind::Skull ? 3.1f : 1.5f);
    if (icon==Kind::Skull) {
        g.setColour(skell::theme::colour(skell::theme::dim));
        g.fillEllipse(c.x-inner+3,c.y-inner+3,2*(inner-3),2*(inner-3));
        // Same reusable skull geometry defined in the SkellCore master asset.
        // Clip away its key shaft to fit the circular medallion in the reference.
        juce::Graphics::ScopedSaveState saved(g);
        const float sx = c.x - 12.5f, sy = c.y - 15.0f;
        g.reduceClipRegion(juce::Rectangle<int>(static_cast<int>(sx),
                           static_cast<int>(sy),26,29));
        skellcore::ui::drawSkullKeyMark(g,sx,sy,neon,
                    skell::theme::colour(skell::theme::deep).darker(.6f));
    } else if (icon==Kind::Power) {
        juce::Path arc;
        arc.addCentredArc(c.x,c.y+1.f,9.f,9.f,0.f,.70f,
                          juce::MathConstants<float>::twoPi-.70f,true);
        g.strokePath(arc,juce::PathStrokeType(2.8f,
                     juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.drawLine(c.x,c.y-11.f,c.x,c.y+1.f,2.8f);
    } else {
        g.drawEllipse(c.x-6.5f,c.y-6.5f,13.f,13.f,2.3f);
        g.drawEllipse(c.x-2.3f,c.y-2.3f,4.6f,4.6f,1.6f);
        for(int i=0;i<8;++i) {
            const float a=i*juce::MathConstants<float>::twoPi/8.f;
            const float dx=std::cos(a),dy=std::sin(a);
            g.drawLine(c.x+dx*8.f,c.y+dy*8.f,c.x+dx*11.f,c.y+dy*11.f,2.f);
        }
    }
    if (hover||down) {
        g.setColour(neon.withAlpha(down?.25f:.09f));
        g.fillEllipse(c.x-inner,c.y-inner,2*inner,2*inner);
    }
}
TopBar::TopBar(SkellGlitchAudioProcessor& p) : processor(p) {
    addAndMakeVisible(skull);
    addAndMakeVisible(sync);
    addAndMakeVisible(power);
    addAndMakeVisible(settings);
    skull.setTooltip("SKELL appearance: click skull to cycle the shared SkellCore skins");
    settings.setTooltip("Engine settings and window sizes");
    power.setTooltip("Click-free effect bypass");
}
void TopBar::resized() {
    skull.setBounds(10,4,49,50);
    settings.setBounds(694,7,43,45);
    sync.setBounds(744,8,145,43);
    power.setBounds(900,7,46,46);
}
void TopBar::paint(juce::Graphics& g) {
    const auto green=skell::theme::colour(skell::theme::neon);
    const auto dark=skell::theme::colour(skell::theme::bone);
    const auto face=skell::theme::colour(skell::theme::tile);
    g.setGradientFill(juce::ColourGradient(face.brighter(.24f),0.f,0.f,
                                          skell::theme::colour(skell::theme::background).darker(.08f),
                                          0.f,59.f,false));
    g.fillAll();
    g.setColour(skell::theme::colour(skell::theme::border));
    g.drawLine(0.f,57.5f,static_cast<float>(getWidth()),57.5f,1.3f);
    g.setColour(juce::Colours::white.withAlpha(.42f));
    g.drawLine(0.f,55.8f,static_cast<float>(getWidth()),55.8f,.7f);
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::translation(static_cast<float>(jitterOffset),0.f));
        g.setFont(skell::theme::display(29.f));
        g.setColour(dark);g.drawText("Skell",62,8,85,43,juce::Justification::centredLeft);
        g.setColour(green.darker(.3f));
        g.drawText("Glitch",136+1,9,95,43,juce::Justification::centredLeft);
        g.setColour(green);g.drawText("Glitch",136,8,95,43,juce::Justification::centredLeft);
    }
    g.setColour(skell::theme::colour(skell::theme::gray));
    g.setFont(skell::theme::regular(11.f));
    g.drawText("CUT IT. FLIP IT.",249,21,157,18,juce::Justification::centredLeft);
    const double sr=processor.getSampleRate();
    const int latency=processor.displayLatency.load(std::memory_order_relaxed);
    g.setFont(skell::theme::mono(12.f));
    g.setColour(dark.withAlpha(.93f));
    g.drawText(juce::String(sr>0?sr/1000.0:48.0,1)+" kHz  •  "+juce::String(latency)+" samples",
               501,18,185,25,juce::Justification::centredRight);
}
