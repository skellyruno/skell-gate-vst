#pragma once
#include <JuceHeader.h>
namespace skell::theme {
constexpr int width = 958, height = 676;
constexpr auto background = 0xff0a0d0b;
constexpr auto tile = 0xff121815;
constexpr auto deep = 0xff06100b;
constexpr auto border = 0xff1e2a22;
constexpr auto neon = 0xff39ff3a;
constexpr auto pale = 0xff8dff8a;
constexpr auto dim = 0xff135c1a;
constexpr auto magenta = 0xffff2fb3;
constexpr auto cyan = 0xff2fd9ff;
constexpr auto bone = 0xffe8e6da;
constexpr auto gray = 0xff8a9a90;
inline juce::Colour colour(juce::uint32 rgba) { return juce::Colour(rgba); }
inline juce::Font regular(float size) { return juce::Font("Arial", size, juce::Font::plain); }
inline juce::Font display(float size) { return juce::Font("Arial", size, juce::Font::bold); }
inline juce::Font mono(float size) { return juce::Font("Consolas", size, juce::Font::plain); }
inline void roundTile(juce::Graphics& g, juce::Rectangle<float> r, float corner = 9.f) {
    g.setColour(colour(tile)); g.fillRoundedRectangle(r, corner);
    g.setColour(colour(border)); g.drawRoundedRectangle(r.reduced(.5f), corner, 1.f);
}
inline const juce::Rectangle<int> top{0, 0, width, 52};
inline const juce::Rectangle<int> loops{9, 63, 940, 62};
inline const juce::Rectangle<int> wave{219, 158, 520, 239};
inline const juce::Rectangle<int> gate{219, 403, 520, 82};
inline const juce::Rectangle<int> modes{210, 491, 541, 48};
inline const juce::Rectangle<int> presets{218, 541, 521, 28};
inline const juce::Rectangle<int> band{243, 575, 451, 75};
}
