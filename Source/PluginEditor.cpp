#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
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
    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());

    // Dark LCD Screen Background
    g.fillAll(juce::Colour::fromRGB(18, 24, 28));

    // Vertical grid: 32 columns = 1/16 of a beat; beat lines are brighter
    for (int i = 1; i < 32; ++i)
    {
        const bool isBeat = (i % 16 == 0);
        const bool isEighth = (i % 4 == 0);
        g.setColour(juce::Colour::fromRGB(35, 50, 60).withMultipliedBrightness(isBeat ? 2.2f : (isEighth ? 1.4f : 1.0f)));
        g.drawVerticalLine(static_cast<int>(w * (float) i / 32.0f), 0.0f, h);
    }

    // Snap grid overlay
    if (snapX > 0.0f)
    {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        for (float x = snapX; x < 0.999f; x += snapX)
            g.drawVerticalLine(static_cast<int>(w * x), 0.0f, h);
    }

    // Horizontal marker lines (-1 to -7)
    g.setColour(juce::Colours::grey.withAlpha(0.6f));
    g.setFont(12.0f);
    for (int i = 1; i <= 7; ++i)
    {
        const float y = h * (float) i / 8.0f;
        g.drawHorizontalLine(static_cast<int>(y), 0.0f, w);
        g.drawText("-" + juce::String(i), 5, static_cast<int>(y) - 12, 20, 12, juce::Justification::left);
    }

    // Active lane curve
    const bool isTime = (processor.getActiveViewLane() == SkellgateAudioProcessor::LaneTime);
    const auto curve = processor.getCurve(isTime);
    const juce::Colour lineCol = isTime ? juce::Colours::lime : juce::Colours::orange;

    if (curve.count > 1)
    {
        // the TNS knob bends each segment, so draw what the audio engine actually follows
        const float tension = processor.getTensionExponent();
        const bool bent = std::abs(tension - 1.0f) > 1.0e-3f;

        juce::Path curvePath;
        curvePath.startNewSubPath(curve.nodes[0].x * w, (1.0f - curve.nodes[0].y) * h);
        for (int i = 1; i < curve.count; ++i)
        {
            const auto& a = curve.nodes[(size_t) i - 1];
            const auto& b = curve.nodes[(size_t) i];
            const float segW = (b.x - a.x) * w;

            if (bent && segW > 6.0f)
            {
                for (int s = 1; s < 16; ++s)
                {
                    const float f = (float) s / 16.0f;
                    curvePath.lineTo((a.x + f * (b.x - a.x)) * w,
                                     (1.0f - (a.y + std::pow(f, tension) * (b.y - a.y))) * h);
                }
            }
            curvePath.lineTo(b.x * w, (1.0f - b.y) * h);
        }

        // fill under the curve
        juce::Path fillPath(curvePath);
        fillPath.lineTo(curve.nodes[(size_t) curve.count - 1].x * w, h);
        fillPath.lineTo(curve.nodes[0].x * w, h);
        fillPath.closeSubPath();
        g.setColour(lineCol.withAlpha(0.10f));
        g.fillPath(fillPath);

        g.setColour(lineCol);
        g.strokePath(curvePath, juce::PathStrokeType(2.0f));

        for (int i = 0; i < curve.count; ++i)
        {
            const float px = curve.nodes[(size_t) i].x * w;
            const float py = (1.0f - curve.nodes[(size_t) i].y) * h;
            g.setColour(juce::Colours::white);
            g.fillEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f);
            g.setColour(lineCol);
            g.drawEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f, 1.5f);
        }
    }

    // Playhead
    const float cursorX = processor.getCurrentPhase() * w;
    g.setColour(juce::Colours::lime);
    g.fillRect(cursorX - 1.0f, 0.0f, 2.0f, h);
}

int GrossLCDGraphComponent::findNodeAt(const SkellgateAudioProcessor::Curve& curve, juce::Point<int> pos) const
{
    const float w = static_cast<float>(getWidth());
    const float h = static_cast<float>(getHeight());

    int best = -1;
    float bestDist = 12.0f;
    for (int i = 0; i < curve.count; ++i)
    {
        const float px = curve.nodes[(size_t) i].x * w;
        const float py = (1.0f - curve.nodes[(size_t) i].y) * h;
        const float d = pos.toFloat().getDistanceFrom({ px, py });
        if (d < bestDist)
        {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

float GrossLCDGraphComponent::snapPhase(float x, const juce::MouseEvent& e) const
{
    x = juce::jlimit(0.0f, 1.0f, x);
    if (snapX > 0.0f && ! e.mods.isShiftDown())
        x = juce::jlimit(0.0f, 1.0f, std::round(x / snapX) * snapX);
    return x;
}

void GrossLCDGraphComponent::mouseDown(const juce::MouseEvent& event)
{
    const bool isTime = (processor.getActiveViewLane() == SkellgateAudioProcessor::LaneTime);
    const auto curve = processor.getCurve(isTime);
    draggedNodeIndex = findNodeAt(curve, event.getPosition());

    // right-click deletes a node
    if (event.mods.isPopupMenu() && draggedNodeIndex >= 0)
    {
        processor.removeNode(isTime, draggedNodeIndex);
        draggedNodeIndex = -1;
        repaint();
    }
}

void GrossLCDGraphComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (draggedNodeIndex < 0)
        return;

    const bool isTime = (processor.getActiveViewLane() == SkellgateAudioProcessor::LaneTime);
    const float nx = snapPhase(static_cast<float>(event.getPosition().x) / static_cast<float>(getWidth()), event);
    const float ny = 1.0f - (static_cast<float>(event.getPosition().y) / static_cast<float>(getHeight()));
    processor.moveNode(isTime, draggedNodeIndex, nx, ny);
    repaint();
}

void GrossLCDGraphComponent::mouseUp(const juce::MouseEvent&)
{
    draggedNodeIndex = -1;
}

void GrossLCDGraphComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    const bool isTime = (processor.getActiveViewLane() == SkellgateAudioProcessor::LaneTime);
    const auto curve = processor.getCurve(isTime);
    const int hit = findNodeAt(curve, event.getPosition());

    if (hit >= 0)
    {
        processor.removeNode(isTime, hit);
    }
    else
    {
        const float nx = snapPhase(static_cast<float>(event.getPosition().x) / static_cast<float>(getWidth()), event);
        const float ny = 1.0f - (static_cast<float>(event.getPosition().y) / static_cast<float>(getHeight()));
        processor.addNode(isTime, nx, ny);
    }
    draggedNodeIndex = -1;
    repaint();
}

//==============================================================================
SkellgateAudioProcessorEditor::SkellgateAudioProcessorEditor (SkellgateAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), lcdGraph(p),
      snap1_16("1/16"), snap1_12("1/12"), snap1_8("1/8"), snap1_6("1/6"), snap1_4("1/4"), snap1_3("1/3"),
      viewTimeBtn("T"), viewVolBtn("V")
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
    setupRotary(offsetSlider);

    holdButton.setButtonText("HOLD");
    holdButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    holdButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::lime);
    addAndMakeVisible(holdButton);

    // Setup Bottom Knobs
    setupRotary(attSlider);
    setupRotary(relSlider);
    setupRotary(tnsSlider);

    timeMixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "timeMix", timeMixSlider);
    volMixAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "volMix", volMixSlider);
    offsetAttachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, "scratch", offsetSlider);
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
        btn.addListener(this);
        addAndMakeVisible(btn);
    };
    for (auto* b : snapButtons)
        setupToolBtn(*b);
    setupToolBtn(viewTimeBtn);
    setupToolBtn(viewVolBtn);

    updateSlotMatrixUI();
    setResizeLimits (860, 650, 1600, 1000);
    setResizable (true, true);
    setSize (1000, 650);
    startTimerHz(20);
}

SkellgateAudioProcessorEditor::~SkellgateAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SkellgateAudioProcessorEditor::setSnapIndex(int newIndex)
{
    // clicking the active snap button again turns snapping off
    snapIndex = (newIndex == snapIndex) ? -1 : newIndex;

    // button labels are fractions of a beat, the pattern is 2 beats long
    static const float kBeatDivisions[6] = { 16.0f, 12.0f, 8.0f, 6.0f, 4.0f, 3.0f };
    lcdGraph.setSnap(snapIndex < 0 ? 0.0f : 1.0f / (kBeatDivisions[snapIndex] * (float) SkellgateAudioProcessor::kPatternBeats));

    for (int i = 0; i < 6; ++i)
    {
        const bool on = (i == snapIndex);
        snapButtons[i]->setColour(juce::TextButton::buttonColourId, on ? juce::Colour::fromRGB(60, 80, 95) : juce::Colour::fromRGB(25, 30, 35));
        snapButtons[i]->setColour(juce::TextButton::textColourOffId, on ? juce::Colours::white : juce::Colours::grey);
    }
}

void SkellgateAudioProcessorEditor::ensureSoftwareRenderer()
{
   #if JUCE_WINDOWS
    if (auto* peer = getPeer())
    {
        if (peer == lastPeer)
            return;

        lastPeer = peer;
        const int software = peer->getAvailableRenderingEngines().indexOf("Software Renderer");
        if (software >= 0 && peer->getCurrentRenderingEngine() != software)
            peer->setCurrentRenderingEngine(software);
    }
   #else
    juce::ignoreUnused(lastPeer);
   #endif
}

void SkellgateAudioProcessorEditor::timerCallback()
{
    ensureSoftwareRenderer();   // safety net in case the window was created after the hooks above

    const int t = audioProcessor.getCurrentTimePreset();
    const int v = audioProcessor.getCurrentVolumePreset();
    const int lane = static_cast<int>(audioProcessor.getActiveViewLane());

    // follows presets that were switched by MIDI notes
    if (t != shownTimePreset || v != shownVolPreset || lane != shownLane)
        updateSlotMatrixUI();

    const double bpm = audioProcessor.getCurrentBpm();
    if (std::abs(bpm - shownBpm) > 0.05)
    {
        shownBpm = bpm;
        repaint(520, getHeight() - 35, 110, 25);
    }
}

void SkellgateAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    for (size_t i = 0; i < timePads.size(); ++i)
    {
        if (button == timePads[i].get())
        {
            audioProcessor.setTimePreset(static_cast<int>(i));   // also switches the view to the time lane
            updateSlotMatrixUI();
            return;
        }
    }
    for (size_t i = 0; i < volPads.size(); ++i)
    {
        if (button == volPads[i].get())
        {
            audioProcessor.setVolumePreset(static_cast<int>(i));
            updateSlotMatrixUI();
            return;
        }
    }
    for (int i = 0; i < 6; ++i)
    {
        if (button == snapButtons[i])
        {
            setSnapIndex(i);
            return;
        }
    }
    if (button == &viewTimeBtn)
    {
        audioProcessor.setActiveViewLane(SkellgateAudioProcessor::LaneTime);
        updateSlotMatrixUI();
    }
    else if (button == &viewVolBtn)
    {
        audioProcessor.setActiveViewLane(SkellgateAudioProcessor::LaneVolume);
        updateSlotMatrixUI();
    }
}

void SkellgateAudioProcessorEditor::updateSlotMatrixUI()
{
    const int timeIdx = audioProcessor.getCurrentTimePreset();
    for (size_t i = 0; i < timePads.size(); ++i)
    {
        if (static_cast<int>(i) == timeIdx)
            timePads[i]->setColour(juce::TextButton::buttonColourId, juce::Colours::lime.darker(0.3f));
        else
            timePads[i]->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(22, 28, 25));
    }

    const int volIdx = audioProcessor.getCurrentVolumePreset();
    for (size_t i = 0; i < volPads.size(); ++i)
    {
        if (static_cast<int>(i) == volIdx)
            volPads[i]->setColour(juce::TextButton::buttonColourId, juce::Colours::orange.darker(0.3f));
        else
            volPads[i]->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(28, 24, 20));
    }

    const bool isTime = (audioProcessor.getActiveViewLane() == SkellgateAudioProcessor::LaneTime);
    viewTimeBtn.setColour(juce::TextButton::buttonColourId, isTime ? juce::Colours::lime.darker(0.5f) : juce::Colour::fromRGB(25, 30, 35));
    viewVolBtn.setColour(juce::TextButton::buttonColourId, ! isTime ? juce::Colours::orange.darker(0.5f) : juce::Colour::fromRGB(25, 30, 35));

    shownTimePreset = timeIdx;
    shownVolPreset = volIdx;
    shownLane = static_cast<int>(audioProcessor.getActiveViewLane());

    lcdGraph.repaint();
    repaint();   // status bar text
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
    g.setColour(juce::Colours::orange);
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

    const bool isTime = (audioProcessor.getActiveViewLane() == SkellgateAudioProcessor::LaneTime);
    g.setColour(isTime ? juce::Colours::lime : juce::Colours::orange);
    g.setFont(13.0f);

    const juce::String activeTitle = isTime
        ? "Time: " + juce::String(SkellgateAudioProcessor::getTimePresetName(audioProcessor.getCurrentTimePreset()))
        : "Volume: " + juce::String(SkellgateAudioProcessor::getVolumePresetName(audioProcessor.getCurrentVolumePreset()));

    g.drawText(activeTitle, 370, getHeight() - 35, 300, 25, juce::Justification::left);

    g.setColour(juce::Colours::lightgrey);
    g.drawText(juce::String(audioProcessor.getCurrentBpm(), 1) + " BPM", 520, getHeight() - 35, 100, 25, juce::Justification::centredRight);

    g.setColour(juce::Colours::grey);
    g.setFont(11.0f);
    g.drawText("double-click: add / remove node    shift: no snap", 640, getHeight() - 35, getWidth() - 660, 25, juce::Justification::right);

    // OFFSET knob caption
    g.setColour(juce::Colours::lightgrey);
    g.drawText("OFFSET", 247, 62, 60, 12, juce::Justification::centred);
}

void SkellgateAudioProcessorEditor::resized()
{
    // Header Controls
    timeMixSlider.setBounds(20, 15, 45, 45);
    offsetSlider.setBounds(255, 15, 45, 45);
    holdButton.setBounds(140, 55, 75, 25);

    // Dual 36-Pad Grid (9 rows x 4 cols each)
    const int gridX = 20;
    const int gridY = 90;
    const int padW = 70;
    const int padH = 22;

    for (int i = 0; i < 36; ++i)
    {
        const int row = i / 4;
        const int col = i % 4;
        timePads[(size_t) i]->setBounds(gridX + col * padW, gridY + row * padH, padW - 2, padH - 2);
    }

    const int volGridY = gridY + (9 * padH) + 10;
    for (int i = 0; i < 36; ++i)
    {
        const int row = i / 4;
        const int col = i % 4;
        volPads[(size_t) i]->setBounds(gridX + col * padW, volGridY + row * padH, padW - 2, padH - 2);
    }

    // Bottom Knobs
    attSlider.setBounds(90, getHeight() - 110, 35, 35);
    relSlider.setBounds(145, getHeight() - 110, 35, 35);
    tnsSlider.setBounds(200, getHeight() - 110, 35, 35);
    volMixSlider.setBounds(20, getHeight() - 55, 45, 45);

    // Vertical Toolbar
    const int toolX = 325;
    const int toolY = 20;
    for (int i = 0; i < 6; ++i)
        snapButtons[i]->setBounds(toolX, toolY + i * 22, 30, 20);
    viewTimeBtn.setBounds(toolX, toolY + 150, 30, 20);
    viewVolBtn.setBounds(toolX, toolY + 172, 30, 20);

    // Right LCD Screen Graph Area
    lcdGraph.setBounds(360, 15, getWidth() - 375, getHeight() - 60);
}
