#pragma once
// SkellGlitch SkellCore skins: deep midnight-cyan Obsidian is the default.
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
        // Midnight-ocean blue variant of SkellCore Obsidian with bright cyan signal lighting.
        // The saved shared skin index and the other three skins are untouched.
        switch (token) {
            case background: return C(0xff080d19);
            case tile: return C(0xff121e30);
            case deep: return C(0xff1a2c42);
            case border: return C(0xff516b84);
            case neon: return C(0xff40e7ff);
            case pale: return C(0xffb4f6ff);
            case dim: return C(0xff197f9f);
            case magenta: return C(0xffd18be5);
            case cyan: return C(0xff48d9db);
            case bone: return C(0xffecf8ff);
            case gray: return C(0xffa7c0d5);
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
    const bool black = matteBlack();
    // Obsidian uses charcoal switches with one luminous edge, not slabs
    // of full-bright neon. Selected, unselected and hover all share a
    // restrained machined-metal contrast.
    if (black) {
        const auto accent = colour(neon);
        const auto metal = colour(tile);
        const auto edge = colour(border);
        g.setColour(juce::Colours::black.withAlpha(.76f));
        g.fillRoundedRectangle(bounds.translated(0, 2.f), radius);
        g.setGradientFill(juce::ColourGradient(
            edge.brighter(.36f), bounds.getX(), bounds.getY(),
            juce::Colour(0xff09101e), bounds.getRight(), bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, radius);
        auto face = bounds.reduced(2.6f, 2.8f);
        g.setGradientFill(juce::ColourGradient(
            pressed ? juce::Colour(0xff10233a) : metal.brighter(selected ? .33f : .13f),
            face.getCentreX(), face.getY(),
            selected ? juce::Colour(0xff10354a) : juce::Colour(0xff0a1220),
            face.getCentreX(), face.getBottom(), false));
        g.fillRoundedRectangle(face, std::max(2.f, radius-2.f));
        g.setColour(juce::Colours::white.withAlpha(.23f));
        g.drawLine(face.getX()+radius*.55f, face.getY()+1.4f,
                   face.getRight()-radius*.55f, face.getY()+1.4f, .9f);
        g.setColour(selected ? accent : edge.withAlpha(.64f));
        g.drawRoundedRectangle(bounds.reduced(.8f), radius, selected ? 1.7f : .85f);
        if (selected) {
            // Diffuse edge bloom + sharp cyan optical seam: only selected
            // buttons emit light, never an all-button wash.
            g.setColour(accent.withAlpha(.085f));
            g.fillRoundedRectangle(face, std::max(2.f, radius-2.f));
            g.setColour(accent.withAlpha(.20f));
            g.drawRoundedRectangle(bounds.expanded(1.6f), radius+1.f, 2.6f);
            g.setColour(accent.withAlpha(.72f));
            g.fillRoundedRectangle(face.getX()+8.f,face.getBottom()-2.1f,
                std::max(0.f,face.getWidth()-16.f),1.5f,.75f);
        } else {
            g.setColour(juce::Colours::black.withAlpha(.40f));
            g.drawLine(face.getX()+radius*.55f,face.getBottom()-1.2f,
                face.getRight()-radius*.55f,face.getBottom()-1.2f,1.f);
        }
        return;
    }
    const auto c = colour(selected ? neon : tile);
    juce::ColourGradient gradient(c.brighter(selected ? .16f : black ? .07f : .18f),
                                  bounds.getCentreX(), bounds.getY(),
                                  c.darker(selected ? .16f : black ? .14f : .24f),
                                  bounds.getCentreX(), bounds.getBottom(), false);
    if (pressed) gradient = juce::ColourGradient(c.darker(.13f),bounds.getCentreX(),bounds.getY(),
                                                 c.brighter(.05f),bounds.getCentreX(),bounds.getBottom(),false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, radius);
    g.setColour(colour(selected ? dim : border).withAlpha(selected ? .91f : (black ? .50f : .85f)));
    g.drawRoundedRectangle(bounds.reduced(.5f), radius, 1.35f);
    g.setColour(juce::Colours::white.withAlpha(selected ? (black ? .24f : .48f)
                                                      : (ivory() ? .57f : black ? .085f : .16f)));
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
inline const juce::Rectangle<int> modes{210,477,545,40};
inline const juce::Rectangle<int> presets{318,11,322,37};
inline const juce::Rectangle<int> band{233,562,496,85};
}
