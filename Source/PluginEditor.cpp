#include "PluginProcessor.h"
#include "PluginEditor.h"

PatternEditorComponent::PatternEditorComponent(SkellgateAudioProcessor& p) : processor(p)
{
    startTimerHz(30); // 30 FPS smooth playhead animation
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
    // Sleek dark cyberpunk panel background
    g.fillAll(juce::Colour::fromRGB(18, 18, 18));
    g.setColour(juce::Colours::lime.withAlpha(0.7f));
    g.drawRect(getLocalBounds(), 2);

    // Draw grid lines
    g.setColour(juce::Colours::darkgrey.withAlpha(0.4f));
    g.drawHorizontalLine(getLocalBounds().getCentreY(), 0.0f, static_cast<float>(getWidth()));

    // Draw Preset Title & Info
    g.setFont(14.0f);
    int preset = processor.getCurrentPresetIndex();
    juce::String presetName;
    if (preset == 0) presetName = "Preset 1: Default Pass-Through";
    else if (preset == 1) presetName = "Preset 2: Stutter Gate 1/8";
    else if (preset == 2) presetName = "Preset 3: Tape Pitch Warp";
    else presetName = "Preset 4: Reverse Octave Stutter";

    g.setColour(juce::Colours::lime);
    g.drawText(presetName, getLocalBounds().removeFromTop(30), juce::Justification::centred, true);

    // Draw Live Playhead Cursor across the visualizer grid
    float phase = processor.getCurrentPhase();
    float cursorX = phase * getWidth();
    g.setColour(juce::Colours::lime.withAlpha(0.9f));
    g.fillRect(cursorX - 1.0f, 30.0f, 2.0f, static_cast<float>(getHeight() - 30));
}

void PatternEditorComponent::mouseDown(const juce::MouseEvent& /*event*/)
{
    processor.nextPreset();
    repaint();
}

SkellgateAudioProcessorEditor::SkellgateAudioProcessorEditor (SkellgateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), patternEditor(p)
{
    setStyle(&skellLookAndFeel, false);

    addAndMakeVisible(patternEditor);

    presetButton.setButtonText("NEXT PRESET SLOT");
    presetButton.addListener(this);
    presetButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    presetButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lime);
    addAndMakeVisible(presetButton);

    auto setupSlider = [this](juce::Slider& slider, const juce::String& /*name*/) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        addAndMakeVisible(slider);
    };

    setupSlider(attackSlider, "Attack");
    setupSlider(releaseSlider, "Release");
    setupSlider(lengthSlider, "Length");
    setupSlider(timeSlider, "Time");
    setupSlider(mixSlider, "Mix");
    setupSlider(outputSlider, "Output");

    attackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "attack", attackSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "release", releaseSlider);
    lengthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "length", lengthSlider);
    timeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "time", timeSlider);
    mixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "mix", mixSlider);
    outputAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "output", outputSlider);

    bypassButton.setButtonText("BYPASS");
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "bypass", bypassButton);

    reverseButton.setButtonText("REVERSE");
    addAndMakeVisible(reverseButton);
    reverseAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "reverse", reverseButton);

    setSize (750, 500);
}

SkellgateAudioProcessorEditor::~SkellgateAudioProcessorEditor()
{
    setStyle(nullptr, false);
}

void SkellgateAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &presetButton)
    {
        audioProcessor.nextPreset();
        patternEditor.repaint();
    }
}

void SkellgateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB(24, 24, 24));

    // Plugin Title Brand Header
    g.setColour (juce::Colours::lime);
    g.setFont (juce::Font("Consolas", 26.0f, juce::Font::bold));
    g.drawText ("SKELLGATE", 20, 15, 200, 30, juce::Justification::left);

    g.setFont (12.0f);
    g.setColour (juce::Colours::lightgrey);
    g.drawText ("TIME & BUFFER MANIPULATOR", 155, 22, 200, 20, juce::Justification::left);
}

void SkellgateAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    auto topArea = area.removeFromTop(40);
    bypassButton.setBounds(topArea.removeFromRight(80));
    reverseButton.setBounds(topArea.removeFromRight(95).reduced(0, 5));

    // Pattern Screen Area
    auto patternArea = area.removeFromTop(180);
    patternEditor.setBounds(patternArea.removeFromTop(130));
    presetButton.setBounds(patternArea.reduced(180, 5));

    area.removeFromTop(15);

    // 6 Knobs Layout Grid (Attack, Release, Length, Time, Mix, Output)
    auto knobWidth = area.getWidth() / 6;
    attackSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    releaseSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    lengthSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    timeSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    mixSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    outputSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
}
