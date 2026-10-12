#include "PluginProcessor.h"
#include "PluginEditor.h"

DualGraphComponent::DualGraphComponent(SkellgateAudioProcessor& p) : processor(p)
{
    startTimerHz(60);
}

DualGraphComponent::~DualGraphComponent()
{
    stopTimer();
}

void DualGraphComponent::timerCallback()
{
    animPulse += 0.05f;
    repaint();
}

void DualGraphComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(10, 10, 10));

    float audioLevel = processor.getCurrentAudioLevel();
    auto glowAlpha = juce::jlimit(0.2f, 0.5f, audioLevel * 1.8f);
    g.setColour(juce::Colours::lime.withAlpha(glowAlpha));
    g.drawRect(getLocalBounds(), 3);

    // Live Oscilloscope Waveform Backdrop
    g.setColour(juce::Colours::lime.withAlpha(0.12f));
    juce::Path scopePath;
    scopePath.startNewSubPath(0.0f, static_cast<float>(getHeight() / 2));
    for (int x = 0; x < getWidth(); x += 4)
    {
        float waveY = (getHeight() / 2.0f) + std::sin((x * 0.05f) + animPulse * 2.0f) * (audioLevel * 40.0f);
        scopePath.lineTo(static_cast<float>(x), waveY);
    }
    g.strokePath(scopePath, juce::PathStrokeType(1.5f));

    // Neon Grid
    g.setColour(juce::Colours::darkgrey.withAlpha(0.3f));
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

        g.setColour(juce::Colours::lime.withAlpha(0.35f + 0.15f * std::sin(animPulse)));
        g.strokePath(curvePath, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(juce::Colours::lime);
        g.strokePath(curvePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

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

    float phase = processor.getCurrentPhase();
    float cursorX = phase * static_cast<float>(getWidth());
    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.fillRect(cursorX - 1.5f, 0.0f, 3.0f, static_cast<float>(getHeight()));

    g.setColour(juce::Colours::lime.withAlpha(0.6f));
    g.fillRect(cursorX - 4.0f, 0.0f, 8.0f, static_cast<float>(getHeight()));
}

void DualGraphComponent::mouseDown(const juce::MouseEvent& event)
{
    bool isTime = (processor.getActiveViewLane() == SkellgateAudioProcessor::LaneTime);
    auto& nodes = isTime ? processor.getTimeCurveNodes() : processor.getVolumeCurveNodes();
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

void DualGraphComponent::mouseDrag(const juce::MouseEvent& event)
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
    : AudioProcessorEditor (&p), audioProcessor (p), dualGraph(p)
{
    setLookAndFeel(&skellLookAndFeel);
    addAndMakeVisible(dualGraph);

    timeLaneButton.setButtonText("TIME LANE (36 Slots)");
    timeLaneButton.addListener(this);
    timeLaneButton.setColour(juce::TextButton::buttonColourId, juce::Colours::lime);
    timeLaneButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    addAndMakeVisible(timeLaneButton);

    volumeLaneButton.setButtonText("VOLUME LANE (36 Slots)");
    volumeLaneButton.addListener(this);
    volumeLaneButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 24, 24));
    volumeLaneButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
    addAndMakeVisible(volumeLaneButton);

    for (int i = 0; i < 36; ++i)
    {
        auto btn = std::make_unique<juce::TextButton>();
        btn->setButtonText("T" + juce::String(i + 1));
        btn->addListener(this);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(18, 18, 18));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        addAndMakeVisible(btn.get());
        timeSlotButtons.push_back(std::move(btn));
    }

    for (int i = 0; i < 36; ++i)
    {
        auto btn = std::make_unique<juce::TextButton>();
        btn->setButtonText("V" + juce::String(i + 1));
        btn->addListener(this);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(18, 18, 18));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        addAndMakeVisible(btn.get());
        volumeSlotButtons.push_back(std::move(btn));
    }

    updateSlotMatrixUI();

    auto setupSlider = [this](juce::Slider& slider) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 16);
        addAndMakeVisible(slider);
    };

    setupSlider(attackSlider);
    setupSlider(releaseSlider);
    setupSlider(lengthSlider);
    setupSlider(timeSlider);
    setupSlider(mixSlider);
    setupSlider(scratchSlider);
    setupSlider(pitchSlider);
    setupSlider(saturationSlider);

    attackAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "attack", attackSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "release", releaseSlider);
    lengthAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "length", lengthSlider);
    timeAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "time", timeSlider);
    mixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "mix", mixSlider);
    scratchAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "scratch", scratchSlider);
    pitchAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "pitch", pitchSlider);
    saturationAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "saturation", saturationSlider);

    bypassButton.setButtonText("BYPASS");
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "bypass", bypassButton);

    reverseButton.setButtonText("REVERSE");
    addAndMakeVisible(reverseButton);
    reverseAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "reverse", reverseButton);

    hqButton.setButtonText("HQ");
    addAndMakeVisible(hqButton);
    hqAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "hq", hqButton);

    clickReductionButton.setButtonText("NO-CLICK");
    addAndMakeVisible(clickReductionButton);
    clickReductionAttachment = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "clickReduction", clickReductionButton);

    setSize (1100, 680);
}

SkellgateAudioProcessorEditor::~SkellgateAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void SkellgateAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &timeLaneButton)
    {
        audioProcessor.setActiveViewLane(SkellgateAudioProcessor::LaneTime);
        timeLaneButton.setColour(juce::TextButton::buttonColourId, juce::Colours::lime);
        timeLaneButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        volumeLaneButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 24, 24));
        volumeLaneButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        dualGraph.repaint();
    }
    else if (button == &volumeLaneButton)
    {
        audioProcessor.setActiveViewLane(SkellgateAudioProcessor::LaneVolume);
        volumeLaneButton.setColour(juce::TextButton::buttonColourId, juce::Colours::lime);
        volumeLaneButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        timeLaneButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 24, 24));
        timeLaneButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey);
        dualGraph.repaint();
    }
    else
    {
        for (size_t i = 0; i < timeSlotButtons.size(); ++i)
        {
            if (button == timeSlotButtons[i].get())
            {
                audioProcessor.setTimePreset(static_cast<int>(i));
                updateSlotMatrixUI();
                return;
            }
        }
        for (size_t i = 0; i < volumeSlotButtons.size(); ++i)
        {
            if (button == volumeSlotButtons[i].get())
            {
                audioProcessor.setVolumePreset(static_cast<int>(i));
                updateSlotMatrixUI();
                return;
            }
        }
    }
}

void SkellgateAudioProcessorEditor::updateSlotMatrixUI()
{
    int timeIdx = audioProcessor.getCurrentTimePreset();
    for (size_t i = 0; i < timeSlotButtons.size(); ++i)
    {
        if (static_cast<int>(i) == timeIdx)
            timeSlotButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colours::lime.darker(0.2f));
        else
            timeSlotButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(18, 18, 18));
    }

    int volIdx = audioProcessor.getCurrentVolumePreset();
    for (size_t i = 0; i < volumeSlotButtons.size(); ++i)
    {
        if (static_cast<int>(i) == volIdx)
            volumeSlotButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colours::lime.darker(0.2f));
        else
            volumeSlotButtons[i]->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(18, 18, 18));
    }
    dualGraph.repaint();
}

void SkellgateAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB(14, 14, 14));

    g.setColour (juce::Colours::lime.withAlpha(0.35f));
    g.setFont (juce::Font (juce::FontOptions().withName("Consolas").withHeight(28.0f).withStyle("Bold")));
    g.drawText ("SKELLGATE", 22, 17, 220, 30, juce::Justification::left);

    g.setColour (juce::Colours::lime);
    g.drawText ("SKELLGATE", 20, 15, 220, 30, juce::Justification::left);

    g.setFont (11.0f);
    g.setColour (juce::Colours::grey);
    g.drawText ("ULTIMATE 36-SLOT DUAL-LANE ENGINE", 175, 22, 300, 20, juce::Justification::left);
}

void SkellgateAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    
    auto topArea = area.removeFromTop(40);
    clickReductionButton.setBounds(topArea.removeFromRight(95));
    hqButton.setBounds(topArea.removeFromRight(50).reduced(0, 5));
    reverseButton.setBounds(topArea.removeFromRight(95).reduced(0, 5));
    bypassButton.setBounds(topArea.removeFromRight(80));

    auto laneBar = area.removeFromTop(30);
    timeLaneButton.setBounds(laneBar.removeFromLeft(160));
    laneBar.removeFromLeft(10);
    volumeLaneButton.setBounds(laneBar.removeFromLeft(160));

    area.removeFromTop(10);

    auto mainBody = area.removeFromTop(320);
    
    auto matrixArea = mainBody.removeFromLeft(340);
    
    auto timeMatrix = matrixArea.removeFromTop(155);
    int tSlotW = timeMatrix.getWidth() / 6;
    int tSlotH = timeMatrix.getHeight() / 6;
    for (int i = 0; i < 36; ++i)
    {
        int row = i / 6;
        int col = i % 6;
        timeSlotButtons[i]->setBounds(timeMatrix.getX() + col * tSlotW + 1,
                                      timeMatrix.getY() + row * tSlotH + 1,
                                      tSlotW - 2, tSlotH - 2);
    }

    matrixArea.removeFromTop(10);

    auto volMatrix = matrixArea;
    int vSlotW = volMatrix.getWidth() / 6;
    int vSlotH = volMatrix.getHeight() / 6;
    for (int i = 0; i < 36; ++i)
    {
        int row = i / 6;
        int col = i % 6;
        volumeSlotButtons[i]->setBounds(volMatrix.getX() + col * vSlotW + 1,
                                        volMatrix.getY() + row * vSlotH + 1,
                                        vSlotW - 2, tSlotH - 2);
    }

    mainBody.removeFromLeft(15);
    dualGraph.setBounds(mainBody);

    area.removeFromTop(15);

    auto knobWidth = area.getWidth() / 8;
    attackSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    releaseSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    lengthSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    timeSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    mixSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    scratchSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    pitchSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
    saturationSlider.setBounds(area.removeFromLeft(knobWidth).reduced(5));
}
