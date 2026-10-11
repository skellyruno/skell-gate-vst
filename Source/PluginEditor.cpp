#include "PluginProcessor.h"
#include "PluginEditor.h"

PatternEditorComponent::PatternEditorComponent(SkellgateAudioProcessor& p) : processor(p)
{
    startTimerHz(30);
}

PatternEditorComponent::~PatternEditorComponent()
{
    stopTimer();
}

void PatternEditorComponent::timerCallback()
{
    repaint();
}

void PatternEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(16, 16, 16));
    g.setColour(juce::Colours::lime.withAlpha(0.6f));
    g.drawRect(getLocalBounds(), 2);

    // Grid lines
    g.setColour(juce::Colours::darkgrey.withAlpha(0.3f));
    g.drawHorizontalLine(getLocalBounds().getCentreY(), 0.0f, static_cast<float>(getWidth()));

    // Draw Interactive Curve Lines & Nodes
    auto& nodes = processor.getCurveNodes();
    if (nodes.size() > 1)
    {
        juce::Path curvePath;
        curvePath.startNewPath(nodes[0].x * getWidth(), (1.0f - nodes[0].y) * getHeight());
        for (size_t i = 1; i < nodes.size(); ++i)
        {
            curvePath.lineTo(nodes[i].x * getWidth(), (1.0f - nodes[i].y) * getHeight());
        }

        g.setColour(juce::Colours::lime);
        g.strokePath(curvePath, juce::PathStrokeType(2.5f));

        // Draw draggable nodes
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            float px = nodes[i].x * getWidth();
            float py = (1.0f - nodes[i].y) * getHeight();
            g.setColour(juce::Colours::black);
            g.fillEllipse(px - 5.0f, py - 5.0f, 10.0f, 10.0f);
            g.setColour(juce::Colours::lime);
            g.drawEllipse(px - 5.0f, py - 5.0f, 10.0f, 10.0f, 2.0f);
        }
    }

    // Playhead Cursor
    float phase = processor.getCurrentPhase();
    float cursorX = phase * getWidth();
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.fillRect(cursorX - 1.0f, 0.0f, 2.0f, static_cast<float>(getHeight()));
}

void PatternEditorComponent::mouseDown(const juce::MouseEvent& event)
{
    auto& nodes = processor.getCurveNodes();
    draggedNodeIndex = -1;

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        float px = nodes[i].x * getWidth();
        float py = (1.0f - nodes[i].y) * getHeight();
        if (event.getPosition().getDistanceFrom({static_cast<int>(px), static_cast<int>(py)}) < 12)
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
    prevButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(30, 30, 30));
    prevButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lime);
    addAndMakeVisible(prevButton);

    nextButton.setButtonText(">");
    nextButton.addListener(this);
    nextButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(30, 30, 30));
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
    presetDropdown.addListener(this);
    addAndMakeVisible(presetDropdown);
    updatePresetDropdown();

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

    setSize (720, 500);
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
        updatePresetDropdown();
    }
    else if (button == &nextButton)
    {
        audioProcessor.nextPreset();
        updatePresetDropdown();
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
            patternEditor.repaint();
        }
    }
}

void SkellgateAudioProcessorEditor::updatePresetDropdown()
{
    presetDropdown.setSelectedId(audioProcessor.getCurrentPresetIndex() + 1, juce::dontSendNotification);
    patternEditor.repaint();
}

void SkellgateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB(22, 22, 22));

    g.setColour (juce::Colours::lime);
    g.setFont (juce::Font (juce::FontOptions().withName("Consolas").withHeight(24.0f).withStyle("Bold")));
    g.drawText ("SKELLGATE", 20, 15, 180, 30, juce::Justification::left);

    g.setFont (11.0f);
    g.setColour (juce::Colours::grey);
    g.drawText ("INTERACTIVE TIME-WARP ENGINE", 145, 22, 220, 20, juce::Justification::left);
}

void SkellgateAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    auto topArea = area.removeFromTop(40);
    bypassButton.setBounds(topArea.removeFromRight(80));
    reverseButton.setBounds(topArea.removeFromRight(95).reduced(0, 5));

    auto presetBar = area.removeFromTop(35);
    prevButton.setBounds(presetBar.removeFromLeft(40));
    nextButton.setBounds(presetBar.removeFromLeft(40));
    presetBar.removeFromLeft(10);
    presetDropdown.setBounds(presetBar.removeFromLeft(280));

    area.removeFromTop(10);

    patternEditor.setBounds(area.removeFromTop(160));

    area.removeFromTop(15);

    auto knobWidth = area.getWidth() / 5;
    attackSlider.setBounds(area.removeFromLeft(knobWidth).reduced(6));
    releaseSlider.setBounds(area.removeFromLeft(knobWidth).reduced(6));
    lengthSlider.setBounds(area.removeFromLeft(knobWidth).reduced(6));
    timeSlider.setBounds(area.removeFromLeft(knobWidth).reduced(6));
    mixSlider.setBounds(area.removeFromLeft(knobWidth).reduced(6));
}
