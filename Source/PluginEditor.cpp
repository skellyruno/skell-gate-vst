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

    // Subtle grid background lines
    g.setColour(juce::Colours::darkgrey.withAlpha(0.3f));
    g.drawHorizontalLine(getLocalBounds().getCentreY(), 0.0f, static_cast<float>(getWidth()));
    g.drawVerticalLine(getWidth() / 2, 0.0f, static_cast<float>(getHeight()));

    // Active Playhead Cursor
    float phase = processor.getCurrentPhase();
    float cursorX = phase * getWidth();
    g.setColour(juce::Colours::lime.withAlpha(0.9f));
    g.fillRect(cursorX - 1.5f, 0.0f, 3.0f, static_cast<float>(getHeight()));
}

SkellgateAudioProcessorEditor::SkellgateAudioProcessorEditor (SkellgateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), patternEditor(p)
{
    setLookAndFeel(&skellLookAndFeel);

    addAndMakeVisible(patternEditor);

    // Preset Controls Setup
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
    presetDropdown.addItem("Slot 3: Tape Pitch Warp", 3);
    presetDropdown.addItem("Slot 4: Reverse Octave Stutter", 4);
    presetDropdown.addListener(this);
    addAndMakeVisible(presetDropdown);
    updatePresetSelection();

    // Setup 5 Clean Knobs
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

    setSize (720, 480);
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
        updatePresetSelection();
    }
    else if (button == &nextButton)
    {
        audioProcessor.nextPreset();
        updatePresetSelection();
    }
}

void SkellgateAudioProcessorEditor::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &presetDropdown)
    {
        int selectedId = presetDropdown.getSelectedId();
        if (selectedId > 0)
        {
            audioProcessor.setPreset(selectedId - 1);
            patternEditor.repaint();
        }
    }
}

void SkellgateAudioProcessorEditor::updatePresetSelection()
{
    presetDropdown.setSelectedId(audioProcessor.getCurrentPresetIndex() + 1, juce::dontSendNotification);
    patternEditor.repaint();
}

void SkellgateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB(22, 22, 22));

    // Sleek Brand Title
    g.setColour (juce::Colours::lime);
    g.setFont (juce::Font (juce::FontOptions().withName("Consolas").withHeight(24.0f).withStyle("Bold")));
    g.drawText ("SKELLGATE", 20, 15, 180, 30, juce::Justification::left);

    g.setFont (11.0f);
    g.setColour (juce::Colours::grey);
    g.drawText ("GLITCH BUFFER ENGINE", 145, 22, 180, 20, juce::Justification::left);
}

void SkellgateAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    // Top Bar (Bypass & Reverse)
    auto topArea = area.removeFromTop(40);
    bypassButton.setBounds(topArea.removeFromRight(80));
    reverseButton.setBounds(topArea.removeFromRight(95).reduced(0, 5));

    // Preset Selector Bar (Forward, Backward, Dropdown)
    auto presetBar = area.removeFromTop(35);
    prevButton.setBounds(presetBar.removeFromLeft(40));
    nextButton.setBounds(presetBar.removeFromLeft(40));
    presetBar.removeFromLeft(10);
    presetDropdown.setBounds(presetBar.removeFromLeft(280));

    area.removeFromTop(10);

    // Visualizer Window
    patternEditor.setBounds(area.removeFromTop(150));

    area.removeFromTop(15);

    // 5 Knobs Layout Grid (Attack, Release, Length, Time, Mix)
    auto knobWidth = area.getWidth() / 5;
    attackSlider.setBounds(area.removeFromLeft(knobWidth).reduced(6));
    releaseSlider.setBounds(area.removeFromLeft(knobWidth).reduced(6));
    lengthSlider.setBounds(area.removeFromLeft(knobWidth).reduced(6));
    timeSlider.setBounds(area.removeFromLeft(knobWidth).reduced(6));
    mixSlider.setBounds(area.removeFromLeft(knobWidth).reduced(6));
}
