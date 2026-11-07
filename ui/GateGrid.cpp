#include "GateGrid.h"
#include "../PluginProcessor.h"
#include "../dsp/GlitchRandomizer.h"
GateGrid::GateGrid(SkellGlitchAudioProcessor& p) : processor(p),
    commands{juce::TextButton("RANDOM"), juce::TextButton("◀ SHIFT"),
             juce::TextButton("SHIFT ▶"), juce::TextButton("INVERT"), juce::TextButton("CLEAR")} {
    for (int i = 0; i < 5; ++i) {
        addAndMakeVisible(commands[static_cast<size_t>(i)]);
        commands[static_cast<size_t>(i)].onClick = [this, i] { applyPattern(i); };
    }
    pattern.addItemList({"Straight", "Offbeat", "Trap Hats", "Triplet Stutter", "Half-Time",
        "Glitch 1", "Glitch 2", "Glitch 3", "Glitch 4", "Glitch 5", "Glitch 6", "Glitch 7", "Glitch 8"}, 1);
    pattern.setTextWhenNothingSelected("PATTERNS");
    pattern.onChange = [this] { if (pattern.getSelectedId() > 0) loadPattern(pattern.getSelectedId() - 1); };
    addAndMakeVisible(pattern);
    startTimerHz(60);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
void GateGrid::resized() {
    const int buttonWidth = (getWidth() - 140) / 5;
    for (int i = 0; i < 5; ++i) commands[static_cast<size_t>(i)].setBounds(i * buttonWidth + 1, 53, buttonWidth - 2, 27);
    pattern.setBounds(getWidth() - 138, 53, 136, 27);
}
void GateGrid::paint(juce::Graphics& g) {
    const int steps = std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()), 1, 32);
    const float cell = static_cast<float>(getWidth()) / steps;
    const int active = processor.displayStep.load();
    for (int i = 0; i < steps; ++i) {
        const auto name = "step" + juce::String(i + 1);
        const int state = static_cast<int>(processor.apvts.getRawParameterValue(name + "_state")->load());
        const float probability = processor.apvts.getRawParameterValue(name + "_prob")->load();
        const float x = i * cell + 2.f;
        const float w = cell - 4.f;
        const auto fill = state == 3 ? skell::theme::magenta :
                         state == 2 ? skell::theme::pale :
                         state == 1 ? skell::theme::neon : skell::theme::tile;
        g.setColour(skell::theme::colour(fill).withAlpha(state == 0 ? .9f : .77f));
        g.fillRoundedRectangle(x, state == 2 ? 0.f : 7.f, w, state == 2 ? 39.f : 32.f, 4.f);
        if (i == active) {
            g.setColour(skell::theme::colour(skell::theme::bone));
            g.drawRoundedRectangle(x, 0.f, w, 41.f, 4.f, 1.25f);
        }
        g.setColour(skell::theme::colour(skell::theme::gray).withAlpha(.30f + probability * .007f));
        g.fillEllipse(x + w * .5f - 2.f, 44.f, 4.f, 4.f);
    }
}
void GateGrid::setStep(int index, int value) {
    const auto key = "step" + juce::String(index + 1) + "_state";
    if (auto* p = processor.apvts.getParameter(key)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(value)));
        p->endChangeGesture();
    }
}
void GateGrid::edit(const juce::MouseEvent& e) {
    if (e.position.y >= 52.f) return;
    const int steps = std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()), 1, 32);
    const int idx = std::clamp(static_cast<int>(e.position.x / (getWidth() / static_cast<float>(steps))), 0, steps - 1);
    if (idx == edited && !probabilityMode) return;
    edited = idx;
    if (probabilityMode) {
        const auto key = "step" + juce::String(idx + 1) + "_prob";
        const float probability = std::clamp((50.f - e.position.y) / 15.f * 100.f, 0.f, 100.f);
        if (auto* p = processor.apvts.getParameter(key)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(probability));
            p->endChangeGesture();
        }
    } else setStep(idx, drawValue);
}
void GateGrid::mouseDown(const juce::MouseEvent& e) {
    if (e.position.y >= 52) return;
    edited = -1;
    probabilityMode = e.position.y > 39;
    const int steps = std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()), 1, 32);
    const int idx = std::clamp(static_cast<int>(e.position.x / (getWidth() / static_cast<float>(steps))), 0, steps - 1);
    const auto key = "step" + juce::String(idx + 1) + (probabilityMode ? "_prob" : "_state");
    drawValue = e.mods.isAltDown() ? 3 : (static_cast<int>(processor.apvts.getRawParameterValue("step" + juce::String(idx + 1) + "_state")->load()) + 1) % 4;
    edit(e);
}
void GateGrid::mouseDrag(const juce::MouseEvent& e) { edit(e); }
void GateGrid::mouseUp(const juce::MouseEvent&) {
    edited = -1;
}
void GateGrid::applyPattern(int op) {
    const int steps = std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()), 1, 32);
    std::array<int, 32> values{};
    for (int i = 0; i < steps; ++i) values[static_cast<size_t>(i)] = static_cast<int>(processor.apvts.getRawParameterValue("step" + juce::String(i + 1) + "_state")->load());
    if (op == 0) {
        const int seed = static_cast<int>(processor.apvts.getRawParameterValue("seed")->load());
        for (int i = 0; i < steps; ++i) values[static_cast<size_t>(i)] = static_cast<int>(skell::dsp::GlitchRandomizer::hash(static_cast<uint32_t>(seed + i)) % 4);
    } else if (op == 1 || op == 2) {
        const auto old = values;
        for (int i = 0; i < steps; ++i) values[static_cast<size_t>(i)] = old[static_cast<size_t>((i + (op == 1 ? 1 : steps - 1)) % steps)];
    } else if (op == 3) {
        for (int i = 0; i < steps; ++i) values[static_cast<size_t>(i)] = values[static_cast<size_t>(i)] == 0 ? 1 : 0;
    } else values.fill(0);
    for (int i = 0; i < steps; ++i) {
        const auto key = "step" + juce::String(i + 1) + "_state";
        if (auto* p = processor.apvts.getParameter(key)) {
            setStep(i, values[static_cast<size_t>(i)]);
        }
    }
    repaint();
}

void GateGrid::loadPattern(int index) {
    const int steps = std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()), 1, 32);
    const auto name = pattern.getItemText(index);
    processor.apvts.state.setProperty("patternName", name, nullptr);
    for (int i = 0; i < steps; ++i) {
        int value = 1;
        switch (index) {
        case 0: value = 1; break;
        case 1: value = i % 2 ? 1 : 0; break;
        case 2: value = i % 4 == 0 ? 2 : (i % 2 ? 0 : 1); break;
        case 3: value = i % 3 == 0 ? 3 : (i % 3 == 1 ? 1 : 0); break;
        case 4: value = i < steps / 2 ? 1 : 0; break;
        default: value = static_cast<int>(skell::dsp::GlitchRandomizer::hash(
            static_cast<uint32_t>((index + 1) * 719 + i * 1567)) % 4); break;
        }
        setStep(i, value);
    }
    repaint();
}
