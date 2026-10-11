#include "PluginProcessor.h"
#include "PluginEditor.h"

PatternEditorComponent::PatternEditorComponent(SkellgateAudioProcessor& p) : processor(p)
{
    startTimerHz(60); // 60 FPS buttery smooth animation
}

PatternEditorComponent::~PatternEditorComponent()
{
    stopTimer();
}

void PatternEditorComponent::timerCallback()
{
    animPulse += 0.05f;
    repaint();
}

void PatternEditorComponent::paint(juce::Graphics& g)
{
    // Deep cyberpunk background with pulsing ambient glow
    g.fillAll(juce::Colour::fromRGB(12, 12, 12));

    float audioLevel = processor.getCurrentAudioLevel();
    auto glowAlpha = juce::jlimit(0.1f, 0.4f, audioLevel * 1.5f);
    g.setColour(juce::Colours::lime.withAlpha(glowAlpha));
    g.drawRect(getLocalBounds(), 3);

    // Dancing Grid Lines
    g.setColour(juce::Colours::darkgrey.withAlpha(0.35f));
    for (int i = 1; i < 4; ++i)
    {
        float y = static_cast<float>(getHeight() * i / 4);
        g.drawHorizontalLine(static_cast<int>(y), 0.0f, static_cast<float>(getWidth()));
    }
    for (int i = 1; i < 8; ++i)
    {
        float x = static_cast<float>(getWidth() * i / 8);
        g.drawVerticalLine(static_cast<int>(x), 0.0f, static_cast<float>(getHeight()));
    }

    // Draw Interactive Curve Lines & Glowing Nodes
    auto& nodes = processor.getCurveNodes();
    if (nodes.size() > 1)
    {
        juce::Path curvePath;
        curvePath.startNewSubPath(nodes[0].x * static_cast<float>(getWidth()), (1.0f - nodes[0].y) * static_cast<float>(getHeight()));
        for (size_t i = 1; i < nodes.size(); ++i)
        {
            curvePath.lineTo(nodes[i].x * static_cast<float>(getWidth()), (1.0f - nodes[i].y) * static_cast<float>(getHeight()));
        }

        // Neon Glow Pass
        g.setColour(juce::Colours::lime.withAlpha(0.3f + 0.1f * std::sin(animPulse)));
        g.strokePath(curvePath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Sharp Neon Core Pass
        g.setColour(juce::Colours::lime);
        g.strokePath(curvePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw Draggable Breakpoint Nodes
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            float px = nodes[i].x * static_cast<float>(getWidth());
            float py = (1.0f - nodes[i].y) * static_cast<float>(getHeight());
            g.setColour(juce::Colours::black);
            g.fillEllipse(px - 6.0f, py - 6.0f, 12.0f, 12.0f);
            g.setColour(juce::Colours::lime);
            g.drawEllipse(px - 6.0f, py - 6.0f, 12.0f, 12.0f, 2.5f);
        }
    }

    // Animated Playhead Cursor
    float phase = processor.getCurrentPhase();
    float cursorX = phase * static_cast<float>(getWidth());
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.fillRect(cursorX - 1.5f, 0.0f, 3.0f, static_cast<float>(getHeight()));
    
    // Playhead glow
    g.setColour(juce::Colours::lime.withAlpha(0.5f));
    g.fillRect(cursorX - 4.0f, 0.0f, 8.0f, static_cast<float>(getHeight()));
}

void PatternEditorComponent::mouseDown(const juce::MouseEvent& event)
{
    auto& nodes = processor.getCurveNodes();
    draggedNodeIndex = -1;

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        float px = nodes[i].x * static_cast<float>(getWidth());
        float py = (1.0f - nodes[i].y) * static_cast<float>(getHeight());
        if (event.getPosition().getDistanceFrom({static_cast<int>(px), static_cast<int>(py)}) < 14)
        {
            draggedNodeIndex = static_cast<int>(i);
            break;
        }
    }
}

void PatternEditorComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (draggedNodeIndex != -1)
    {
        float normalizedY = 1.0f - (static_cast<float>(event.getPosition().y) / static_cast<float>(getHeight()));
        processor.updateNodePosition(draggedNodeIndex, normalizedY);
        repaint();
    }
}

SkellgateAudioProcessorEditor::SkellgateAudioProcessorEditor (SkellgateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), patternEditor(p)
{
    setLookAndFeel(&skellLookAndFeel);

    addAndMakeVisible(patternEditor);

    prevButton.setButtonText("<");
    prevButton.addListener(this);
    prevButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(26, 26, 26));
    prevButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lime);
    addAndMakeVisible(prevButton);

    nextButton.setButtonText(">");
    nextButton.addListener(this);
    nextButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(26, 26, 26));
    nextButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lime);
    addAndMakeVisible(nextButton);

    presetDropdown.addItem("Slot 1: Pass-Through (Clean)", 1);
    presetDropdown.addItem("Slot 2: 1/8 Stutter Gate", 2);
    presetDropdown.addItem("Slot 3: Tape Stop Warp", 3);
    presetDropdown.addItem("Slot 4: Half-Time Stretch", 4);
    presetDropdown.addItem("Slot 5: Reverse Sweep", 5);
    presetDropdown.addItem("Slot 6: Vinyl Scratch", 6);
    presetDropdown.addItem("Slot 7: Granular Scatter", 7);
    presetDropdown.addItem("Slot 8: Double-Time Glitch", 8);
    presetDropdown.addItem("Slot 9: Pitch Rise", 9);
    presetDropdown.addItem("Slot 10: Trance Gater", 10);
    presetDropdown.addItem("Slot 11: Chaos Loop A", 11);
    presetDropdown.addItem("Slot 12: Chaos Loop B", 12);
    presetDropdown.addItem("Slot 13: Stutter 1/4", 13);
    presetDropdown.addItem("Slot 14: Sub Drop Warp", 14);
    presetDropdown.addItem("Slot 15: Fast Glitch", 15);
    presetDropdown.addItem("Slot 16: Cosmic Swell", 16);
    presetDropdown.addListener(this);
    addAndMakeVisible(presetDropdown);

    // Create Gross Beat Style Slot Matrix (16 Performance Buttons)[cite: 3]
    for (int i = 0; i < 16; ++i)
    {
        auto btn = std::make_unique<juce::TextButton>();
        btn->setButtonText("S" + juce::String(i + 1));
        btn->addListener(this);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(20, 20, 20));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colours::lime);
        addAndMakeVisible(btn.get());
        slotButtons.push_back(std::move(btn));
    }

    updatePresetUI();

    auto setupSlider = [this](juce::Slider& slider) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 16);
        addAndMakeVisible(slider);
    };

    setupSlider(attackSlider);
    setupSlider(releaseSlider);
    setupSlider(lengthSlider);
    setupSlider(timeSlider);
    setupSlider(mixSlider);

    attackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "attack", attackSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "release", releaseSlider);
    lengthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "length", lengthSlider);
    timeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "time", timeSlider);
    mixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "mix", mixSlider);

    bypassButton.setButtonText("BYPASS");
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "bypass", bypassButton);

    reverseButton.setButtonText("REVERSE");
    addAndMakeVisible(reverseButton);
    reverseAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "reverse", reverseButton);

    setSize (900, 560);
}

SkellgateAudioProcessorEditor::~SkellgateAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void SkellgateAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &prevButton)
    {
        audioProcessor.previousPreset();
        updatePresetUI();
    }
    else if (button == &nextButton)
    {
        audioProcessor.nextPreset();
        updatePresetUI();
    }
    else
    {
        // Check slot matrix buttons
        for (size_t i = 0; i < slotButtons.size(); ++i)
        {
            if (button == slotButtons[i].get())
            {
                audioProcessor.loadPreset(static_cast<int>(i));
                updatePresetUI();
                break;
            }
        }
    }
}

void SkellgateAudioProcessorEditor::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &presetDropdown)
    {
        int selectedId = presetDropdown.getSelectedId();
        if (selectedId > 0)
        {
            audioProcessor.loadPreset(selectedId - 1);
            updatePresetUI();
        }
    }
}

void SkellgateAudioProcessorEditor::updatePresetUI()
{
    int currentIdx = audioProcessor.getCurrentPresetIndex();
    presetDropdown.setSelectedId(currentIdx + 1, juce::dontSendNotification);

    for (size_t i = 0; i < slotButtons.size(); ++i)
    {
        if (static_cast<int>(i) == currentIdx)
        {
            slotButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colours::lime.darker(0.2f));
            slotButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        }
        else
        {
            slotButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(20, 20, 20));
            slotButtons[i]->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        }
    }
    patternEditor.repaint();
}

void SkellgateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB(18, 18, 18));

    // Glowing Neon Brand Title
    g.setColour (juce::Colours::lime.withAlpha(0.3f));
    g.setFont (juce::Font (juce::FontOptions().withName("Consolas").withHeight(26.0f).withStyle("Bold")));
    g.drawText ("SKELLGATE", 22, 17, 200, 30, juce::Justification::left);

    g.setColour (juce::Colours::lime);
    g.drawText ("SKELLGATE", 20, 15, 200, 30, juce::Justification::left);

    g.setFont (11.0f);
    g.setColour (juce::Colours::grey);
    g.drawText ("ULTIMATE TIME-WARP GLITCH ENGINE", 155, 22, 250, 20, juce::Justification::left);
}

void SkellgateAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    auto topArea = area.removeFromTop(40);
    bypassButton.setBounds(topArea.removeFromRight(80));
    reverseButton.setBounds(topArea.removeFromRight(95).reduced(0, 5));

    // Preset & Slot Controls Bar
    auto presetBar = area.removeFromTop(35);
    prevButton.setBounds(presetBar.removeFromLeft(35));
    nextButton.setBounds(presetBar.removeFromLeft(35));
    presetBar.removeFromLeft(10);
    presetDropdown.setBounds(presetBar.removeFromLeft(260));

    area.removeFromTop(10);

    // Main Body split: Left = Gross Beat Slot Matrix[cite: 3], Right = Interactive Graph LCD
    auto mainBody = area.removeFromTop(200);
    
    // Slot Matrix Grid (4x4 layout)[cite: 3]
    auto slotGridArea = mainBody.removeFromLeft(210);
    int slotW = slotGridArea.getWidth() / 4;
    int slotH = slotGridArea.getHeight() / 4;
    for (int i = 0; i < 16; ++i)
    {
        int row = i / 4;
        int col = i % 4;
        slotButtons[i]->setBounds(slotGridArea.getX() + col * slotW + 2,
                                  slotGridArea.getY() + row * slotH + 2,
                                  slotW - 4, slotH - 4);
    }

    mainBody.removeFromLeft(15);
    patternEditor.setBounds(mainBody);

    area.removeFromTop(15);

    // 5 Core Performance Knobs Layout
    auto knobWidth = area.getWidth() / 5;
    attackSlider.setBounds(area.removeFromLeft(knobWidth).reduced(8));
    releaseSlider.setBounds(area.removeFromLeft(knobWidth).reduced(8));
    lengthSlider.setBounds(area.removeFromLeft(knobWidth).reduced(8));
    timeSlider.setBounds(area.removeFromLeft(knobWidth).reduced(8));
    mixSlider.setBounds(area.removeFromLeft(knobWidth).reduced(8));
}
