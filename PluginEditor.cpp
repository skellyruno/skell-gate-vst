#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <array>
#include <cmath>

namespace {
class Backplate final : public juce::Component {
public:
    Backplate() { setInterceptsMouseClicks(false, false); setBufferedToImage(true); }
    void paint(juce::Graphics& g) override {
        g.fillAll(skell::theme::colour(skell::theme::background));
        juce::ColourGradient vignette(skell::theme::colour(0xff132319), 479, 320,
                                      skell::theme::colour(skell::theme::background), 880, 650, true);
        g.setGradientFill(vignette); g.fillAll();
        g.setColour(skell::theme::colour(skell::theme::border));
        g.drawLine(0.f, 55.f, 958.f, 55.f, 1.f);
        g.drawLine(0.f, 672.f, 958.f, 672.f, 1.f);
        // Quiet section dividers preserve visual hierarchy without noisy glows.
        g.setColour(skell::theme::colour(skell::theme::border).withAlpha(.65f));
        g.drawLine(186.f, 143.f, 772.f, 143.f, 1.f);
        g.drawLine(215.f, 583.f, 744.f, 583.f, 1.f);
        g.setColour(skell::theme::colour(skell::theme::gray));
        g.setFont(skell::theme::regular(12));
        g.drawText("FADE IN", 50, 346, 120, 24, juce::Justification::centred);
        g.drawText("FADE OUT", 786, 346, 120, 24, juce::Justification::centred);
        g.drawText("MODE", 151, 505, 53, 19, juce::Justification::centredRight);
        g.drawText("SMOOTH", 50, 648, 120, 18, juce::Justification::centred);
        g.drawText("MIX", 783, 648, 120, 18, juce::Justification::centred);
        g.setFont(skell::theme::mono(11));
        g.drawText("v1.5.4", 700, 652, 70, 17, juce::Justification::centredRight);
    }
};

class MomentaryButton final : public juce::TextButton {
public:
    MomentaryButton(juce::AudioProcessorValueTreeState& state, juce::String id, juce::String label)
       : juce::TextButton(label), apvts(state), name(std::move(id)) {}
    void mouseDown(const juce::MouseEvent& e) override { set(1.f, true); juce::TextButton::mouseDown(e); }
    void mouseUp(const juce::MouseEvent& e) override { set(0.f, false); juce::TextButton::mouseUp(e); }
private:
    void set(float v, bool begin) {
        if (auto* p = apvts.getParameter(name)) {
            if (begin) p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(v));
            if (!begin) p->endChangeGesture();
        }
    }
    juce::AudioProcessorValueTreeState& apvts;
    juce::String name;
};
}

class SettingsPanel final : public juce::Component {
public:
    SettingsPanel(SkellGlitchAudioProcessor& p, std::function<void(int)> sizeCallback) : processor(p), resizeEditor(std::move(sizeCallback)),
        trigger(p.apvts, "trigger", "TRIGGER"), freeze(p.apvts, "freeze", "HOLD FREEZE") {
        setAlwaysOnTop(true);
        setVisible(false);
        addAndMakeVisible(close);
        close.setButtonText("X"); close.onClick = [this] { setVisible(false); };
        for (auto param : {"chaos", "reversePitch", "gateAttack", "gateRelease", "gateDepth", "gateSwing", "loopMs", "gateSteps", "seed"}) {
            auto slider = std::make_unique<juce::Slider>();
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 19);
            slider->setDoubleClickReturnValue(true, p.apvts.getParameter(param)->convertFrom0to1(p.apvts.getParameter(param)->getDefaultValue()));
            labels.add(juce::String(param));
            addAndMakeVisible(*slider);
            sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, param, *slider));
            sliders.push_back(std::move(slider));
        }
        addCombo("triggerSource", {"Auto", "MIDI", "Sidechain", "Manual"});
        addCombo("latencyMode", {"Zero (previous slice)", "Aligned (delay dry)"});
        addCombo("gateCurve", {"Linear", "Exponential", "S-Curve"});
        for (const auto& param : {"bandInvert", "seedLock", "deltaListen"}) {
            auto t = std::make_unique<juce::TextButton>(param);
            t->setClickingTogglesState(true);
            addAndMakeVisible(*t);
            toggleAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, param, *t));
            toggles.push_back(std::move(t));
        }
        addAndMakeVisible(trigger); addAndMakeVisible(freeze);
        manualCopy.setButtonText("GATE > REVERSE");
        manualCopy.setClickingTogglesState(true);
        manualCopy.setToggleState(p.gateFirst.load(), juce::dontSendNotification);
        manualCopy.onClick = [this] { processor.gateFirst.store(manualCopy.getToggleState()); };
        addAndMakeVisible(manualCopy);
        jitter.setButtonText("TITLE GLITCH");
        jitter.setClickingTogglesState(true);
        jitter.setToggleState(p.titleJitter.load(), juce::dontSendNotification);
        jitter.onClick = [this] { processor.titleJitter.store(jitter.getToggleState()); };
        addAndMakeVisible(jitter);
        size.addItemList({"XS - 50%", "S - 75%", "M - 100%", "L - 125%"}, 1);
        size.setSelectedId(p.getSavedSize() + 1, juce::dontSendNotification);
        size.onChange = [this] { if (size.getSelectedId() > 0) resizeEditor(size.getSelectedId() - 1); };
        addAndMakeVisible(size);
        randomSeed.setButtonText("RE-SEED");
        randomSeed.onClick = [this] {
            if (auto* p = processor.apvts.getParameter("seed")) {
                const int old = static_cast<int>(processor.apvts.getRawParameterValue("seed")->load());
                const int next = (old * 1664525LL + 1013904223LL) % 1000001;
                p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(next))); p->endChangeGesture();
            }
        };
        addAndMakeVisible(randomSeed);
    }
    void paint(juce::Graphics& g) override {
        skell::theme::roundTile(g, getLocalBounds().toFloat().reduced(1.f), 13);
        g.setColour(skell::theme::colour(skell::theme::neon));
        g.setFont(skell::theme::display(19));
        g.drawText("ENGINE SETTINGS", 18, 12, 254, 25, juce::Justification::centredLeft);
        g.setColour(skell::theme::colour(skell::theme::gray));
        g.setFont(skell::theme::regular(12));
        for (int i = 0; i < labels.size(); ++i)
            g.drawText(labels[i].toUpperCase(), 18, 51 + i * 29, 130, 22, juce::Justification::centredLeft);
        g.drawText("TRIGGER SOURCE", 18, 323, 126, 18, juce::Justification::centredLeft);
        g.drawText("LATENCY", 18, 354, 126, 18, juce::Justification::centredLeft);
        g.drawText("GATE CURVE", 18, 385, 126, 18, juce::Justification::centredLeft);
        g.drawText("WINDOW SIZE", 18, 512, 130, 18, juce::Justification::centredLeft);
    }
    void resized() override {
        close.setBounds(getWidth() - 44, 10, 28, 28);
        for (size_t i = 0; i < sliders.size(); ++i)
            sliders[i]->setBounds(140, 51 + static_cast<int>(i) * 29, getWidth() - 154, 22);
        for (size_t i = 0; i < combos.size(); ++i)
            combos[i]->setBounds(145, 317 + static_cast<int>(i) * 31, getWidth() - 161, 25);
        for (size_t i = 0; i < toggles.size(); ++i)
            toggles[i]->setBounds(15 + static_cast<int>(i) * 124, 413, 120, 29);
        manualCopy.setBounds(15, 446, 173, 29);
        jitter.setBounds(194, 446, 175, 29);
        trigger.setBounds(15, 479, 173, 29);
        freeze.setBounds(194, 479, 175, 29);
        size.setBounds(145, 510, 135, 26);
        randomSeed.setBounds(286, 510, 94, 26);
    }
private:
    void addCombo(const char* name, juce::StringArray choices) {
        auto c = std::make_unique<juce::ComboBox>();
        c->addItemList(choices, 1);
        addAndMakeVisible(*c);
        comboAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, name, *c));
        combos.push_back(std::move(c));
    }
    SkellGlitchAudioProcessor& processor;
    std::function<void(int)> resizeEditor;
    juce::TextButton close{"✕"}, manualCopy, jitter, randomSeed;
    MomentaryButton trigger, freeze;
    juce::ComboBox size;
    juce::StringArray labels;
    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<juce::TextButton>> toggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> toggleAttachments;
};

SkellGlitchAudioProcessorEditor::SkellGlitchAudioProcessorEditor(SkellGlitchAudioProcessor& p)
    : juce::AudioProcessorEditor(&p), processor(p), top(p), loops(p), waveform(p), gate(p), modes(p), presets(p), band(p) {
    setLookAndFeel(&look);
    canvas.setInterceptsMouseClicks(false, true);
    addAndMakeVisible(canvas);
    background = std::make_unique<Backplate>();
    canvas.addAndMakeVisible(*background);
    background->setBounds(0, 0, 958, 676);
    for (juce::Component* c : std::array<juce::Component*, 11>{&top, &loops, &waveform, &gate, &modes, &presets,
                                                         &band, &fadeIn, &fadeOut, &smooth, &mix}) canvas.addAndMakeVisible(*c);
    top.setBounds(skell::theme::top);
    loops.setBounds(skell::theme::loops);
    waveform.setBounds(skell::theme::wave);
    gate.setBounds(skell::theme::gate);
    modes.setBounds(skell::theme::modes);
    presets.setBounds(skell::theme::presets);
    band.setBounds(skell::theme::band);
    fadeIn.setBounds(43, 191, 130, 150);
    fadeOut.setBounds(783, 191, 130, 150);
    smooth.setBounds(50, 555, 119, 97);
    mix.setBounds(786, 555, 119, 97);
    for (auto* slider : {&fadeIn, &fadeOut, &smooth, &mix}) slider->setDoubleClickReturnValue(true, slider == &smooth ? 30. : slider == &mix ? 100. : 0.);
    fadeIn.textFromValueFunction = [](double value) { return value < .5 ? "Inst" : juce::String(value, 0) + " ms"; };
    fadeOut.textFromValueFunction = fadeIn.textFromValueFunction;
    smooth.textFromValueFunction = [](double value) { return juce::String(value, 0) + "%"; };
    mix.textFromValueFunction = smooth.textFromValueFunction;
    fadeInAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "fadeIn", fadeIn);
    fadeOutAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "fadeOut", fadeOut);
    smoothAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "smooth", smooth);
    mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "mix", mix);
    top.sync.setClickingTogglesState(true); top.power.setClickingTogglesState(true);
    syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "sync", top.sync);
    powerAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "power", top.power);
    settings = std::make_unique<SettingsPanel>(p, [this](int size) { applyWindowSize(size); });
    canvas.addAndMakeVisible(*settings);
    settings->setBounds(530, 102, 402, 548);
    settings->setVisible(false);
    top.settings.onClick = [this] { settings->setVisible(!settings->isVisible()); settings->toFront(false); };
    tooltips = std::make_unique<juce::TooltipWindow>(this, 700);
    setResizable(true, true);
    setResizeLimits(479, 338, 1198, 845);
    getConstrainer()->setFixedAspectRatio(958.0 / 676.0);
    applyWindowSize(p.getSavedSize());
    startTimerHz(30);
}
SkellGlitchAudioProcessorEditor::~SkellGlitchAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
    // JUCE removes children automatically; this instance owns the widgets and settings.
}
void SkellGlitchAudioProcessorEditor::applyWindowSize(int index) {
    // 75% is precisely half the size of the old 150% screenshot.
    // The 50% option is available for very compact DAW layouts.
    constexpr float scales[] = {.5f, .75f, 1.f, 1.25f};
    uiScale = scales[static_cast<size_t>(std::clamp(index, 0, 3))];
    processor.setSavedSize(index);
    setSize(juce::roundToInt(958 * uiScale), juce::roundToInt(676 * uiScale));
}
void SkellGlitchAudioProcessorEditor::resized() {
    uiScale = getWidth() / 958.f;
    canvas.setBounds(0, 0, 958, 676);
    canvas.setTransform(juce::AffineTransform::scale(uiScale));
}
void SkellGlitchAudioProcessorEditor::paint(juce::Graphics& g) { g.fillAll(skell::theme::colour(skell::theme::background)); }
void SkellGlitchAudioProcessorEditor::timerCallback() {
    const bool on = processor.apvts.getRawParameterValue("sync")->load() >= .5f;
    const auto label = on ? juce::String("SYNC ON") : juce::String("SYNC OFF");
    if (top.sync.getButtonText() != label) top.sync.setButtonText(label);
    const auto latency = processor.displayLatency.load(std::memory_order_relaxed);
    const auto sampleRate = processor.getSampleRate();
    if (lastHeaderLatency != latency || lastHeaderSampleRate != sampleRate) {
        lastHeaderLatency = latency;
        lastHeaderSampleRate = sampleRate;
        top.repaint();
    }
    const auto current = processor.flipCounter.load(std::memory_order_relaxed);
    if (current != lastFlip) {
        lastFlip = current;
        top.setJitterOffset(processor.titleJitter.load() ? (current & 1U ? 2 : -2) : 0);
    } else top.setJitterOffset(0);
}

juce::AudioProcessorEditor* SkellGlitchAudioProcessor::createEditor() {
    return new SkellGlitchAudioProcessorEditor(*this);
}
