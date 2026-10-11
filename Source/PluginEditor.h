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

        juce::Path bgArc;
        bgArc.addCentredArc (center.x, center.y, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colours::darkgrey.darker(0.6f));
        g.strokePath (bgArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (sliderPos > 0.0f)
        {
            juce::Path valueArc;
            valueArc.addCentredArc (center.x, center.y, radius, radius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour (juce::Colours::lime);
            g.strokePath (valueArc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour(juce::Colours::lime.withAlpha(0.25f));
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        auto thumbWidth = 8.0f;
        auto innerRadius = radius - 10.0f;
        juce::Point<float> thumbPoint (center.x + innerRadius * std::sin (angle),
                                       center.y - innerRadius * std::cos (angle));
        g.setColour (juce::Colours::lime);
        g.fillEllipse (juce::Rectangle<float> (thumbWidth, thumbWidth).withCentre (thumbPoint));
    }
};

class DualGraphComponent : public juce::Component, public juce::Timer
{
public:
    DualGraphComponent(SkellgateAudioProcessor& p);
    ~DualGraphComponent() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void timerCallback() override;

private:
    SkellgateAudioProcessor& processor;
    int draggedNodeIndex = -1;
    float animPulse = 0.0f;
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
    void updateSlotMatrixUI();

private:
    SkellgateAudioProcessor& audioProcessor;
    SkellgateLookAndFeel skellLookAndFeel;

    DualGraphComponent dualGraph;

    // Lane Toggle Buttons (Time vs Volume)
    juce::TextButton timeLaneButton;
    juce::TextButton volumeLaneButton;

    // Dual 12-Slot Matrices (Gross Beat Style)[cite: 3]
    std::vector<std::unique_ptr<juce::TextButton>> timeSlotButtons;
    std::vector<std::unique_ptr<juce::TextButton>> volumeSlotButtons;

    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider lengthSlider;
    juce::Slider timeSlider;
    juce::Slider mixSlider;

    juce::ToggleButton bypassButton;
    juce::ToggleButton reverseButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> lengthAttachment;
    std::unique_ptr<SliderAttachment> timeAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ButtonAttachment> reverseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkellgateAudioProcessorEditor)
};
