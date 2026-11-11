#include "TopBar.h"
#include "../PluginProcessor.h"
#include <cmath>
void SkellGlyphButton::paintButton(juce::Graphics& g, bool hover, bool down) {
    const auto r=getLocalBounds().toFloat().reduced(2.f);
    const auto neon=skell::theme::colour(skell::theme::neon);
    const auto border=skell::theme::colour(skell::theme::border);
    const auto bronze=skell::theme::colour(skell::theme::tile);
    const auto c=r.getCentre();
    const float radius=std::min(r.getWidth(),r.getHeight())*.49f;
    const auto ellipse=[&](float rad) {
        return juce::Rectangle<float>(c.x-rad,c.y-rad,2*rad,2*rad);
    };
    g.setColour(juce::Colours::black.withAlpha(.27f));
    g.fillEllipse(ellipse(radius+1.f).translated(.7f,2.5f));
    g.setColour(border.darker(.38f));
    g.fillEllipse(ellipse(radius));
    // Reference gear and power buttons are warm inset brass, not
    // neon-ringed badges; only the skull identity badge has a green rim.
    g.setGradientFill(juce::ColourGradient(
         bronze.brighter(.34f),c.x-radius*.7f,c.y-radius*.8f,
         bronze.darker(.24f),c.x+radius*.78f,c.y+radius*.95f,false));
    g.fillEllipse(ellipse(radius-1.3f));
    g.setColour(juce::Colours::white.withAlpha(.60f));
    g.drawEllipse(ellipse(radius-3.f),1.f);
    g.setColour(border.withAlpha(.37f));
    g.drawEllipse(ellipse(radius-4.7f),.7f);
    if(icon==Kind::Skull) {
        const float medallion=radius*.78f;
        g.setColour(neon.darker(.62f).withAlpha(.5f));
        g.fillEllipse(ellipse(medallion+2.f));
        g.setColour(neon.brighter(.15f));
        g.drawEllipse(ellipse(medallion+1.f),3.5f);
        g.setColour(juce::Colours::black.withAlpha(.97f));
        g.fillEllipse(ellipse(medallion-1.4f));
        juce::Graphics::ScopedSaveState save(g);
        const float sx=c.x-13.f,sy=c.y-14.4f;
        g.reduceClipRegion(juce::Rectangle<int>((int)(sx-2),(int)(sy-1),30,30));
        // Shared SkellCore master skull, cropped to the medallion silhouette;
        // jaw details are polished without creating a new brand skull.
        skellcore::ui::drawSkullKeyMark(g,sx,sy,neon,juce::Colours::black);
        g.setColour(juce::Colours::black);
        g.fillRoundedRectangle(c.x-9.3f,c.y+5.3f,18.6f,4.1f,1.f);
        for(int t=-2;t<=2;++t) {
            g.setColour(neon);
            g.fillRoundedRectangle(c.x+t*3.25f-.66f,c.y+5.6f,1.5f,4.1f,.5f);
        }
    } else if(icon==Kind::Settings) {
        // Dark bevel under each spoke gives the green, engraved mechanical
        // gear in the reference, rather than a flat neon sun icon.
        const auto olive=skell::theme::colour(skell::theme::dim).darker(.26f);
        for(int n=0;n<8;++n) {
            const float a=juce::MathConstants<float>::twoPi*n/8.f;
            const float dx=std::sin(a),dy=-std::cos(a);
            g.setColour(olive.darker(.48f));
            g.drawLine(c.x+dx*8.6f+1.f,c.y+dy*8.6f+1.f,
                       c.x+dx*12.f+1.f,c.y+dy*12.f+1.f,3.2f);
            g.setColour(olive);
            g.drawLine(c.x+dx*8.6f,c.y+dy*8.6f,
                       c.x+dx*12.f,c.y+dy*12.f,2.6f);
        }
        g.setColour(olive.darker(.47f));
        g.fillEllipse(ellipse(9.7f));
        g.setColour(neon.darker(.3f));
        g.fillEllipse(ellipse(8.2f));
        g.setColour(olive);
        g.fillEllipse(ellipse(5.7f));
        g.setColour(neon.brighter(.22f));
        g.drawEllipse(ellipse(5.7f),1.1f);
        g.setColour(olive.darker(.52f));
        g.fillEllipse(ellipse(2.7f));
    } else {
        const auto olive=skell::theme::colour(skell::theme::dim).darker(.35f);
        juce::Path powerArc;
        powerArc.addCentredArc(c.x,c.y+1.f,9.f,9.f,0.f,.68f,
                              juce::MathConstants<float>::twoPi-.68f,true);
        const juce::PathStrokeType thick(3.7f,juce::PathStrokeType::curved,
                                        juce::PathStrokeType::rounded);
        g.setColour(juce::Colours::black.withAlpha(.49f));
        g.strokePath(powerArc,juce::PathStrokeType(5.f));
        g.drawLine(c.x+1.3f,c.y-11.8f,c.x+1.3f,c.y+.5f,4.5f);
        g.setColour(olive);
        g.strokePath(powerArc,thick);
        g.drawLine(c.x,c.y-11.f,c.x,c.y+.5f,3.5f);
        g.setColour(neon.darker(.24f).withAlpha(.83f));
        g.strokePath(powerArc,juce::PathStrokeType(1.15f));
        g.drawLine(c.x-.8f,c.y-10.1f,c.x-.8f,c.y-1.f,1.f);
    }
    if(hover || down) {
        g.setColour(neon.withAlpha(down?.12f:.055f));
        g.fillEllipse(ellipse(radius-5.f));
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
