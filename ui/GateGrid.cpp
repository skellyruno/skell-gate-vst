#include "GateGrid.h"
#include "../PluginProcessor.h"
#include "../dsp/GlitchRandomizer.h"
#include <cmath>
GateGrid::GateGrid(SkellGlitchAudioProcessor& p) : processor(p),
    commands{juce::TextButton("RANDOM"), juce::TextButton("SHIFT <"),
             juce::TextButton("SHIFT >"), juce::TextButton("INVERT"), juce::TextButton("CLEAR")} {
    for (int i = 0; i < 5; ++i) {
        addAndMakeVisible(commands[static_cast<size_t>(i)]);
        commands[static_cast<size_t>(i)].onClick = [this, i] { applyPattern(i); };
    }
    pattern.addItemList({"Straight", "Offbeat", "Trap Hats", "Triplet Stutter", "Half-Time",
        "Glitch 1", "Glitch 2", "Glitch 3", "Glitch 4", "Glitch 5", "Glitch 6", "Glitch 7", "Glitch 8"}, 1);
    pattern.setTextWhenNothingSelected("PATTERNS");
    pattern.onChange = [this] { if (pattern.getSelectedId() > 0) loadPattern(pattern.getSelectedId() - 1); };
    addAndMakeVisible(pattern);
    setTooltip("Click to cycle steps. Drag to paint. Drag probability strip or hold Shift to edit chance.");
    for (int i = 0; i < 32; ++i) {
        const auto name = "step" + juce::String(i + 1);
        stateValues[static_cast<size_t>(i)] = processor.apvts.getRawParameterValue(name + "_state");
        probabilityValues[static_cast<size_t>(i)] = processor.apvts.getRawParameterValue(name + "_prob");
    }
    lastStates.fill(-1.f);
    lastProbabilities.fill(-1.f);
    startTimerHz(30);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}
void GateGrid::timerCallback() {
    if (!isShowing()) return;
    const int steps = std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()), 1, 32);
    const int active = processor.displayStep.load(std::memory_order_relaxed);
    bool changed = steps != lastStepCount || active != lastPaintedStep;
    lastStepCount = steps;
    lastPaintedStep = active;
    for (int i = 0; i < steps; ++i) {
        const auto idx = static_cast<size_t>(i);
        const float state = stateValues[idx]->load(std::memory_order_relaxed);
        const float chance = probabilityValues[idx]->load(std::memory_order_relaxed);
        if (state != lastStates[idx] || chance != lastProbabilities[idx]) changed = true;
        lastStates[idx] = state;
        lastProbabilities[idx] = chance;
    }
    if (changed) repaint();
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
        const auto idx = static_cast<size_t>(i);
        const int state = static_cast<int>(stateValues[idx]->load(std::memory_order_relaxed));
        const float probability = probabilityValues[idx]->load(std::memory_order_relaxed);
        const float x = i * cell + 2.f;
        const float w = cell - 4.f;
        const auto fill = state == 3 ? skell::theme::magenta :
                         state == 2 ? skell::theme::pale :
                         state == 1 ? skell::theme::neon : skell::theme::tile;
        g.setColour(skell::theme::colour(fill).withAlpha(state == 0 ? .9f : .77f));
        g.fillRoundedRectangle(x, state == 2 ? 0.f : 7.f, w, state == 2 ? 39.f : 32.f, 4.f);
        if (i == active || i == hoveredStep) {
            g.setColour(skell::theme::colour(i == active ? skell::theme::bone : skell::theme::cyan));
            g.drawRoundedRectangle(x, 0.f, w, 41.f, 4.f, i == active ? 1.3f : 1.f);
        }
        if ((i % 4 == 0 || i == hoveredStep) && w > 22.f) {
            g.setColour(skell::theme::colour(state == 0 ? skell::theme::gray : skell::theme::deep).withAlpha(.8f));
            g.setFont(skell::theme::mono(9.f));
            g.drawText(juce::String(i + 1), static_cast<int>(x), 29,
                       static_cast<int>(w), 10, juce::Justification::centred);
        }
        g.setColour(skell::theme::colour(skell::theme::border));
        g.fillRoundedRectangle(x, 44.f, w, 3.f, 1.5f);
        if (probability > 0.f) {
            g.setColour(skell::theme::colour(skell::theme::cyan).withAlpha(.35f + probability * .004f));
            g.fillRoundedRectangle(x, 44.f, w * probability * .01f, 3.f, 1.5f);
        }
    }
}
void GateGrid::setStep(int index, int value) {
    const auto key = "step" + juce::String(index + 1) + "_state";
    if (auto* p = processor.apvts.getParameter(key)) {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(value)));
        p->endChangeGesture();
        repaint();
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
        // Holding Shift over the full-height step edits 0-100% instead of
        // instantly pegging at 100%; the small strip keeps its own fine zone.
        const float probability = std::clamp(
            (probabilityOnBody ? (39.f - e.position.y) / 39.f
                               : (52.f - e.position.y) / 13.f) * 100.f, 0.f, 100.f);
        if (idx != activeProbabilityStep) {
            finishProbabilityGesture();
            if (auto* p = processor.apvts.getParameter(key)) {
                p->beginChangeGesture();
                activeProbabilityStep = idx;
            }
        }
        if (auto* p = processor.apvts.getParameter(key)) {
            const float old = processor.apvts.getRawParameterValue(key)->load();
            if (std::abs(old - probability) >= .5f) {
                p->setValueNotifyingHost(p->convertTo0to1(probability));
                repaint();
            }
        }
    } else setStep(idx, drawValue);
}
void GateGrid::mouseDown(const juce::MouseEvent& e) {
    if (e.position.y >= 52) return;
    edited = -1;
    finishProbabilityGesture();
    probabilityMode = e.position.y > 39 || e.mods.isShiftDown();
    probabilityOnBody = e.mods.isShiftDown() && e.position.y <= 39;
    const int steps = std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()), 1, 32);
    const int idx = std::clamp(static_cast<int>(e.position.x / (getWidth() / static_cast<float>(steps))), 0, steps - 1);
    const auto key = "step" + juce::String(idx + 1) + (probabilityMode ? "_prob" : "_state");
    drawValue = e.mods.isAltDown() ? 3 : (static_cast<int>(processor.apvts.getRawParameterValue("step" + juce::String(idx + 1) + "_state")->load()) + 1) % 4;
    edit(e);
}
void GateGrid::mouseDrag(const juce::MouseEvent& e) { edit(e); }
void GateGrid::finishProbabilityGesture() {
    if (activeProbabilityStep < 0) return;
    const auto key = "step" + juce::String(activeProbabilityStep + 1) + "_prob";
    if (auto* p = processor.apvts.getParameter(key)) p->endChangeGesture();
    activeProbabilityStep = -1;
}
void GateGrid::mouseUp(const juce::MouseEvent&) {
    finishProbabilityGesture();
    edited = -1;
    probabilityMode = false;
}
void GateGrid::mouseMove(const juce::MouseEvent& e) {
    const int steps = std::clamp(static_cast<int>(processor.apvts.getRawParameterValue("gateSteps")->load()), 1, 32);
    const int next = e.position.y < 52.f
        ? std::clamp(static_cast<int>(e.position.x * steps / getWidth()), 0, steps - 1) : -1;
    if (next != hoveredStep) { hoveredStep = next; repaint(); }
}
void GateGrid::mouseExit(const juce::MouseEvent&) {
    if (hoveredStep != -1) { hoveredStep = -1; repaint(); }
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
