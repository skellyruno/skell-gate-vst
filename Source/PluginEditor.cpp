#include "PluginProcessor.h"
#include "PluginEditor.h"

GrossLCDGraphComponent::GrossLCDGraphComponent(SkellgateAudioProcessor& p) : processor(p)
{
    startTimerHz(60);
}

GrossLCDGraphComponent::~GrossLCDGraphComponent()
{
    stopTimer();
}

void GrossLCDGraphComponent::timerCallback()
{
    repaint();
}

void GrossLCDGraphComponent::paint(juce::Graphics& g)
{
    // Dark LCD Screen Background
    g.fillAll(juce::Colour::fromRGB(18, 24, 28));

    // Vertical Grid Lines (Time Slices)
    g.setColour(juce::Colour::fromRGB(35, 50, 60));
    int numColumns = 32;
    for (int i = 1; i < numColumns; ++i)
    {
        float x = static_cast<float>(getWidth() * i / numColumns);
        g.drawVerticalLine(static_cast<int>(x), 0.0f, static_cast<float>(getHeight()));
    }

    // Vertical Measure Marker Numbers (-1 to -7)
    g.setColour(juce::Colours::grey.withAlpha(0.6f));
    g.setFont(12.0f);
    for (int i = 1; i <= 7; ++i)
    {
        float y = static_cast<float>(getHeight() * i / 8);
        g.drawHorizontalLine(static_cast<int>(y), 0.0f, static_cast<float>(getWidth()));
        g.drawText("-" + juce::String(i), 5, static_cast<int>(y) - 12, 20, 12, juce::Justification::left);
    }

    // Active Lane Nodes & Envelope Curve
    bool isTime = (processor.getActiveViewLane() == SkellgateAudioProcessor::LaneTime);
    auto& nodes = isTime ? processor.getTimeCurveNodes() : processor.getVolumeCurveNodes();

    if (nodes.size() > 1)
    {
        juce::Path curvePath;
        curvePath.startNewSubPath(nodes[0].x * static_cast<float>(getWidth()), (1.0f - nodes[0].y) * static_cast<float>(getHeight()));
        for (size_t i = 1; i < nodes.size(); ++i)
        {
            curvePath.lineTo(nodes[i].x * static_cast<float>(getWidth()), (1.0f - nodes[i].y) * static_cast<float>(getHeight()));
        }

        // Neon Green / Orange Envelope Line
        juce::Colour lineCol = isTime ? juce::Colours::lime : juce::Colours::orange;
        g.setColour(lineCol);
        g.strokePath(curvePath, juce::PathStrokeType(2.0f));

        // Nodes
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            float px = nodes[i].x * static_cast<float>(getWidth());
            float py = (1.0f - nodes[i].y) * static_cast<float>(getHeight());
            g.setColour(juce::Colours::white);
            g.fillEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f);
            g.setColour(lineCol);
            g.drawEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f, 1.5f);
        }
    }

    // Active Green Playhead
    float phase = processor.getCurrentPhase();
    float cursorX = phase * static_cast<float>(getWidth());
    g.setColour(juce::Colours::lime);
    g.fillRect(cursorX - 1.0f, 0.0f, 2.0f, static_cast<float>(getHeight()));
}

void GrossLCDGraphComponent::mouseDown(const juce::MouseEvent& event)
{
    bool isTime = (processor.getActiveViewLane() == SkellgateAudioProcessor::LaneTime);
    auto& nodes = isTime ? processor.getTimeCurveNodes() : processor.getVolumeCurveNodes();
    draggedNodeIndex = -1;

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        float px = nodes[i].x * static_cast<float>(getWidth());
        float py = (1.0f - nodes[i].y) * static_cast<float>(getHeight());
        if (event.getPosition().getDistanceFrom({static_cast<int>(px), static_cast<int>(py)}) < 12)
        {
            draggedNodeIndex = static_cast<int>(i);
            break;
        }
    }
}

void GrossLCDGraphComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (draggedNodeIndex != -1)
    {
        bool isTime = (processor.getActiveViewLane() == SkellgateAudioProcessor::LaneTime);
        float normalizedY = 1.0f - (static_cast<float>(event.getPosition().y) / static_cast<float>(getHeight()));
        processor.updateNodePosition(isTime, draggedNodeIndex, normalizedY);
        repaint();
    }
}

SkellgateAudioProcessorEditor::SkellgateAudioProcessorEditor (SkellgateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), lcdGraph(p),
      snap1_16("1/16"), snap1_12("1/12"), snap1_8("1/8"), snap1_6("1/6"), snap1_4("1/4"), snap1_3("1/3")
{
    setLookAndFeel(&grossLookAndFeel);
    addAndMakeVisible(lcdGraph);

    // Setup Header Controls
    auto setupRotary = [this](juce::Slider& slider) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
    };

    setupRotary(timeMixSlider);
    setupRotary(volMixSlider);

    holdButton.setButtonText("HOLD");
    addAndMakeVisible(holdButton);

    // Setup Bottom Knobs
    setupRotary(attSlider);
    setupRotary(relSlider);
    setupRotary(tnsSlider);

    timeMixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "timeMix", timeMixSlider);
    volMixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "volMix", volMixSlider);
    attAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "att", attSlider);
    relAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "rel", relSlider);
    tnsAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "tns", tnsSlider);
    holdAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "hold", holdButton);

    // Setup 36 Time Pads (Green)
    for (int i = 0; i < 36; ++i)
    {
        auto btn = std::make_unique<juce::TextButton>();
        btn->setButtonText(SkellgateAudioProcessor::getTimePresetName(i));
        btn->addListener(this);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(22, 28, 25));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::lime);
        addAndMakeVisible(btn.get());
        timePads.push_back(std::move(btn));
    }

    // Setup 36 Volume Pads (Orange)
    for (int i = 0; i < 36; ++i)
    {
        auto btn = std::make_unique<juce::TextButton>();
        btn->setButtonText(SkellgateAudioProcessor::getVolumePresetName(i));
        btn->addListener(this);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(28, 24, 20));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::orange);
        addAndMakeVisible(btn.get());
        volPads.push_back(std::move(btn));
    }

    // Vertical Toolbar
    auto setupToolBtn = [this](juce::TextButton& btn) {
        btn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(25, 30, 35));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
        addAndMakeVisible(btn);
    };
    setupToolBtn(snap1_16); setupToolBtn(snap1_12); setupToolBtn(snap1_8);
    setupToolBtn(snap1_6);  setupToolBtn(snap1_4);  setupToolBtn(snap1_3);

    updateSlotMatrixUI();
    setSize (1000, 650);
}

SkellgateAudioProcessorEditor::~SkellgateAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void SkellgateAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    for (size_t i = 0; i < timePads.size(); ++i)
    {
        if (button == timePads[i].get())
        {
            audioProcessor.setActiveViewLane(SkellgateAudioProcessor::LaneTime);
            audioProcessor.setTimePreset(static_cast<int>(i));
            updateSlotMatrixUI();
            return;
        }
    }
    for (size_t i = 0; i < volPads.size(); ++i)
    {
        if (button == volPads[i].get())
        {
            audioProcessor.setActiveViewLane(SkellgateAudioProcessor::LaneVolume);
            audioProcessor.setVolumePreset(static_cast<int>(i));
            updateSlotMatrixUI();
            return;
        }
    }
}

void SkellgateAudioProcessorEditor::updateSlotMatrixUI()
{
    int timeIdx = audioProcessor.getCurrentTimePreset();
    for (size_t i = 0; i < timePads.size(); ++i)
    {
        if (static_cast<int>(i) == timeIdx)
            timePads[i]->setColour(juce::TextButton::buttonColourId, juce::Colours::lime.darker(0.3f));
        else
            timePads[i]->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(22, 28, 25));
    }

    int volIdx = audioProcessor.getCurrentVolumePreset();
    for (size_t i = 0; i < volPads.size(); ++i)
    {
        if (static_cast<int>(i) == volIdx)
            volPads[i]->setColour(juce::TextButton::buttonColourId, juce::Colours::orange.darker(0.3f));
        else
            volPads[i]->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(28, 24, 20));
    }
    lcdGraph.repaint();
}

void SkellgateAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Dark Chassis Background
    g.fillAll (juce::Colour::fromRGB(25, 32, 38));

    // Left Panel Chassis
    g.setColour(juce::Colour::fromRGB(32, 40, 48));
    g.fillRect(10, 10, 310, getHeight() - 20);

    // Time Label
    g.setColour(juce::Colours::lime);
    g.setFont(juce::Font(juce::FontOptions().withName("Consolas").withHeight(22.0f).withStyle("Bold")));
    g.drawText("time", 75, 20, 80, 25, juce::Justification::left);

    // Volume Label
    g.drawText("volume", 75, getHeight() - 45, 80, 25, juce::Justification::left);

    // Knob Titles (ATT, REL, TNS)
    g.setFont(10.0f);
    g.setColour(juce::Colours::lightgrey);
    g.drawText("ATT", 90, getHeight() - 75, 35, 12, juce::Justification::centred);
    g.drawText("REL", 145, getHeight() - 75, 35, 12, juce::Justification::centred);
    g.drawText("TNS", 200, getHeight() - 75, 35, 12, juce::Justification::centred);

    // Active Preset Title in Status Bar at Bottom
    g.setColour(juce::Colour::fromRGB(20, 26, 30));
    g.fillRect(360, getHeight() - 35, getWidth() - 370, 25);
    g.setColour(juce::Colours::lime);
    g.setFont(13.0f);

    bool isTime = (audioProcessor.getActiveViewLane() == SkellgateAudioProcessor::LaneTime);
    juce::String activeTitle = isTime 
        ? "Time: " + juce::String(SkellgateAudioProcessor::getTimePresetName(audioProcessor.getCurrentTimePreset()))
        : "Volume: " + juce::String(SkellgateAudioProcessor::getVolumePresetName(audioProcessor.getCurrentVolumePreset()));
    
    g.drawText(activeTitle, 370, getHeight() - 35, 300, 25, juce::Justification::left);
}

void SkellgateAudioProcessorEditor::resized()
{
    // Header Controls
    timeMixSlider.setBounds(20, 15, 45, 45);
    holdButton.setBounds(140, 55, 75, 25);

    // Dual 36-Pad Grid (9 rows x 4 cols each)
    int gridX = 20;
    int gridY = 90;
    int padW = 70;
    int padH = 22;

    for (int i = 0; i < 36; ++i)
    {
        int row = i / 4;
        int col = i % 4;
        timePads[i]->setBounds(gridX + col * padW, gridY + row * padH, padW - 2, padH - 2);
    }

    int volGridY = gridY + (9 * padH) + 10;
    for (int i = 0; i < 36; ++i)
    {
        int row = i / 4;
        int col = i % 4;
        volPads[i]->setBounds(gridX + col * padW, volGridY + row * padH, padW - 2, padH - 2);
    }

    // Bottom Knobs
    attSlider.setBounds(90, getHeight() - 110, 35, 35);
    relSlider.setBounds(145, getHeight() - 110, 35, 35);
    tnsSlider.setBounds(200, getHeight() - 110, 35, 35);
    volMixSlider.setBounds(20, getHeight() - 55, 45, 45);

    // Vertical Toolbar
    int toolX = 325;
    int toolY = 20;
    snap1_16.setBounds(toolX, toolY, 30, 20);
    snap1_12.setBounds(toolX, toolY + 22, 30, 20);
    snap1_8.setBounds(toolX, toolY + 44, 30, 20);
    snap1_6.setBounds(toolX, toolY + 66, 30, 20);
    snap1_4.setBounds(toolX, toolY + 88, 30, 20);
    snap1_3.setBounds(toolX, toolY + 110, 30, 20);

    // Right LCD Screen Graph Area
    lcdGraph.setBounds(360, 15, getWidth() - 375, getHeight() - 60);
}
