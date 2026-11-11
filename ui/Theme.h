#pragma once
// SkellGlitch shared SkellCore skins; matte-black vintage Obsidian is the new-install default.
// UI-only; no painting or file I/O from the audio callback.
#include <JuceHeader.h>
#include <SkellCore/StudioUI.h>
#include <SkellCore/SkullTheme.h>
#include <atomic>
#include <algorithm>
#include <cmath>
namespace skell::theme {
constexpr int width = 960, height = 720;
constexpr auto background = 0xffdac5a9;
constexpr auto tile       = 0xffe5d3bb;
constexpr auto deep       = 0xffc5b094;
constexpr auto border     = 0xff7b6955;
constexpr auto neon       = 0xff54fa12;
constexpr auto pale       = 0xffa0ff50;
constexpr auto dim        = 0xff2c8a0b;
constexpr auto magenta    = 0xffce3290;
constexpr auto cyan       = 0xff3cae72;
constexpr auto bone       = 0xff2b2924;
constexpr auto gray       = 0xff594f44;
inline std::atomic<int>& skinStorage() { static std::atomic<int> choice{1}; return choice; }
inline void setSkin(skellcore::ui::StudioSkin s) { skinStorage().store(static_cast<int>(s), std::memory_order_relaxed); }
inline skellcore::ui::StudioSkin skin() { return skellcore::ui::studioSkinFromIndex(skinStorage().load(std::memory_order_relaxed)); }
inline bool ivory() { return skin() == skellcore::ui::StudioSkin::Ivory; }
inline bool matteBlack() { return skin() == skellcore::ui::StudioSkin::Obsidian; }
inline juce::Colour colour(juce::uint32 token) {
    using C = juce::Colour;
    if (ivory()) return C(token);
    if (matteBlack()) {
        // A dark, warm, subtly aged hardware variant of SkellCore Obsidian.
        // The saved shared skin index and the other three skins are untouched.
        switch (token) {
            case background: return C(0xff090b0b);
            case tile: return C(0xff20221f);
            case deep: return C(0xff33342f);
            case border: return C(0xff67645b);
            case neon: return C(0xff57fb1e);
            case pale: return C(0xffa3ff75);
            case dim: return C(0xff2a9b18);
            case magenta: return C(0xffca6595);
            case cyan: return C(0xff4aae70);
            case bone: return C(0xffeeece2);
            case gray: return C(0xffb6b3a8);
        }
    }
    const auto p = skellcore::ui::studioPalette(skin());
    switch (token) {
        case background: return p.background;
        case tile: return p.surface;
        case deep: return p.raised;
        case border: return p.border;
        case neon: return p.accent;
        case pale: return p.metalLight;
        case dim: return p.metalDark;
        case magenta: return p.accent.interpolatedWith(C(0xffcc559a), .48f);
        case cyan: return p.accent.withMultipliedBrightness(.75f);
        case bone: return p.foreground;
        case gray: return p.secondary;
        default: return C(token);
    }
}
inline juce::Font regular(float size) { return juce::Font(juce::FontOptions("Arial", size, juce::Font::plain)); }
inline juce::Font display(float size) { return juce::Font(juce::FontOptions("Arial", size, juce::Font::bold)); }
inline juce::Font mono(float size) { return juce::Font(juce::FontOptions("Consolas", size, juce::Font::plain)); }
inline void raised(juce::Graphics& g, juce::Rectangle<float> bounds, float radius, bool selected=false, bool pressed=false) {
    if (bounds.isEmpty()) return;
    // Recessed shadow and warm machined relief, restrained and legible at 50%.
    g.setColour(juce::Colours::black.withAlpha(.26f));
    g.fillRoundedRectangle(bounds.translated(0.f, 2.5f), radius);
    const auto c = colour(selected ? neon : tile);
    juce::ColourGradient gradient(c.brighter(selected ? .20f : .18f),
                                  bounds.getCentreX(), bounds.getY(),
                                  c.darker(selected ? .16f : .24f),
                                  bounds.getCentreX(), bounds.getBottom(), false);
    if (pressed) gradient = juce::ColourGradient(c.darker(.13f),bounds.getCentreX(),bounds.getY(),
                                                 c.brighter(.05f),bounds.getCentreX(),bounds.getBottom(),false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(colour(selected ? dim : border).withAlpha(selected ? .95f : .85f));
    g.drawRoundedRectangle(bounds.reduced(.5f), radius, 1.35f);
    g.setColour(juce::Colours::white.withAlpha(selected ? .48f : (ivory() ? .57f : .16f)));
    g.drawRoundedRectangle(bounds.reduced(1.6f), std::max(0.f,radius-1.2f), .9f);
    g.setColour(juce::Colours::black.withAlpha(.16f));
    g.drawLine(bounds.getX()+radius*.55f, bounds.getBottom()-2.f,
               bounds.getRight()-radius*.55f, bounds.getBottom()-2.f, 1.f);
}
inline void roundTile(juce::Graphics& g, juce::Rectangle<float> bounds, float corner=9.f) {
    raised(g,bounds,corner,false,false);
}
inline const juce::Rectangle<int> top{0,0,width,59};
inline const juce::Rectangle<int> loops{22,72,924,71};
inline const juce::Rectangle<int> wave{207,152,546,233};
inline const juce::Rectangle<int> gate{210,393,545,85};
inline const juce::Rectangle<int> modes{209,489,585,47};
inline const juce::Rectangle<int> presets{205,541,560,32};
inline const juce::Rectangle<int> band{233,586,496,85};
}
