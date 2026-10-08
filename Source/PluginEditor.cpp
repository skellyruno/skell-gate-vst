#include "PluginProcessor.h"
#include "PluginEditor.h"

PatternEditorComponent::PatternEditorComponent(SkellgateAudioProcessor& p) : processor(p) {}

void PatternEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    g.setColour(juce::Colours::green);
    g.drawRect(getLocalBounds(), 2);

    g.setFont(14.0f);
    int preset = processor.getCurrentPresetIndex();
    juce::String presetName = (preset == 0) ? "Preset: Default (1:1 Pass-through)" : (preset == 1) ? "Preset: Stutter Gate" : "Preset: Reverse Warp";
    g.drawText(presetName, getLocalBounds(), juce::Justification::centred, true);
}

void PatternEditorComponent::mouseDown(const juce::MouseEvent& /*event*/)
{
    processor.nextPreset();
    repaint();
}

SkellgateAudioProcessorEditor::SkellgateAudioProcessorEditor (SkellgateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), patternEditor(p)
{
    addAndMakeVisible(patternEditor);

    presetButton.setButtonText("Next Preset");
    presetButton.addListener(this);
    addAndMakeVisible(presetButton);

    auto setupSlider = [this](juce::Slider& slider, const juce::String& /*name*/) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        addAndMakeVisible(slider);
    };

    setupSlider(attackSlider, "Attack");
    setupSlider(releaseSlider, "Release");
    setupSlider(lengthSlider, "Length");
    setupSlider(timeSlider, "Time");

    attackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "attack", attackSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "release", releaseSlider);
    lengthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "length", lengthSlider);
    timeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "time", timeSlider);

    bypassButton.setButtonText("Bypass");
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "bypass", bypassButton);

    reverseButton.setButtonText("Reverse");
    addAndMakeVisible(reverseButton);
    reverseAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "reverse", reverseButton);

    setSize (700, 450);
}

SkellgateAudioProcessorEditor::~SkellgateAudioProcessorEditor()
{
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
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::green);
    g.setFont (24.0f);
    g.drawText ("Skellgate", 20, 15, 200, 30, juce::Justification::left);
}

void SkellgateAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    auto topArea = area.removeFromTop(40);
    bypassButton.setBounds(topArea.removeFromRight(80));
    reverseButton.setBounds(topArea.removeFromRight(90).reduced(0, 5));

    auto patternArea = area.removeFromTop(200);
    patternEditor.setBounds(patternArea.removeFromTop(160));
    presetButton.setBounds(patternArea.reduced(200, 5));

    area.removeFromTop(10);

    auto knobWidth = area.getWidth() / 4;
    attackSlider.setBounds(area.removeFromLeft(knobWidth).reduced(10));
    releaseSlider.setBounds(area.removeFromLeft(knobWidth).reduced(10));
    lengthSlider.setBounds(area.removeFromLeft(knobWidth).reduced(10));
    timeSlider.setBounds(area.removeFromLeft(knobWidth).reduced(10));
}
