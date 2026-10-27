#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class GrossLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GrossLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::lime);
        setColour(juce::Slider::thumbColourId, juce::Colours::black);
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(40, 50, 40));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, const float rotaryStartAngle,
                           const float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Knob Dark Metal Body
        g.setColour (juce::Colour::fromRGB(25, 30, 25));
        g.fillEllipse (bounds);
        g.setColour (juce::Colours::lime.withAlpha(0.8f));
        g.drawEllipse (bounds, 2.0f);

        // Value Arc
        juce::Path valueArc;
        valueArc.addCentredArc (center.x, center.y, radius - 4.0f, radius - 4.0f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (juce::Colours::lime);
        g.strokePath (valueArc, juce::PathStrokeType (3.0f));

        // Indicator Line
        juce::Point<float> thumbPoint (center.x + (radius - 8.0f) * std::sin (angle),
                                       center.y - (radius - 8.0f) * std::cos (angle));
        g.setColour (juce::Colours::lime);
        g.drawLine (center.x, center.y, thumbPoint.x, thumbPoint.y, 2.5f);
    }
};

/** The LCD envelope editor.
    drag a node to move it, double-click empty space to add a node,
    double-click (or right-click) a node to delete it, hold Shift to bypass snapping. */
class GrossLCDGraphComponent : public juce::Component, public juce::Timer
{
public:
    GrossLCDGraphComponent(SkellgateAudioProcessor& p);
    ~GrossLCDGraphComponent() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void timerCallback() override;

    /** Grid size as a fraction of the 2-beat pattern, or 0 for no snapping. */
    void setSnap (float patternFraction) { snapX = patternFraction; repaint(); }

private:
    int findNodeAt (const SkellgateAudioProcessor::Curve& curve, juce::Point<int> pos) const;
    float snapPhase (float x, const juce::MouseEvent& e) const;

    SkellgateAudioProcessor& processor;
    int draggedNodeIndex = -1;
    float snapX = 0.0f;
};

class SkellgateAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Button::Listener,
                                      private juce::Timer
{
public:
    SkellgateAudioProcessorEditor (SkellgateAudioProcessor&);
    ~SkellgateAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked (juce::Button* button) override;
    void updateSlotMatrixUI();

private:
    void timerCallback() override;
    void setSnapIndex (int newIndex);

    SkellgateAudioProcessor& audioProcessor;
    GrossLookAndFeel grossLookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 600 };

    GrossLCDGraphComponent lcdGraph;

    // Header Controls
    juce::Slider timeMixSlider;
    juce::Slider volMixSlider;
    juce::Slider offsetSlider;
    juce::ToggleButton holdButton;

    // Bottom Knobs
    juce::Slider attSlider;
    juce::Slider relSlider;
    juce::Slider tnsSlider;

    // 36 Time Pads (Green Grid) + 36 Volume Pads (Orange Grid)
    std::vector<std::unique_ptr<juce::TextButton>> timePads;
    std::vector<std::unique_ptr<juce::TextButton>> volPads;

    // Vertical Toolbar Buttons: snap grids (in fractions of a beat) and lane view switch
    juce::TextButton snap1_16, snap1_12, snap1_8, snap1_6, snap1_4, snap1_3;
    juce::TextButton viewTimeBtn, viewVolBtn;
    juce::TextButton* snapButtons[6] = { &snap1_16, &snap1_12, &snap1_8, &snap1_6, &snap1_4, &snap1_3 };
    int snapIndex = -1;

    int shownTimePreset = -1, shownVolPreset = -1, shownLane = -1;
    double shownBpm = -1.0;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> timeMixAttachment;
    std::unique_ptr<SliderAttachment> volMixAttachment;
    std::unique_ptr<SliderAttachment> offsetAttachment;
    std::unique_ptr<SliderAttachment> attAttachment;
    std::unique_ptr<SliderAttachment> relAttachment;
    std::unique_ptr<SliderAttachment> tnsAttachment;
    std::unique_ptr<ButtonAttachment> holdAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SkellgateAudioProcessorEditor)
};
