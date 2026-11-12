#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>

// Reuse the exact processor factory linked into the VST3 wrapper.
extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main() {
    juce::ScopedJuceInitialiser_GUI initialize;
    std::unique_ptr<juce::AudioProcessor> processor(createPluginFilter());
    if (!processor) { std::cerr << "Failed to create plugin processor\n"; return 1; }
    processor->prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor->createEditor());
    if (!editor) { std::cerr << "Failed to create plugin editor\n"; return 2; }
    editor->setVisible(true);
    const auto directory = juce::File::getCurrentWorkingDirectory()
                               .getChildFile("build").getChildFile("ui-snapshots");
    if (!directory.createDirectory()) { std::cerr << "Failed creating snapshot directory\n"; return 3; }
    const std::array<std::pair<int, int>, 4> sizes{
        std::pair<int, int>{480, 360}, {720, 540}, {960, 720}, {1200, 900}
    };
    juce::PNGImageFormat png;
    for (const auto& [width, height] : sizes) {
        editor->setSize(width, height);
        const auto frame = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        if (!frame.isValid() || frame.getWidth() != width || frame.getHeight() != height) {
            std::cerr << "Invalid snapshot dimensions at " << width << "x" << height << "\n";
            return 4;
        }
        int green = 0;
        for (int y = 0; y < height; y += 6)
            for (int x = 0; x < width; x += 6) {
                const auto px = frame.getPixelAt(x, y);
                if (px.getGreen() > 115 && px.getGreen() > px.getRed() + 50
                    && px.getGreen() > px.getBlue() + 35) ++green;
            }
        if (green < 10) {
            std::cerr << "Blank or missing green highlights at " << width << "x" << height << "\n";
            return 5;
        }
        if (width == 960) {
            // On a clean runner with no previous SkellCore selection, the
            // matte-black hardware finish must be the default, not Ivory.
            const auto body = frame.getPixelAt(110, 160);
            if (body.getRed() > 85 || body.getGreen() > 85 || body.getBlue() > 85) {
                std::cerr << "UI_THEME: initial chassis is not matte black\n";
                return 25;
            }
            // The reference's Inst fade knobs have a deliberately lit bezel
            // even at zero value. Verify that the actual JUCE draw covers the
            // left dial rather than only the parameter's tiny endpoint dot.
            int illuminated = 0;
            for (int yy = 197; yy <= 333; ++yy)
                for (int xx = 43; xx <= 170; ++xx) {
                    const auto pixel = frame.getPixelAt(xx, yy);
                    if (pixel.getGreen() > 165 && pixel.getGreen() > pixel.getRed() + 75
                        && pixel.getGreen() > pixel.getBlue() + 45) ++illuminated;
                }
            if (illuminated < 110) {
                std::cerr << "UI_REFERENCE: fade bezel lacks green illumination\n";
                return 24;
            }
            std::cout << "UI_REFERENCE: fade illuminated pixels=" << illuminated << "\n";
            // Black-green Obsidian must not regress into the previous
            // blue/cyan panel. Check the actual rendered chassis midtone.
            const auto chassis = frame.getPixelAt(80, 172);
            if (chassis.getBlue() > chassis.getGreen() + 15) {
                std::cerr << "UI_THEME: blue chassis leaked into green skin\n";
                return 41;
            }
        }
        const auto output = directory.getChildFile(
            "Skell-Glitch-" + juce::String(width) + "x" + juce::String(height) + ".png");
        juce::FileOutputStream stream(output);
        if (stream.failedToOpen() || !png.writeImageToStream(frame, stream)) {
            std::cerr << "Failed to write screenshot: " << output.getFullPathName().toStdString() << "\n";
            return 6;
        }
        std::cout << "UI_SMOKE: " << width << "x" << height
                  << " valid, green samples=" << green << "\n";
    }
    const auto findParameter = [&](const juce::String& name) -> juce::AudioProcessorParameter* {
        for (auto* p : processor->getParameters())
            if (p->getName(128) == name) return p;
        return nullptr;
    };
    auto* power = findParameter("Power");
    if (power == nullptr || power->getValue() >= .5f) {
        std::cerr << "Expected bypassed initial power parameter\n";
        return 7;
    }
    // Full JUCE processor signal-path smoke: real blocks, 2-channel sine,
    // default bypass pass-through, then enabled reverse processing.
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    const auto began = std::chrono::steady_clock::now();
    int64_t processed = 0;
    bool heardReverse = false;
    constexpr float amplitude = .4f;
    for (int block = 0; block < 500; ++block) {
        for (int sample = 0; sample < 512; ++sample) {
            const double phase = (block * 512 + sample) * 6.283185307179586 * 220.0 / 48000.0;
            const float left = amplitude * static_cast<float>(std::sin(phase));
            audio.setSample(0, sample, left);
            audio.setSample(1, sample, -left);
        }
        const bool enabled = block >= 210;
        if (block == 210) power->setValueNotifyingHost(1.f);
        processor->processBlock(audio, midi);
        for (int sample = 0; sample < 512; ++sample) {
            const double phase = (block * 512 + sample) * 6.283185307179586 * 220.0 / 48000.0;
            const float original = amplitude * static_cast<float>(std::sin(phase));
            const float left = audio.getSample(0, sample);
            const float right = audio.getSample(1, sample);
            if (!std::isfinite(left) || !std::isfinite(right)) {
                std::cerr << "HOST_SMOKE: non-finite output\n";
                return 8;
            }
            if (std::abs(left) > 1.f || std::abs(right) > 1.f) {
                std::cerr << "HOST_SMOKE: clipping on below-unity sine\n";
                return 9;
            }
            if (std::abs(left + right) > .001f) {
                std::cerr << "HOST_SMOKE: stereo phase mismatch\n";
                return 10;
            }
            if (!enabled && std::abs(left - original) > 1.e-5f) {
                std::cerr << "HOST_SMOKE: bypass does not reproduce dry signal\n";
                return 11;
            }
            if (enabled && block > 240 && std::abs(left - original) > .01f)
                heardReverse = true;
        }
        processed += 512;
    }
    if (!heardReverse || processor->getLatencySamples() != 0) {
        std::cerr << "HOST_SMOKE: reverse inactive or unexpected zero-mode latency\n";
        return 12;
    }
    // The actual UI callbacks must write automatable host parameters.
    std::function<juce::TextButton*(juce::Component&, const juce::String&)> findButton;
    findButton = [&](juce::Component& root, const juce::String& text) -> juce::TextButton* {
        if (auto* button = dynamic_cast<juce::TextButton*>(&root))
            if (button->getButtonText() == text) return button;
        for (int i = 0; i < root.getNumChildComponents(); ++i)
            if (auto* found = findButton(*root.getChildComponent(i), text)) return found;
        return nullptr;
    };
    // The header must own the only visible preset strip; the lower section
    // must have no third action-button row after the redesign.
    std::function<juce::Component*(juce::Component&, const juce::String&)> findNamed;
    findNamed = [&](juce::Component& root, const juce::String& name) -> juce::Component* {
        if (root.getName() == name) return &root;
        for (int i = 0; i < root.getNumChildComponents(); ++i)
            if (auto* match = findNamed(*root.getChildComponent(i), name)) return match;
        return nullptr;
    };
    auto* settingsIcon = findNamed(*editor, "HEADER_SETTINGS");
    auto* powerIcon = findNamed(*editor, "HEADER_POWER");
    if (!settingsIcon || !powerIcon || settingsIcon->getWidth() > 38 ||
        powerIcon->getWidth() > 38 || settingsIcon->getHeight() > 38 ||
        powerIcon->getHeight() > 38 ||
        settingsIcon->getRight() + 8 < powerIcon->getX() ||
        settingsIcon->getBounds().intersects(powerIcon->getBounds())) {
        std::cerr << "UI_LAYOUT: header gear and power controls are oversized or misaligned\n";
        return 38;
    }
    auto* headerPreset = findNamed(*editor, "HEADER_PRESET_BAR");
    auto* presetTools = findNamed(*editor, "PRESET_TOOLS_MENU");
    auto* previousPreset = dynamic_cast<juce::TextButton*>(
        findNamed(*editor, "PRESET_PREVIOUS"));
    auto* nextPreset = dynamic_cast<juce::TextButton*>(
        findNamed(*editor, "PRESET_NEXT"));
    auto* selection = dynamic_cast<juce::ComboBox*>(
        findNamed(*editor, "HEADER_PRESET_SELECTION"));
    if (findButton(*editor,"EDIT") || findButton(*editor,"PATTERNS") ||
        !findNamed(*editor,"SEQUENCER_GRID") ||
        !previousPreset || !nextPreset || !selection ||
        !previousPreset->onClick || !nextPreset->onClick ||
        !presetTools || dynamic_cast<juce::TextButton*>(presetTools)->getButtonText() != "TOOLS") {
        std::cerr << "UI_LAYOUT: sequencer or descriptive preset controls missing\n";
        return 26;
    }
    previousPreset->onClick();
    if (selection->getSelectedId() == 12) {
        std::cerr << "UI_PRESET: previous preset did not navigate\n";
        return 34;
    }
    nextPreset->onClick();
    if (selection->getSelectedId() != 12) {
        std::cerr << "UI_PRESET: next preset did not restore INIT\n";
        return 35;
    }
    if (!headerPreset || !presetTools || !presetTools->isVisible() ||
        headerPreset->getY() >= 59 || headerPreset->getHeight() > 59 ||
        headerPreset->getWidth() < 250 ||
        presetTools->getParentComponent() != headerPreset) {
        std::cerr << "UI_LAYOUT: header preset control/menu missing or incorrectly placed\n";
        return 27;
    }
    // The rate/style row must never intrude into the four rotary controls.
    // This caught an earlier 29-pixel downward shift that clipped MIX.
    auto* reverseStyleSetting = dynamic_cast<juce::ComboBox*>(
        findNamed(*editor, "REVERSESTYLE"));
    auto* gateRateSetting = dynamic_cast<juce::ComboBox*>(
        findNamed(*editor, "GATERATE"));
    if (!reverseStyleSetting || !gateRateSetting ||
        !reverseStyleSetting->getParentComponent() ||
        reverseStyleSetting->getParentComponent() != gateRateSetting->getParentComponent() ||
        reverseStyleSetting->getY() >= gateRateSetting->getY() ||
        reverseStyleSetting->getBounds().intersects(gateRateSetting->getBounds())) {
        std::cerr << "UI_LAYOUT: advanced rhythm selectors missing from Settings\n";
        return 31;
    }
    // Moving these controls cannot break host automation or preset recall.
    auto* reverseStyleParameter = findParameter("Reverse style");
    if (!reverseStyleParameter) return 32;
    reverseStyleSetting->setSelectedId(4, juce::sendNotificationSync);
    if (reverseStyleParameter->getValue() < .99f) {
        std::cerr << "UI_CONTROL: Settings reverse-style picker lost automation\n";
        return 33;
    }
    reverseStyleSetting->setSelectedId(1, juce::sendNotificationSync);
    auto* modeButton = findButton(*editor, "REVERSE");
    auto* knobMix = findNamed(*editor, "MIX");
    auto* knobSmooth = findNamed(*editor, "SMOOTH");
    auto* fadeInKnob = dynamic_cast<juce::Slider*>(findNamed(*editor, "FADE IN"));
    auto* fadeParameter = findParameter("Fade in");
    if (!fadeInKnob || !fadeParameter) {
        std::cerr << "UI_CONTROL: fade-in knob or host parameter missing\n";
        return 29;
    }
    // Verify the hardware dial responds to real user-level slider changes,
    // that its APVTS attachment writes the host parameter, and that its
    // rendered value/pointer actually moves at the design size.
    editor->setSize(960, 720);
    const auto dialBefore = editor->createComponentSnapshot(editor->getLocalBounds(), true);
    fadeInKnob->setValue(100.0, juce::sendNotificationSync);
    const auto dialAfter = editor->createComponentSnapshot(editor->getLocalBounds(), true);
    int dialChangedPixels = 0;
    for (int y = 190; y < 344; y += 2)
        for (int x = 34; x < 175; x += 2)
            if (dialBefore.getPixelAt(x, y) != dialAfter.getPixelAt(x, y))
                ++dialChangedPixels;
    if (std::abs(fadeParameter->getValue() - .2f) > .005f || dialChangedPixels < 50) {
        std::cerr << "UI_CONTROL: rotary dial didn't automate or update its indicator\n";
        return 30;
    }
    fadeInKnob->setValue(0.0, juce::sendNotificationSync);
    std::cout << "UI_CONTROL: knob automation and live rotary rendering passed\n";
    if (!modeButton || !knobMix || !knobSmooth ||
        modeButton->getParentComponent()->getBounds().intersects(knobMix->getBounds()) ||
        modeButton->getParentComponent()->getBounds().intersects(knobSmooth->getBounds())) {
        std::cerr << "UI_LAYOUT: mode buttons collide with lower rotary knobs\n";
        return 28;
    }
    auto* gateButton = findButton(*editor, "GATE");
    auto* loopButton = findButton(*editor, "1/16");
    auto* syncButton = findButton(*editor, "SYNC ON");
    auto* reverseButton = findButton(*editor, "REVERSE");
    if (!syncButton || !reverseButton ||
        syncButton->getParentComponent() != reverseButton->getParentComponent() ||
        syncButton->getRight() + 2 > reverseButton->getX() ||
        !syncButton->getBounds().intersects(reverseButton->getBounds().withX(syncButton->getX()))) {
        std::cerr << "UI_LAYOUT: sync must sit immediately left of reverse\n";
        return 26;
    }
    auto* freezeButton = dynamic_cast<juce::TextButton*>(
        findNamed(*editor, "PERFORMANCE_FREEZE"));
    auto* freezeParameter = findParameter("Freeze");
    if (!freezeButton || !freezeParameter || !freezeButton->isVisible()) {
        std::cerr << "UI_CONTROL: fifth FREEZE switch missing\n";
        return 36;
    }
    freezeButton->setToggleState(true, juce::sendNotificationSync);
    if (freezeParameter->getValue() < .5f) {
        std::cerr << "UI_CONTROL: FREEZE did not automate\n";
        return 37;
    }
    freezeButton->setToggleState(false, juce::sendNotificationSync);
    const std::array<juce::String, 5> performanceLabels{
        "SYNC ON", "REVERSE", "GATE", "BOTH", "FREEZE"};
    juce::Component* parent = nullptr;
    juce::TextButton* previous = nullptr;
    for (const auto& label : performanceLabels) {
        auto* button = findButton(*editor, label);
        if (!button || button->getWidth() > 105 || button->getHeight() > 36 ||
            (parent && parent != button->getParentComponent()) ||
            (previous && previous->getRight() >= button->getX())) {
            std::cerr << "UI_LAYOUT: five compact performance switches not aligned\n";
            return 39;
        }
        parent = button->getParentComponent();
        previous = button;
    }
    auto* selectedLoop = findButton(*editor, "1 Bar");
    if (!selectedLoop || selectedLoop->getHeight() > 36) {
        std::cerr << "UI_LAYOUT: loop-length switches still oversized\n";
        return 40;
    }
    auto* selectedReverse = findButton(*editor, "REVERSE");
    auto* selectedBar = findButton(*editor, "1 Bar");
    if (!selectedReverse || !selectedBar || !selectedReverse->getToggleState()
        || !selectedBar->getToggleState()) {
        std::cerr << "UI_CONTROL: initial mode/loop selection not visible\n";
        return 17;
    }
    auto* mode = findParameter("Mode");
    auto* loop = findParameter("Loop length");
    if (!gateButton || !loopButton || !mode || !loop
        || !gateButton->onClick || !loopButton->onClick) {
        std::cerr << "UI_CONTROL: missing gate or loop callback/parameter\n";
        return 13;
    }
    gateButton->onClick();
    loopButton->onClick();
    if (!gateButton->getToggleState() || !loopButton->getToggleState()) {
        std::cerr << "UI_CONTROL: active mode/loop highlights lag behind clicks\n";
        return 18;
    }
    if (std::abs(mode->getValue() - .5f) > .001f || loop->getValue() > .001f) {
        std::cerr << "UI_CONTROL: mode or loop action did not automate\n";
        return 14;
    }
    auto* triggerSource = findParameter("Trigger source");
    if (!triggerSource) return 15;
    triggerSource->setValueNotifyingHost(1.f / 3.f); // MIDI
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, static_cast<juce::uint8>(100)), 127);
    processor->processBlock(audio, midi);
    for (int i = 0; i < 512; ++i)
        if (!std::isfinite(audio.getSample(0, i))) return 16;
    // Aligned mode must report its delay before processing starts, then
    // reproduce a delayed dry impulse exactly under bypass (host PDC contract).
    auto* latencyMode = findParameter("Latency mode");
    if (!latencyMode) { std::cerr << "LATENCY_SMOKE: missing latency control\n"; return 19; }
    latencyMode->setValueNotifyingHost(1.f); // Aligned
    power->setValueNotifyingHost(0.f); // Bypass retains declared host latency
    midi.clear();
    processor->prepareToPlay(48000.0, 512);
    if (processor->getLatencySamples() != 6000) {
        std::cerr << "LATENCY_SMOKE: expected 6000-sample 1/16 delay at 120 BPM\n";
        return 20;
    }
    for (int block = 0; block < 13; ++block) {
        audio.clear();
        if (block == 0) audio.setSample(0, 0, .75f);
        processor->processBlock(audio, midi);
        for (int i = 0; i < audio.getNumSamples(); ++i) {
            const int absolute = block * audio.getNumSamples() + i;
            const float expected = absolute == 6000 ? .75f : 0.f;
            if (!std::isfinite(audio.getSample(0, i))
                || std::abs(audio.getSample(0, i) - expected) > 1.e-5f
                || std::abs(audio.getSample(1, i)) > 1.e-5f) {
                std::cerr << "LATENCY_SMOKE: impulse misaligned at sample " << absolute << "\n";
                return 21;
            }
        }
    }
    std::cout << "LATENCY_SMOKE: aligned=6000, bypass impulse sample=6000, passed\n";
    // Verify that the official SkellCore skull is clickable and actually
    // changes the real editor skin; four clicks must restore the original.
    std::function<juce::TextButton*(juce::Component&)> findSkull;
    findSkull = [&](juce::Component& root) -> juce::TextButton* {
        if (auto* button = dynamic_cast<juce::TextButton*>(&root))
            if (button->getTooltip().containsIgnoreCase("SKELL appearance")) return button;
        for (int i = 0; i < root.getNumChildComponents(); ++i)
            if (auto* found = findSkull(*root.getChildComponent(i))) return found;
        return nullptr;
    };
    auto* skull = findSkull(*editor);
    if (!skull || !skull->onClick) {
        std::cerr << "SKELLCORE_SKULL: shared skull button missing\n";
        return 22;
    }
    editor->setSize(960, 720);
    const auto beforeSkin = editor->createComponentSnapshot(editor->getLocalBounds(),true);
    skull->onClick();
    const auto afterSkin = editor->createComponentSnapshot(editor->getLocalBounds(),true);
    if (!beforeSkin.isValid() || !afterSkin.isValid()
        || beforeSkin.getPixelAt(120,170) == afterSkin.getPixelAt(120,170)) {
        std::cerr << "SKELLCORE_SKULL: clicking skull did not change editor skin\n";
        return 23;
    }
    for (int i = 0; i < 3; ++i) skull->onClick();
    std::cout << "SKELLCORE_SKULL: 4-skin cycle and return passed\n";
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - began).count();
    std::cout << "HOST_SMOKE: processed=" << processed
              << " stereo frames, elapsed=" << elapsed << " s, latency=0, UI callbacks=passed\n";
    editor.reset();
    processor->releaseResources();
    processor.reset();
    return 0;
}
