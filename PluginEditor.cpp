#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <array>
#include <cmath>

namespace {
class Backplate final : public juce::Component {
public:
    Backplate() { setInterceptsMouseClicks(false, false); setBufferedToImage(true); }
    void paint(juce::Graphics& g) override {
        using namespace skell::theme;
        const auto ivory = skell::theme::ivory();
        const auto base = colour(background);
        const auto upper = ivory ? juce::Colour(0xffe9d8c1) : base.brighter(.12f);
        const auto lower = ivory ? juce::Colour(0xffc8b094) : base.darker(.12f);
        g.setGradientFill(juce::ColourGradient(upper,100.f,55.f,lower,870.f,720.f,false));
        g.fillAll();
        // Side and lower vignette reproduce the reference's patinated champagne
        // metal without a bitmap overlay that would hide interactive controls.
        if (ivory) {
            g.setGradientFill(juce::ColourGradient(
                juce::Colours::black.withAlpha(.075f),0.f,320.f,
                juce::Colours::black.withAlpha(0.f),480.f,320.f,false));
            g.fillAll();
            g.setGradientFill(juce::ColourGradient(
                juce::Colours::black.withAlpha(0.f),0.f,310.f,
                juce::Colours::black.withAlpha(.048f),0.f,708.f,false));
            g.fillAll();
        }
        // Reproducible horizontal brush strokes: cached offscreen by JUCE,
        // not an expensive texture allocation in the audio process.
        if (ivory) {
            juce::uint32 seed=0x1af0405u;
            for(int i=0;i<2200;++i) {
                seed ^= seed<<13; seed ^= seed>>17; seed ^= seed<<5;
                const float x=static_cast<float>(seed%960);
                seed ^= seed<<13; seed ^= seed>>17; seed ^= seed<<5;
                const float y=static_cast<float>(seed%720);
                seed ^= seed<<13; seed ^= seed>>17; seed ^= seed<<5;
                const float len=5.f+static_cast<float>(seed%78);
                g.setColour(((seed&1u)?juce::Colours::white:juce::Colours::black)
                             .withAlpha(.018f+static_cast<float>(seed%5u)*.005f));
                g.drawLine(x,y,std::min(955.f,x+len),y,.43f);
            }
        }
        auto frame=juce::Rectangle<float>(2,2,956,716);
        g.setColour(juce::Colours::black.withAlpha(.62f));
        g.drawRoundedRectangle(frame,23.f,3.f);
        g.setColour(colour(border).brighter(.22f));
        g.drawRoundedRectangle(frame.reduced(2.f),21.f,2.f);
        g.setColour(juce::Colours::white.withAlpha(.62f));
        g.drawRoundedRectangle(frame.reduced(4.f),19.f,1.f);
        g.setColour(juce::Colours::black.withAlpha(.18f));
        g.drawRoundedRectangle(frame.reduced(5.5f),18.f,1.f);
        // Continuous thin engraved section rules.
        g.setColour(colour(border).withAlpha(.47f));
        g.drawLine(168,139,431,139,.85f);
        g.drawLine(535,139,792,139,.85f);
        g.drawLine(196,583,759,583,1.1f);
        g.setColour(juce::Colours::white.withAlpha(.35f));
        g.drawLine(196,584,759,584,.75f);
        g.setFont(regular(14));
        g.setColour(colour(gray));
        g.drawText("LOOP LENGTH",426,120,110,24,juce::Justification::centred);
        g.drawText("FADE IN",45,340,120,26,juce::Justification::centred);
        g.drawText("FADE OUT",794,340,120,26,juce::Justification::centred);
        g.drawText("MODE",141,494,70,25,juce::Justification::centred);
        g.drawText("SMOOTH",43,676,120,24,juce::Justification::centred);
        g.drawText("MIX",793,676,120,24,juce::Justification::centred);
        g.setFont(mono(11.5f));
        g.drawText("v1.6.2",744,681,74,16,juce::Justification::centredRight);
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
    background->setBounds(0, 0, skell::theme::width, skell::theme::height);
    for (juce::Component* c : std::array<juce::Component*, 11>{&top, &loops, &waveform, &gate, &modes, &presets,
                                                         &band, &fadeIn, &fadeOut, &smooth, &mix}) canvas.addAndMakeVisible(*c);
    top.setBounds(skell::theme::top);
    loops.setBounds(skell::theme::loops);
    waveform.setBounds(skell::theme::wave);
    gate.setBounds(skell::theme::gate);
    modes.setBounds(skell::theme::modes);
    presets.setBounds(skell::theme::presets);
    band.setBounds(skell::theme::band);
    fadeIn.setBounds(34, 190, 141, 154);
    fadeOut.setBounds(783, 190, 141, 154);
    smooth.setBounds(39, 541, 117, 126);
    mix.setBounds(793, 541, 117, 126);
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
    // Same skull-click appearance workflow as SkellKey and the SkellCore quickstart.
    top.skull.onClick = [this] {
        const auto next = skellcore::ui::studioSkinFromIndex((static_cast<int>(look.skin()) + 1) % 4);
        setLookAndFeel(nullptr);
        look.setSkin(next);
        setLookAndFeel(&look);
        (void)skellcore::ui::StudioSkinStore::save(next);
        background->repaint();
        top.repaint(); loops.repaint(); waveform.repaint(); gate.repaint();
        modes.repaint(); presets.repaint(); band.repaint();
        for (auto* dial : {&fadeIn, &fadeOut, &smooth, &mix}) dial->repaint();
        settings->repaint();
    };
    tooltips = std::make_unique<juce::TooltipWindow>(this, 700);
    setResizable(true, true);
    setResizeLimits(480, 360, 1200, 900);
    getConstrainer()->setFixedAspectRatio(4.0 / 3.0);
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
    setSize(juce::roundToInt(skell::theme::width * uiScale),
            juce::roundToInt(skell::theme::height * uiScale));
}
void SkellGlitchAudioProcessorEditor::resized() {
    uiScale = getWidth() / static_cast<float>(skell::theme::width);
    canvas.setBounds(0, 0, skell::theme::width, skell::theme::height);
    canvas.setTransform(juce::AffineTransform::scale(uiScale));
}
void SkellGlitchAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(skell::theme::colour(skell::theme::background));
}
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
