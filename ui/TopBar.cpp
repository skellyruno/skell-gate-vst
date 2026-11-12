#include "TopBar.h"
#include "../PluginProcessor.h"
#include <cmath>
void SkellGlyphButton::paintButton(juce::Graphics& g, bool hover, bool down) {
    const auto r=getLocalBounds().toFloat().reduced(2.f);
    // Plugin-specific glitch treatment inherits the active SkellCore skin
    // accent; no separate green/cyan hard-code for the skull.
    const auto neon=skell::theme::colour(skell::theme::neon);
    const auto border=skell::theme::colour(skell::theme::border);
    const auto bronze=skell::theme::matteBlack()
        ? juce::Colour(0xff2f3e32) : skell::theme::colour(skell::theme::tile);
    const auto c=r.getCentre();
    const float radius=std::min(r.getWidth(),r.getHeight())*.49f;
    const auto ellipse=[&](float rad) {
        return juce::Rectangle<float>(c.x-rad,c.y-rad,2*rad,2*rad);
    };
    g.setColour(juce::Colours::black.withAlpha(.27f));
    g.fillEllipse(ellipse(radius+1.f).translated(.7f,2.5f));
    g.setColour(border.darker(.38f));
    g.fillEllipse(ellipse(radius));
    // Recessed hardware buttons share the same green/graphite material
    // language. The glitching skull remains a separate identity medallion.
    g.setGradientFill(juce::ColourGradient(
         bronze.brighter(.34f),c.x-radius*.7f,c.y-radius*.8f,
         bronze.darker(.24f),c.x+radius*.78f,c.y+radius*.95f,false));
    g.fillEllipse(ellipse(radius-1.3f));
    g.setColour(juce::Colours::white.withAlpha(skell::theme::ivory() ? .60f : .19f));
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
        // Narrow, tapered jaw and shorter teeth match the approved skull
        // medallion more closely while retaining the shared SkellCore mark.
        g.fillEllipse(c.x-15.1f,c.y+8.8f,7.f,6.f);
        g.fillEllipse(c.x+8.1f,c.y+8.8f,7.f,6.f);
        g.fillRoundedRectangle(c.x-7.5f,c.y+5.3f,15.f,3.8f,1.f);
        for(int t=-2;t<=2;++t) {
            g.setColour(neon);
            g.fillRoundedRectangle(c.x+t*2.6f-.55f,c.y+5.8f,1.15f,3.2f,.5f);
        }
        // The official SkellCore skull remains recognizable; displaced
        // horizontal scan bands are composited only inside this icon.
        // Static micro-fractures make its glitch identity obvious even
        // between rare, stronger UI-interference bursts.
        constexpr float bandY[] = {-10.3f, -3.9f, 2.4f, 8.5f, -6.8f};
        constexpr float bandDx[] = {3.5f, -2.8f, 2.5f, -2.0f, 5.6f};
        const int bandCount = glitchIntensity > 0 ? 5 : 4;
        for(int stripe=0;stripe<bandCount;++stripe) {
            const float y = c.y + bandY[stripe]
                + (glitchIntensity > 0 ? float((glitchVariant >> (stripe*3)) & 3u)-1.f : 0.f);
            const float dx = bandDx[stripe] +
                (glitchIntensity > 0
                    ? float(glitchIntensity)*float((glitchVariant >> (stripe*4)) & 1u ? 1.1f : -1.1f)
                    : 0.f);
            const float h = (stripe%2 == 0 ? 2.6f : 1.9f);
            g.setColour(juce::Colours::black.withAlpha(.94f));
            g.fillRect(juce::Rectangle<float>(c.x-13.0f,y,26.f,h));
            {
                juce::Graphics::ScopedSaveState slice(g);
                g.reduceClipRegion(juce::Rectangle<int>(
                    juce::roundToInt(c.x-13.f),
                    juce::roundToInt(y),26,juce::roundToInt(h)+1));
                skellcore::ui::drawSkullKeyMark(
                    g,sx+dx,sy,neon.withAlpha(.90f),juce::Colours::black);
            }
            g.setColour(neon.withAlpha(.52f + .14f*(stripe%2)));
            const float tailX=(stripe%2 == 0 ? c.x+10.5f : c.x-15.3f);
            g.fillRect(tailX,y+.45f,2.9f+float(stripe%3)*1.4f,.9f);
        }
        // Fine broken pixels around the eye and jaw edges: no cyan
        // chromatic aberration; the same Skell-green accent is used.
        g.setColour(neon.withAlpha(.75f));
        g.fillRect(c.x-14.8f,c.y-12.9f,4.7f,1.0f);
        g.fillRect(c.x+9.9f,c.y+10.1f,4.3f,1.0f);
        g.fillRect(c.x-10.8f,c.y+12.9f,2.0f,.9f);
        if (glitchIntensity > 0) {
            std::uint32_t noise=glitchVariant ^ 0x51f15a3du;
            for(int i=0;i<6+glitchIntensity*2;++i) {
                noise ^= noise<<13; noise ^= noise>>17; noise ^= noise<<5;
                const float x=c.x-14.5f+float(noise%29u);
                noise ^= noise<<13; noise ^= noise>>17; noise ^= noise<<5;
                const float y=c.y-12.f+float(noise%26u);
                g.setColour(neon.withAlpha(i%2 ? .56f : .96f));
                g.fillRect(x,y,1.0f+float(i%3),.8f);
            }
        }
    } else if(icon==Kind::Settings) {
        // Eight real gear teeth, not the old radial sunburst. Geometry is
        // proportional to the smaller SkellCore control and stays legible.
        juce::Path gear;
        constexpr int vertices = 32;
        for(int i=0;i<vertices;++i) {
            const float a=juce::MathConstants<float>::twoPi * float(i)/float(vertices)
                          - juce::MathConstants<float>::halfPi;
            const float toothed=(i % 4 == 1 || i % 4 == 2) ? .74f : .59f;
            const float px=c.x+std::cos(a)*radius*toothed;
            const float py=c.y+std::sin(a)*radius*toothed;
            if(i==0) gear.startNewSubPath(px,py);
            else gear.lineTo(px,py);
        }
        gear.closeSubPath();
        g.setColour(juce::Colours::black.withAlpha(.69f));
        g.fillPath(gear,juce::AffineTransform::translation(.6f,1.2f));
        g.setGradientFill(juce::ColourGradient(
            neon.brighter(.26f),c.x-radius*.55f,c.y-radius*.55f,
            neon.darker(.33f),c.x+radius*.55f,c.y+radius*.55f,false));
        g.fillPath(gear);
        g.setColour(neon.brighter(.14f).withAlpha(.8f));
        g.strokePath(gear,juce::PathStrokeType(.7f));
        g.setColour(bronze.darker(.48f));
        g.fillEllipse(ellipse(radius*.36f));
        g.setColour(neon.brighter(.18f));
        g.drawEllipse(ellipse(radius*.33f),1.15f);
    } else {
        const bool active = getToggleState();
        const auto olive = skell::theme::matteBlack()
            ? neon.darker(active ? .06f : .32f)
            : skell::theme::colour(skell::theme::dim).darker(.35f);
        if (active) {
            g.setColour(neon.withAlpha(.085f));
            g.fillEllipse(ellipse(radius - 5.f));
        }
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
    addAndMakeVisible(power);
    addAndMakeVisible(settings);
    settings.setName("HEADER_SETTINGS");
    power.setName("HEADER_POWER");
    skull.setTooltip("SKELL appearance: click skull to cycle the shared SkellCore skins");
    settings.setTooltip("Engine settings and window sizes");
    power.setTooltip("Click-free effect bypass");
}
void TopBar::resized() {
    skull.setBounds(10,4,49,50);
    // Smaller SkellCore hardware controls, 3px apart with clear header spacing.
    settings.setBounds(880, 11, 35, 36);
    power.setBounds(918, 11, 36, 36);
}
void TopBar::paint(juce::Graphics& g) {
    const auto green=skell::theme::colour(skell::theme::neon);
    const auto dark=skell::theme::colour(skell::theme::bone);
    const auto face=skell::theme::colour(skell::theme::tile);
    g.setGradientFill(juce::ColourGradient(
        skell::theme::matteBlack() ? face.brighter(.08f) : face.brighter(.24f),0.f,0.f,
                                          skell::theme::colour(skell::theme::background).darker(.08f),
                                          0.f,59.f,false));
    g.fillAll();
    g.setColour(skell::theme::colour(skell::theme::border));
    g.drawLine(0.f,57.5f,static_cast<float>(getWidth()),57.5f,1.3f);
    g.setColour(juce::Colours::white.withAlpha(skell::theme::matteBlack() ? .14f : .42f));
    g.drawLine(0.f,55.8f,static_cast<float>(getWidth()),55.8f,.7f);
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.addTransform(juce::AffineTransform::translation(static_cast<float>(jitterOffset),0.f));
        // A single uppercase word. A compact secondary line carries the
        // vintage front-panel slogan; version is engraved alongside the name.
        g.setFont(skell::theme::display(25.5f));
        g.setColour(dark);
        g.drawText("SKELL",62,7,83,30,juce::Justification::centredLeft);
        g.setColour(green.darker(.32f));
        g.drawText("GLITCH",138,8,102,30,juce::Justification::centredLeft);
        g.setColour(green);
        g.drawText("GLITCH",137,7,102,30,juce::Justification::centredLeft);
    }
    if (glitchIntensity > 0 && skell::theme::matteBlack()) {
        // Tiny occasional displacement of the title's green signal and a
        // few packet-loss bars. This is visual only, not a color/theme switch.
        juce::Graphics::ScopedSaveState burst(g);
        const float y=11.f+float(glitchVariant%19u);
        g.reduceClipRegion(juce::Rectangle<int>(132,juce::roundToInt(y),123,4));
        g.setColour(green.brighter(.27f).withAlpha(.80f));
        g.setFont(skell::theme::display(25.5f));
        const float dx=float(glitchIntensity)*1.8f;
        g.drawText("GLITCH",juce::roundToInt(137.f+dx),7,102,30,
                   juce::Justification::centredLeft);
    }
    if (glitchIntensity > 0 && skell::theme::matteBlack()) {
        std::uint32_t bits=glitchVariant ^ 0xd3c4f2a1u;
        for (int i=0;i<5;++i) {
            bits ^= bits<<13; bits ^= bits>>17; bits ^= bits<<5;
            const float x=105.f+float(bits%156u);
            const float y=5.f+float((bits>>8)%45u);
            g.setColour(green.withAlpha(.32f+.11f*glitchIntensity));
            g.fillRect(x,y,5.f+float((bits>>16)%19u),.9f);
        }
    }
    g.setColour(skell::theme::colour(skell::theme::gray));
    g.setFont(skell::theme::regular(10.5f));
    g.drawText("CUT IT. FLIP IT.",63,38,170,14,juce::Justification::centredLeft);
    // Small engraved edition mark directly beside the one-word title.
    g.setColour(skell::theme::colour(skell::theme::gray).withAlpha(.88f));
    g.setFont(skell::theme::mono(9.f));
    g.drawText("v1.7.s",258,15,55,14,juce::Justification::centredLeft);
    const double sr=processor.getSampleRate();
    const int latency=processor.displayLatency.load(std::memory_order_relaxed);
    g.setFont(skell::theme::mono(12.f));
    g.setColour(dark.withAlpha(.93f));
    g.drawText(juce::String(sr>0?sr/1000.0:48.0,1)+" kHz  •  "+juce::String(latency)+" samples",
               653,18,206,25,juce::Justification::centredRight);
}
