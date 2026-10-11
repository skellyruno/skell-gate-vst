#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class SkellgateLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SkellgateLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::lime);
        setColour(juce::Slider::thumbColourId, juce::Colours::black);
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::darkgrey);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, const float rotaryStartAngle,
                           const float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Background Track
        juce::Path bgArc;
        bgArc.addCentredArc (center.x, center.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colours::darkgrey.darker(0.5f));
        g.strokePath (bgArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active Lime Arc
        if (sliderPos > 0.0f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (center.x, center.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour (juce::Colours::lime);
            g.strokePath (valueArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Thumb Knob
        auto thumbWidth = 8.0f;
        auto innerRadius = radius - 10.0f;
        juce::Point<float> thumbPoint (center.x + innerRadius * std::sin (angle),
                                       center.y - innerRadius * std::cos (angle));
        g.setColour (juce::Colours::lime);
        g.fillEllipse (juce::Rectangle<float> (thumbWidth, thumbWidth).withCentre (thumbPoint));
    }
};

class PatternEditorComponent : public juce::Component,
                               public juce::Timer
{
public:
    PatternEditorComponent(SkellgateAudioProcessor& p);
    ~PatternEditorComponent() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void timerCallback() override;

private:
    SkellgateAudioProcessor& processor;
};

class SkellgateAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Button::Listener
{
public:
    SkellgateAudioProcessorEditor (SkellgateAudioProcessor&);
    ~SkellgateAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked (juce::Button* button) override;

private:
    SkellgateAudioProcessor& audioProcessor;
    SkellgateLookAndFeel skellLookAndFeel;

    PatternEditorComponent patternEditor;
    juce::TextButton presetButton;

    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider lengthSlider;
    juce::Slider timeSlider;
    juce::Slider mixSlider;
    juce::Slider outputSlider;

    juce::ToggleButton bypassButton;
    juce::ToggleButton reverseButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> lengthAttachment;
    std::unique_ptr<SliderAttachment> timeAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ButtonAttachment> reverseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkellgateAudioProcessorEditor)
};
