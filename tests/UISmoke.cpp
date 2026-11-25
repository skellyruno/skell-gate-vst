#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include "../ui/GlitchPulse.h"
#include "../ui/TopBar.h"
#include "../ui/GateGrid.h"
#include "../ui/PresetBar.h"
#include "../PluginProcessor.h"

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
    if (editor->getWidth()<636 || editor->getWidth()>645 ||
        editor->getHeight()<476 || editor->getHeight()>487) {
        std::cerr << "UI_SIZE: default must be ~11 percent smaller than old 75 percent\n";
        return 56;
    }
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
            for (int yy = skell::theme::fadeInDial.getY(); yy < skell::theme::fadeInDial.getBottom(); ++yy)
                for (int xx = skell::theme::fadeInDial.getX(); xx < skell::theme::fadeInDial.getRight(); ++xx) {
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
            // The faceplate must actually look black now, not charcoal green.
            const auto exposed = frame.getPixelAt(185, 300);
            if (exposed.getRed() >= 45 || exposed.getGreen() >= 45
                || exposed.getBlue() >= 45) {
                std::cerr << "UI_THEME: background is not near-black\n";
                return 42;
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
    if (power == nullptr || power->getValue() < .5f) {
        std::cerr << "Expected enabled default starter effect\n";
        return 7;
    }
    // The new default is ON, but explicit bypass must still reproduce the
    // original dry signal sample-for-sample and preserve host latency.
    power->setValueNotifyingHost(0.f);
    processor->prepareToPlay(48000.0,512);
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
    // Preserve factory IDs 0..11 and verify new categorised programs
    // contain audible, distinct gate/reverse/Both settings without using
    // external host presets or file I/O.
    const auto names=SkellGlitchAudioProcessor::factoryPresetNames();
    auto* engine=dynamic_cast<SkellGlitchAudioProcessor*>(processor.get());
    if (!engine || names.size()!=28 || names[11]!="Default" ||
        engine->currentPresetName()!="Default") {
        std::cerr << "PRESET: stable factory index or Default name incorrect\n";
        return 51;
    }
    engine->loadFactoryPreset(17); // Gate / Sharp Sixteenths.
    if (engine->apvts.getRawParameterValue("mode")->load()!=1.f ||
        engine->apvts.getRawParameterValue("step2_state")->load()!=0.f) {
        std::cerr << "PRESET: new Gate program not audible/distinct\n";
        return 52;
    }
    engine->loadFactoryPreset(25); // Both / Slow Motion.
    if (engine->apvts.getRawParameterValue("mode")->load()!=2.f) {
        std::cerr << "PRESET: new Both program missing\n";
        return 53;
    }
    // Actual audio A/B: selecting a Gate preset must audibly differ from
    // the dry source without NaNs, overs or stereo image destruction.
    engine->loadFactoryPreset(17);
    engine->prepareToPlay(48000.0,512);
    juce::AudioBuffer<float> gateSignal(2,512);
    juce::MidiBuffer emptyMidi;
    double gateDifference=0.0;
    float maximum=0.f;
    for (int block=0;block<110;++block) {
        for (int sample=0;sample<512;++sample) {
            const double phase=(block*512+sample)*6.283185307179586*220.0/48000.0;
            const float dry=.26f*static_cast<float>(std::sin(phase));
            gateSignal.setSample(0,sample,dry);
            gateSignal.setSample(1,sample,-dry);
        }
        engine->processBlock(gateSignal,emptyMidi);
        for (int sample=0;sample<512;++sample) {
            const float dry=.26f*static_cast<float>(std::sin(
                (block*512+sample)*6.283185307179586*220.0/48000.0));
            const float left=gateSignal.getSample(0,sample);
            const float right=gateSignal.getSample(1,sample);
            if (!std::isfinite(left) || !std::isfinite(right) ||
                std::abs(left+right)>.001f) {
                std::cerr << "PRESET_AUDIO: gate introduced NaNs or stereo imbalance\n";
                return 57;
            }
            maximum=std::max(maximum,std::abs(left));
            gateDifference+=std::abs(left-dry);
        }
    }
    if (gateDifference<2.0 || maximum>.85f) {
        std::cerr << "PRESET_AUDIO: gate did not audibly modulate the signal or exceeded headroom\n";
        return 58;
    }
    std::cout << "PRESET_AUDIO: gate vs dry sum="<<gateDifference
              <<" peak="<<maximum<<" passed\n";
    engine->loadFactoryPreset(11);
    engine->prepareToPlay(48000.0,512);
    if (engine->apvts.getRawParameterValue("power")->load()<.5f ||
        engine->apvts.getRawParameterValue("loopLength")->load()!=2.f) {
        std::cerr << "PRESET: Default still bypassed or too slow\n";
        return 54;
    }
    std::cout << "PRESET: 28 factory programs, stable IDs and audible Default passed\n";
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
    // Simulate 90 seconds of UI ticks without sleeping or running the host.
    // The effect must be genuinely intermittent: several short bursts with
    // quiet time between them, never a constant flicker.
    GlitchPulse pulse(0x236cc08du);
    int activeFrames=0, currentRun=0, longestRun=0;
    for (int frame=0;frame<2700;++frame) {
        pulse.tick();
        if (pulse.intensity()>0) {
            ++activeFrames;
            ++currentRun;
            longestRun=std::max(longestRun,currentRun);
        } else currentRun=0;
    }
    if (pulse.bursts()<5 || pulse.bursts()>24 ||
        activeFrames<12 || activeFrames>125 || longestRun>5) {
        std::cerr << "UI_GLITCH: visual scheduler is too frequent, too long or inert\n";
        return 43;
    }
    auto* visualHeader = dynamic_cast<TopBar*>(findNamed(*editor,"SKELL_HEADER"));
    auto* visualSteps = dynamic_cast<GateGrid*>(findNamed(*editor,"SEQUENCER_GRID"));
    if (!visualHeader || !visualSteps) {
        std::cerr << "UI_GLITCH: animated UI targets missing\n";
        return 44;
    }
    editor->setSize(960,720);
    const auto calm = editor->createComponentSnapshot(editor->getLocalBounds(),true);
    // The small version must live below "CH", not to the right of GLITCH.
    // Check visible engraved pixels and absence of the previous right-side mark.
    int versionPixels=0, staleVersionPixels=0, headerStatusPixels=0;
    for (int y=39;y<=49;++y)
        for (int x=194;x<=238;++x) {
            const auto col=calm.getPixelAt(x,y);
            if (col.getRed()>55 && col.getGreen()>55 && col.getBlue()>55)
                ++versionPixels;
        }
    for (int y=16;y<=28;++y)
        for (int x=260;x<=310;++x) {
            const auto col=calm.getPixelAt(x,y);
            if (col.getRed()>65 && col.getGreen()>65 && col.getBlue()>65)
                ++staleVersionPixels;
        }
    // The former "ON" label occupied this otherwise empty header region.
    for (int y=19;y<=39;++y)
        for (int x=843;x<=876;++x) {
            const auto col=calm.getPixelAt(x,y);
            if (col.getGreen()>110 && col.getGreen()>col.getRed()+45)
                ++headerStatusPixels;
        }
    if (versionPixels<8 || staleVersionPixels>3 || headerStatusPixels>3) {
        std::cerr << "UI_HEADER: version placement or redundant ON label regressed "
                  << versionPixels << "/" << staleVersionPixels << "/"
                  << headerStatusPixels << "\n";
        return 76;
    }
    // Capture the real OFF state and reject the old separate red status LED.
    auto* offTestPower=findParameter("Power");
    if (!offTestPower) return 77;
    offTestPower->setValueNotifyingHost(offTestPower->convertTo0to1(0.f));
    visualHeader->repaint();
    const auto powerOff=editor->createComponentSnapshot(editor->getLocalBounds(),true);
    int redDotPixels=0;
    for (int y=37;y<=42;++y)
        for (int x=945;x<=949;++x) {
            const auto col=powerOff.getPixelAt(x,y);
            if (col.getRed()>115 && col.getRed()>col.getGreen()+40)
                ++redDotPixels;
        }
    offTestPower->setValueNotifyingHost(offTestPower->convertTo0to1(1.f));
    visualHeader->repaint();
    if (redDotPixels>0) {
        std::cerr << "UI_HEADER: separate red LED dot remains on power button\n";
        return 78;
    }
    {
        juce::FileOutputStream stream(directory.getChildFile("Skell-Glitch-Power-Off-960x720.png"));
        if (stream.failedToOpen() || !png.writeImageToStream(powerOff,stream))
            return 79;
    }
    visualHeader->setGlitchBurst(3,0xac861b32u);
    visualSteps->setGlitchBurst(3,0xac861b32u);
    const auto disrupted = editor->createComponentSnapshot(editor->getLocalBounds(),true);
    int skullPixels=0, stepPixels=0;
    for (int y=3;y<58;++y)
        for(int x=10;x<62;++x)
            if (calm.getPixelAt(x,y)!=disrupted.getPixelAt(x,y)) ++skullPixels;
    for (int y=visualSteps->getY();y<visualSteps->getY()+48;y+=2)
        for(int x=visualSteps->getX()+6;x<visualSteps->getRight()-3;x+=2)
            if (calm.getPixelAt(x,y)!=disrupted.getPixelAt(x,y)) ++stepPixels;
    if (skullPixels<8 || stepPixels<8) {
        std::cerr << "UI_GLITCH: burst does not visibly affect the skull and green sequencer\n";
        return 45;
    }
    const auto burstFile=directory.getChildFile("Skell-Glitch-Burst-960x720.png");
    juce::FileOutputStream burstStream(burstFile);
    if (burstStream.failedToOpen() || !png.writeImageToStream(disrupted,burstStream)) {
        std::cerr << "UI_GLITCH: unable to save actual burst screenshot\n";
        return 46;
    }
    visualHeader->setGlitchBurst(0,0u);
    visualSteps->setGlitchBurst(0,0u);
    visualHeader->setTitleGlitchEnabled(false);
    visualHeader->setGlitchBurst(3,0xac861b32u);
    const auto titleOff=editor->createComponentSnapshot(editor->getLocalBounds(),true);
    int titleChanges=0;
    for(int y=10;y<37;++y)
        for(int x=138;x<230;++x)
            if(calm.getPixelAt(x,y)!=titleOff.getPixelAt(x,y)) ++titleChanges;
    visualHeader->setGlitchBurst(0,0u);
    visualHeader->setTitleGlitchEnabled(true);
    if (titleChanges>2) {
        std::cerr << "UI_GLITCH: TITLE GLITCH off still moves title text\n";
        return 64;
    }

    std::cout << "UI_GLITCH: "<<pulse.bursts()<<" rare bursts in 90s, longest "
              <<longestRun<<" frames; changed skull="<<skullPixels
              <<" and steps="<<stepPixels<<" pixels\n";
    auto* settingsIcon = findNamed(*editor, "HEADER_SETTINGS");
    auto* powerIcon = findNamed(*editor, "HEADER_POWER");
    auto* settingsButton=dynamic_cast<juce::TextButton*>(settingsIcon);
    if (!settingsButton || !settingsButton->onClick) {
        std::cerr << "UI_SETTINGS: settings icon is not interactive\n";
        return 61;
    }
    settingsButton->onClick();
    const auto settingsScene=editor->createComponentSnapshot(editor->getLocalBounds(),true);
    juce::FileOutputStream settingsOutput(directory.getChildFile(
        "Skell-Glitch-Settings-960x720.png"));
    if (settingsOutput.failedToOpen() ||
        !png.writeImageToStream(settingsScene,settingsOutput)) {
        std::cerr << "UI_SETTINGS: failed to render real Settings screenshot\n";
        return 62;
    }
    settingsButton->onClick();
    if (!settingsIcon || !powerIcon || settingsIcon->getWidth() > 38 ||
        powerIcon->getWidth() > 38 || settingsIcon->getHeight() > 38 ||
        powerIcon->getHeight() > 38 ||
        settingsIcon->getRight() + 8 < powerIcon->getX() ||
        settingsIcon->getBounds().intersects(powerIcon->getBounds())) {
        std::cerr << "UI_LAYOUT: header gear and power controls are oversized or misaligned\n";
        return 38;
    }
    auto* startStatus = dynamic_cast<juce::Label*>(
        findNamed(*editor,"LIVE_START_STATUS"));
    if (!startStatus ||
        (!startStatus->getText().containsIgnoreCase("POWER") &&
         !startStatus->getText().containsIgnoreCase("ACTIVE")) ||
        !startStatus->getText().contains(" / ")) {
        std::cerr << "UI_START: active guidance missing or separator not printable\n";
        return 49;
    }
    // The low/high filter parameters remain available to restore legacy
    // presets, but the unwanted bar must not exist in the new editor.
    if (findNamed(*editor,"LOW CUT") || findNamed(*editor,"HIGH CUT")) {
        std::cerr << "UI_LAYOUT: low/high bar unexpectedly visible\n";
        return 50;
    }
    // The 4 knob faces must match in actual diameter and mirror exactly
    // around x=480, without touching the central sequencer lane.
    auto* leftTop=findNamed(*editor,"FADE IN");
    auto* rightTop=findNamed(*editor,"FADE OUT");
    auto* leftBottom=findNamed(*editor,"SMOOTH");
    auto* rightBottom=findNamed(*editor,"MIX");
    if (!leftTop || !rightTop || !leftBottom || !rightBottom ||
        leftTop->getWidth()!=rightTop->getWidth() ||
        leftTop->getWidth()!=leftBottom->getWidth() ||
        leftTop->getWidth()!=rightBottom->getWidth() ||
        std::min(leftTop->getWidth(),leftTop->getHeight()) !=
            std::min(leftBottom->getWidth(),leftBottom->getHeight()) ||
        std::abs(leftTop->getBounds().getCentreX() +
                 rightTop->getBounds().getCentreX()-960)>2 ||
        std::abs(leftBottom->getBounds().getCentreX() +
                 rightBottom->getBounds().getCentreX()-960)>2) {
        std::cerr << "UI_LAYOUT: four hardware knobs are not symmetrical\n";
        return 59;
    }
    auto* display = findNamed(*editor, "LIVE_WAVE_DISPLAY");
    if (!display || display->getParentComponent() != leftTop->getParentComponent() ||
        leftTop->getBounds().getCentreY() != rightTop->getBounds().getCentreY() ||
        leftBottom->getBounds().getCentreY() != rightBottom->getBounds().getCentreY() ||
        leftTop->getBounds().getCentreX() != leftBottom->getBounds().getCentreX() ||
        rightTop->getBounds().getCentreX() != rightBottom->getBounds().getCentreX() ||
        2*display->getBounds().getCentreX() != leftTop->getBounds().getCentreX()+rightTop->getBounds().getCentreX() ||
        2*display->getBounds().getCentreY() != leftTop->getBounds().getCentreY()+leftBottom->getBounds().getCentreY() ||
        display->getBounds().intersects(leftTop->getBounds()) ||
        display->getBounds().intersects(rightTop->getBounds()) ||
        display->getBounds().intersects(leftBottom->getBounds()) ||
        display->getBounds().intersects(rightBottom->getBounds()) ||
        display->getBottom()+12 > visualSteps->getY() ||
        visualSteps->getX()!=display->getX()+skell::theme::waveInset ||
        visualSteps->getRight()!=display->getRight()-skell::theme::waveInset) {
        std::cerr << "UI_LAYOUT: waveform is not exactly centered between four equal knobs or collides with steps\n";
        return 73;
    }
    std::cout << "UI_LAYOUT: waveform and four-knob centre match exactly\n";
    // Confirm balanced clearances and distinct title/value lanes, not just
    // symmetry. These previously looked cramped despite passing centering.
    const auto* topLeftValue=findNamed(*editor,"FADE IN VALUE");
    const auto* topRightValue=findNamed(*editor,"FADE OUT VALUE");
    const auto* bottomLeftValue=findNamed(*editor,"SMOOTH VALUE");
    const auto* bottomRightValue=findNamed(*editor,"MIX VALUE");
    if (!topLeftValue || !topRightValue || !bottomLeftValue || !bottomRightValue ||
        display->getX()-leftTop->getRight()!=20 ||
        rightTop->getX()-display->getRight()!=20 ||
        leftTop->getY()!=rightTop->getY() ||
        leftBottom->getY()!=rightBottom->getY() ||
        topLeftValue->getBottom()>=leftBottom->getY() ||
        topRightValue->getBottom()>=rightBottom->getY() ||
        topLeftValue->getX()+topLeftValue->getWidth()/2!=leftTop->getBounds().getCentreX() ||
        bottomRightValue->getBounds().getCentreX()!=rightBottom->getBounds().getCentreX() ||
        bottomLeftValue->getBounds().getCentreX()!=leftBottom->getBounds().getCentreX() ||
        topRightValue->getBounds().getCentreX()!=rightTop->getBounds().getCentreX()) {
        std::cerr << "UI_LAYOUT: dial clearance, caption or readout placement regressed\n";
        return 75;
    }
    auto* headerPreset = dynamic_cast<PresetBar*>(
        findNamed(*editor, "HEADER_PRESET_BAR"));
    auto* previousPreset = dynamic_cast<juce::TextButton*>(
        findNamed(*editor, "PRESET_PREVIOUS"));
    auto* nextPreset = dynamic_cast<juce::TextButton*>(
        findNamed(*editor, "PRESET_NEXT"));
    auto* selection = dynamic_cast<juce::TextButton*>(
        findNamed(*editor, "HEADER_PRESET_SELECTION"));
    auto* undoButton = dynamic_cast<juce::TextButton*>(
        findNamed(*editor, "PRESET_UNDO"));
    auto* redoButton = dynamic_cast<juce::TextButton*>(
        findNamed(*editor, "PRESET_REDO"));
    if (!headerPreset || !previousPreset || !nextPreset || !selection ||
        !undoButton || !redoButton ||
        !previousPreset->onClick || !nextPreset->onClick ||
        !selection->onClick || !undoButton->onClick || !redoButton->onClick ||
        findNamed(*editor,"PRESET_TOOLS_MENU") ||
        previousPreset->getParentComponent()!=headerPreset ||
        undoButton->getParentComponent()!=headerPreset ||
        redoButton->getParentComponent()!=headerPreset) {
        std::cerr << "UI_LAYOUT: compact preset selector, Undo/Redo or arrows missing\n";
        return 26;
    }
    previousPreset->onClick();
    if (engine->currentPresetName()=="Default") {
        std::cerr << "UI_PRESET: previous preset did not navigate\n";
        return 34;
    }
    nextPreset->onClick();
    if (engine->currentPresetName()!="Default") {
        std::cerr << "UI_PRESET: next preset did not restore Default\n";
        return 35;
    }
    if (!selection->getButtonText().endsWith(
            juce::String::fromUTF8("\xE2\x96\xBE"))) {
        std::cerr << "UI_PRESET: preset dropdown arrow became a missing-codepage glyph\n";
        return 73;
    }
    // Verify real PopupMenu category submenus and in-menu Save/Open commands
    // rather than a flat ComboBox or a duplicate TOOLS control.
    auto popup=headerPreset->buildPresetMenu();
    bool reverseCategory=false,gateCategory=false,bothCategory=false;
    bool saveItem=false,openItem=false,snapshots=false;
    juce::PopupMenu::MenuItemIterator topItems(popup);
    while(topItems.next()) {
        const auto& item=topItems.getItem();
        if(item.text=="Reverse" && item.subMenu) reverseCategory=true;
        if(item.text=="Gate" && item.subMenu) gateCategory=true;
        if(item.text=="Both" && item.subMenu) bothCategory=true;
        if(item.text=="A / B snapshots" && item.subMenu) snapshots=true;
    }
    juce::PopupMenu::MenuItemIterator allItems(popup,true);
    while(allItems.next()) {
        const auto& item=allItems.getItem();
        if(item.itemID==1001) saveItem=true;
        if(item.itemID==1002) openItem=true;
    }
    if(!reverseCategory || !gateCategory || !bothCategory ||
       !saveItem || !openItem || !snapshots) {
        std::cerr << "UI_PRESET: cascade categories or Save/Open commands missing\n";
        return 66;
    }
    auto* stepBack = dynamic_cast<juce::TextButton*>(
        findNamed(*editor,"STEP_PATTERN_PREVIOUS"));
    auto* stepNext = dynamic_cast<juce::TextButton*>(
        findNamed(*editor,"STEP_PATTERN_NEXT"));
    if (!stepBack || !stepNext || !stepBack->onClick || !stepNext->onClick ||
        stepBack->getButtonText()!=previousPreset->getButtonText() ||
        stepNext->getButtonText()!=nextPreset->getButtonText() ||
        stepBack->getY()>=visualSteps->getBottom() ||
        stepNext->getY()>=visualSteps->getBottom()) {
        std::cerr << "UI_PATTERN: matching navigational arrows missing\n";
        return 67;
    }
    stepNext->onClick();
    if (engine->apvts.state.getProperty("patternName").toString()!="Straight" ||
        engine->apvts.getRawParameterValue("step1_state")->load()!=1.f) {
        std::cerr << "UI_PATTERN: right arrow did not load first pattern\n";
        return 68;
    }
    stepNext->onClick();
    if (engine->apvts.state.getProperty("patternName").toString()!="Offbeat" ||
        engine->apvts.getRawParameterValue("step1_state")->load()!=0.f) {
        std::cerr << "UI_PATTERN: right arrow did not advance pattern\n";
        return 69;
    }
    stepBack->onClick();
    if (engine->apvts.state.getProperty("patternName").toString()!="Straight") {
        std::cerr << "UI_PATTERN: left arrow did not reverse pattern\n";
        return 70;
    }
    engine->loadFactoryPreset(11);
    if (std::abs(headerPreset->getBounds().getCentreX()-480)>1 ||
        std::abs(visualSteps->getBounds().getCentreX()-480)>1 ||
        std::abs(stepBack->getBounds().getCentreX()+
                 stepNext->getBounds().getCentreX()-960)>2 ||
        headerPreset->getY()>=59 || headerPreset->getHeight()>59 ||
        headerPreset->getWidth()<250) {
        std::cerr << "UI_LAYOUT: header, steps or pattern arrows not centered\n";
        return 27;
    }
    // The rate/style row must never intrude into the four rotary controls.
    // This caught an earlier 29-pixel downward shift that clipped MIX.
    auto* attackSetting=dynamic_cast<juce::Slider*>(
        findNamed(*editor,"SETTING_GATEATTACK"));
    if (!attackSetting || attackSetting->getTextFromValue(3.75)!="4") {
        std::cerr << "UI_SETTINGS: advanced formatter is not whole numbers\n";
        return 55;
    }
    // Checking getTextFromValue() alone misses the real bug: JUCE caches
    // the initial four-decimal string inside the slider's Label widget.
    juce::Label* liveReadout=nullptr;
    for(int i=0;i<attackSetting->getNumChildComponents();++i)
        if(auto* label=dynamic_cast<juce::Label*>(attackSetting->getChildComponent(i))) {
            liveReadout=label;
            break;
        }
    if (!liveReadout ||
        liveReadout->getText()!=juce::String(juce::roundToInt(attackSetting->getValue()))) {
        std::cerr << "UI_SETTINGS: real label still shows decimal places\n";
        return 65;
    }
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
    auto* valueLabel=dynamic_cast<juce::Label*>(findNamed(*editor,"FADE IN VALUE"));
    if (!valueLabel || valueLabel->getText()!="100 ms" || valueLabel->getBounds().getCentreX()!=leftTop->getBounds().getCentreX()) {
        std::cerr << "UI_CONTROL: persistent fade readout not aligned or updated\n";
        return 74;
    }
    int dialChangedPixels = 0, movingFadeArcPixels=0;
    const auto countArcChanges=[&](const juce::Image& a,const juce::Image& b,
                                  float cx,float cy) {
        int changed=0;
        for (int y=int(cy)-56;y<int(cy)+56;++y)
            for (int x=int(cx)-56;x<int(cx)+56;++x) {
                const float r=std::hypot(float(x)-cx,float(y)-cy);
                if (r<42.f || r>55.f) continue;
                const auto before=a.getPixelAt(x,y);
                const auto after=b.getPixelAt(x,y);
                const auto green=[](juce::Colour p) {
                    return p.getGreen()>120 &&
                           p.getGreen()>p.getRed()+30 &&
                           p.getGreen()>p.getBlue()+20;
                };
                if(green(before)!=green(after)) ++changed;
            }
        return changed;
    };
    for (int y = leftTop->getY(); y < leftTop->getBottom(); y += 2)
        for (int x = leftTop->getX(); x < leftTop->getRight(); x += 2)
            if (dialBefore.getPixelAt(x, y) != dialAfter.getPixelAt(x, y))
                ++dialChangedPixels;
    movingFadeArcPixels=countArcChanges(dialBefore,dialAfter, float(leftTop->getBounds().getCentreX()), float(leftTop->getBounds().getCentreY()));
    if (std::abs(fadeParameter->getValue() - .2f) > .005f ||
        dialChangedPixels < 50 || movingFadeArcPixels<24) {
        std::cerr << "UI_CONTROL: Fade In automation or moving green light broken\n";
        return 30;
    }
    fadeInKnob->setValue(0.0, juce::sendNotificationSync);
    auto* fadeOutKnob=dynamic_cast<juce::Slider*>(findNamed(*editor,"FADE OUT"));
    if(!fadeOutKnob) return 71;
    const auto outBefore=editor->createComponentSnapshot(editor->getLocalBounds(),true);
    fadeOutKnob->setValue(250.0,juce::sendNotificationSync);
    const auto outAfter=editor->createComponentSnapshot(editor->getLocalBounds(),true);
    const int outArcChanges=countArcChanges(outBefore,outAfter, float(rightTop->getBounds().getCentreX()), float(rightTop->getBounds().getCentreY()));
    fadeOutKnob->setValue(0.0,juce::sendNotificationSync);
    if (outArcChanges<24) {
        std::cerr << "UI_CONTROL: Fade Out green arc remains fixed\n";
        return 72;
    }
    std::cout << "UI_CONTROL: moving fade arcs in="<<movingFadeArcPixels
              << " out="<<outArcChanges<<" pixels\n";
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
    auto* selectedBar = findButton(*editor, "1/8");
    if (!selectedReverse || !selectedBar || !selectedReverse->getToggleState()
        || !selectedBar->getToggleState()) {
        std::cerr << "UI_CONTROL: initial reverse/short-loop selection not visible\n";
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
    // Verify that the official SkellCore skull cycles the three muted skins:
    // Obsidian, Amethyst, Crimson. Bright cream Ivory is intentionally skipped.
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
    juce::FileOutputStream altOutput(directory.getChildFile(
        "Skell-Glitch-Amethyst-960x720.png"));
    if (altOutput.failedToOpen() ||
        !png.writeImageToStream(afterSkin,altOutput)) {
        std::cerr << "UI_THEME: could not save true alternate-theme capture\n";
        return 63;
    }
    if (!beforeSkin.isValid() || !afterSkin.isValid()
        || beforeSkin.getPixelAt(120,170) == afterSkin.getPixelAt(120,170)) {
        std::cerr << "SKELLCORE_SKULL: clicking skull did not change editor skin\n";
        return 23;
    }
    for (int i = 0; i < 2; ++i) skull->onClick();
    if (skell::theme::ivory()) {
        std::cerr << "SKELLCORE_SKULL: unwanted bright skin in local cycle\n";
        return 48;
    }
    std::cout << "SKELLCORE_SKULL: 3-dark-skin cycle and return passed\n";
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - began).count();
    std::cout << "HOST_SMOKE: processed=" << processed
              << " stereo frames, elapsed=" << elapsed << " s, latency=0, UI callbacks=passed\n";
    editor.reset();
    processor->releaseResources();
    processor.reset();
    return 0;
}
