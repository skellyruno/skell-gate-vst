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
        setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(24, 24, 24));
        setColour(juce::ComboBox::outlineColourId, juce::Colours::lime);
        setColour(juce::ComboBox::arrowColourId, juce::Colours::lime);
        setColour(juce::ComboBox::textColourId, juce::Colours::lime);
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour::fromRGB(18, 18, 18));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colours::lime.withAlpha(0.3f));
        setColour(juce::PopupMenu::textColourId, juce::Colours::lightgrey);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::lime);
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

        // Glowing outer aura ring
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

class PatternEditorComponent : public juce::Component, public juce::Timer
{
public:
    PatternEditorComponent(SkellgateAudioProcessor& p);
    ~PatternEditorComponent() override;
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
                                      public juce::Button::Listener,
                                      public juce::ComboBox::Listener
{
public:
    SkellgateAudioProcessorEditor (SkellgateAudioProcessor&);
    ~SkellgateAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked (juce::Button* button) override;
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;
    void updatePresetUI();

private:
    SkellgateAudioProcessor& audioProcessor;
    SkellgateLookAndFeel skellLookAndFeel;

    PatternEditorComponent patternEditor;
    
    juce::TextButton prevButton;
    juce::TextButton nextButton;
    juce::ComboBox presetDropdown;

    // Gross Beat Style Slot Matrix Buttons (16 Slots)[cite: 3]
    std::vector<std::unique_ptr<juce::TextButton>> slotButtons;

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
