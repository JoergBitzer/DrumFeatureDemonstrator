#include "StepGrid.h"

#include <cmath>

#include "../src/NoteNames.h"

namespace dfd
{
namespace
{
constexpr int kStepsPerRow = 16;
constexpr int kNumRows = kMaxSteps / kStepsPerRow;
constexpr int kBeatLength = 4;
constexpr float kCornerSize = 4.0f;
constexpr float kInsidePatternAlpha = 1.0f;
constexpr float kOutsidePatternAlpha = 0.3f;
constexpr float kInactiveAlpha = 0.35f;
constexpr float kOffAlpha = 0.5f;
constexpr float kHalf = 0.5f;
constexpr float kArrowHeightFactor = 0.3f;
constexpr float kArrowWidthFactor = 0.22f;
constexpr float kBarWidth = 2.0f;
constexpr float kGapFactor = 0.06f;
constexpr float kStepButtonHeightFactor = 0.42f;
constexpr float kModeButtonHeightFactor = 0.22f;
constexpr float kNumberHeight = 11.0f;
constexpr float kNumberFontSize = 10.0f;
constexpr float kBorderWidth = 2.0f;
constexpr float kDarkerAmount = 0.3f;
constexpr float kOffMix = 0.10f;   // how far an off button moves from the background towards the text colour
constexpr float kBeatMix = 0.20f;
constexpr float kModeMix = 0.14f;
const juce::Colour kInvalidColour(0xFFE05050);
const juce::Colour kCurrentColour(0xFFF2D43C); // yellow frame of the current / selected step

juce::String stepName(int stepIndex)
{
    return juce::String(stepIndex + 1);
}
} // namespace

// ---------------------------------------------------------------------------------------------- ModeButton
ModeButton::ModeButton(juce::RangedAudioParameter& parameter)
    : m_attachment(parameter,
                   [this](float value)
                   {
                       m_value = value;
                       repaint();
                   })
{
    m_attachment.sendInitialUpdate();
    setTooltip("Mode of the step: forward, reverse, or reverse and play to end (the sample ends exactly at the end of this step). Click to change.");
}

float ModeButton::currentMode() const
{
    return m_value;
}

int ModeButton::getMode() const
{
    return static_cast<int>(std::lround(m_value));
}

void ModeButton::setStepActive(bool isActive)
{
    m_stepActive = isActive;
    setEnabled(isActive);
    repaint();
}

void ModeButton::setInvalid(bool isInvalid)
{
    if (m_invalid != isInvalid)
    {
        m_invalid = isInvalid;
        if (isInvalid)
        {
            setTooltip("The sample is longer than the pattern: it cannot play to end. Shorten the sample (end handle), raise the pitch or lengthen the pattern.");
        }
        repaint();
    }
}

void ModeButton::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    if (m_stepActive == false)
    {
        return;
    }
    const int numModes = static_cast<int>(kModeReverseToEnd) + 1;
    const int currentIndex = static_cast<int>(std::lround(currentMode()));
    const int nextIndex = (currentIndex + 1) % numModes;
    m_attachment.setValueAsCompleteGesture(static_cast<float>(nextIndex));
}

void ModeButton::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    const juce::Colour textColour = getLookAndFeel().findColour(juce::Label::textColourId);
    float alpha = 1.0f;
    if (m_stepActive == false)
    {
        alpha = kInactiveAlpha;
    }
    g.setColour(background.interpolatedWith(textColour, kModeMix).withAlpha(alpha));
    g.fillRoundedRectangle(bounds, kCornerSize);

    const int mode = static_cast<int>(std::lround(currentMode()));
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float arrowHeight = bounds.getHeight() * (kHalf + kArrowHeightFactor);
    const float arrowWidth = bounds.getWidth() * kArrowWidthFactor * 2.0f;
    juce::Colour arrowColour = textColour.withAlpha(alpha);
    if (mode == static_cast<int>(kModeReverseToEnd))
    {
        arrowColour = kCurrentColour.withAlpha(alpha);
    }
    if (m_invalid)
    {
        arrowColour = kInvalidColour;
    }
    g.setColour(arrowColour);
    juce::Path arrow;
    if (mode == static_cast<int>(kModeForward))
    {
        arrow.addTriangle(centreX - arrowWidth * kHalf, centreY - arrowHeight * kHalf, centreX - arrowWidth * kHalf,
                          centreY + arrowHeight * kHalf, centreX + arrowWidth * kHalf, centreY);
    }
    else
    {
        float shift = 0.0f;
        if (mode == static_cast<int>(kModeReverseToEnd))
        {
            shift = arrowWidth * kHalf;
        }
        arrow.addTriangle(centreX + arrowWidth * kHalf - shift, centreY - arrowHeight * kHalf,
                          centreX + arrowWidth * kHalf - shift, centreY + arrowHeight * kHalf,
                          centreX - arrowWidth * kHalf - shift, centreY);
    }
    g.fillPath(arrow);
    if (mode == static_cast<int>(kModeReverseToEnd))
    {
        const float barX = centreX + arrowWidth * kHalf + kBarWidth;
        g.fillRect(barX, centreY - arrowHeight * kHalf, kBarWidth, arrowHeight);
    }
    if (m_invalid)
    {
        g.drawRoundedRectangle(bounds, kCornerSize, 1.0f);
    }
}

// ---------------------------------------------------------------------------------------------- StepButton
void StepButton::setCurrent(bool isCurrentStep)
{
    if (m_current != isCurrentStep)
    {
        m_current = isCurrentStep;
        repaint();
    }
}

void StepButton::setBeatAccent(bool isOnBeat)
{
    m_beatAccent = isOnBeat;
}

void StepButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    const juce::Colour onColour = getLookAndFeel().findColour(juce::Slider::rotarySliderFillColourId);
    const juce::Colour textColour = getLookAndFeel().findColour(juce::Label::textColourId);
    juce::Colour fill = background.interpolatedWith(textColour, kOffMix);
    if (m_beatAccent)
    {
        fill = background.interpolatedWith(textColour, kBeatMix);
    }
    if (getToggleState())
    {
        fill = onColour;
    }
    if (shouldDrawButtonAsHighlighted)
    {
        fill = fill.brighter(kDarkerAmount * kHalf);
    }
    if (shouldDrawButtonAsDown)
    {
        fill = fill.darker(kDarkerAmount);
    }
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, kCornerSize);
    if (m_current)
    {
        g.setColour(kCurrentColour);
        g.drawRoundedRectangle(bounds, kCornerSize, kBorderWidth);
    }
}

// ---------------------------------------------------------------------------------------------- StepColumn
StepColumn::StepColumn(int stepIndex, juce::AudioProcessorValueTreeState& apvts)
    : m_stepIndex(stepIndex),
      m_modeButton(*apvts.getParameter(stepParameterId(stepIndex, StepParam::Mode))),
      m_stepAttachment(apvts, stepParameterId(stepIndex, StepParam::Active), m_stepButton),
      m_noteAttachment(apvts, stepParameterId(stepIndex, StepParam::Note), m_noteSlider),
      m_velocityAttachment(apvts, stepParameterId(stepIndex, StepParam::Velocity), m_velocitySlider)
{
    m_stepButton.setClickingTogglesState(true);
    m_stepButton.setBeatAccent((stepIndex % kBeatLength) == 0);
    m_stepButton.onClick = [this]() { updateModeButton(); };
    m_stepButton.setTooltip(stepParameterSpec(stepIndex, StepParam::Active).help);
    addAndMakeVisible(m_stepButton);
    addAndMakeVisible(m_modeButton);

    for (juce::Slider* slider : {&m_noteSlider, &m_velocitySlider})
    {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
    }
    m_noteSlider.setTooltip(stepParameterSpec(stepIndex, StepParam::Note).help);
    m_velocitySlider.setTooltip(stepParameterSpec(stepIndex, StepParam::Velocity).help);
    updateModeButton();

    // mouse events of all children come here: a click selects the step, hovering and changing show the value in the info line
    addMouseListener(this, true);
    m_noteSlider.onValueChange = [this]() { reportControl(&m_noteSlider); };
    m_velocitySlider.onValueChange = [this]() { reportControl(&m_velocitySlider); };
}

juce::String StepColumn::describeControl(const juce::Component* control) const
{
    const juce::String stepText = "Step " + juce::String(m_stepIndex + 1);
    if (control == &m_noteSlider)
    {
        const int note = static_cast<int>(std::lround(m_noteSlider.getValue()));
        return stepText + " note: " + juce::String(noteName(note)) + " (MIDI note " + juce::String(note) + ")";
    }
    if (control == &m_velocitySlider)
    {
        return stepText + " velocity: " + juce::String(static_cast<int>(std::lround(m_velocitySlider.getValue())));
    }
    if (control == &m_modeButton)
    {
        const juce::StringArray modeNames{"forward", "reverse", "reverse and play to end (ends at the end of the step)"};
        return stepText + " mode: " + modeNames[m_modeButton.getMode()];
    }
    if (control == &m_stepButton)
    {
        if (m_stepButton.getToggleState())
        {
            return stepText + ": on";
        }
        return stepText + ": off";
    }
    return juce::String();
}

void StepColumn::reportControl(const juce::Component* control)
{
    if (onInfo)
    {
        onInfo(describeControl(control));
    }
}

void StepColumn::mouseDown(const juce::MouseEvent& event)
{
    if (onSelected)
    {
        onSelected(m_stepIndex);
    }
    // a click on the mode button or the step button changes a value: show it after the change was made
    juce::Component* clicked = event.eventComponent;
    juce::Component::SafePointer<StepColumn> safeThis(this);
    juce::MessageManager::callAsync([safeThis, clicked]()
    {
        if (safeThis != nullptr && clicked != nullptr)
        {
            safeThis->reportControl(clicked);
        }
    });
}

void StepColumn::mouseEnter(const juce::MouseEvent& event)
{
    reportControl(event.eventComponent);
}

void StepColumn::updateModeButton()
{
    m_modeButton.setStepActive(m_stepButton.getToggleState());
}

void StepColumn::setCurrent(bool isCurrentStep)
{
    m_stepButton.setCurrent(isCurrentStep);
}

void StepColumn::setInvalid(bool isInvalid)
{
    m_modeButton.setInvalid(isInvalid);
}

void StepColumn::setInsidePattern(bool isInside)
{
    float alpha = kOutsidePatternAlpha;
    if (isInside)
    {
        alpha = kInsidePatternAlpha;
    }
    setAlpha(alpha);
}

void StepColumn::paint(juce::Graphics& g)
{
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId).withAlpha(kOffAlpha));
    g.setFont(kNumberFontSize);
    g.drawText(stepName(m_stepIndex), 0, 0, getWidth(), static_cast<int>(kNumberHeight), juce::Justification::centred);
}

void StepColumn::resized()
{
    auto area = getLocalBounds();
    const int width = area.getWidth();
    const int height = area.getHeight();
    const int gap = static_cast<int>(static_cast<float>(width) * kGapFactor);
    area.removeFromTop(static_cast<int>(kNumberHeight));
    const int stepHeight = static_cast<int>(static_cast<float>(height) * kStepButtonHeightFactor);
    const int modeHeight = static_cast<int>(static_cast<float>(height) * kModeButtonHeightFactor);
    m_stepButton.setBounds(area.removeFromTop(stepHeight).reduced(gap, 0));
    area.removeFromTop(gap);
    m_modeButton.setBounds(area.removeFromTop(modeHeight).reduced(gap, 0));
    area.removeFromTop(gap);
    const int knobWidth = area.getWidth() / 2;
    m_noteSlider.setBounds(area.removeFromLeft(knobWidth));
    m_velocitySlider.setBounds(area);
}

// ---------------------------------------------------------------------------------------------- StepGrid
StepGrid::StepGrid(juce::AudioProcessorValueTreeState& apvts)
{
    for (int stepIndex = 0; stepIndex < kMaxSteps; ++stepIndex)
    {
        m_columns[static_cast<std::size_t>(stepIndex)] = std::make_unique<StepColumn>(stepIndex, apvts);
        StepColumn& column = *m_columns[static_cast<std::size_t>(stepIndex)];
        column.onSelected = [this](int selectedIndex) { setSelectedStep(selectedIndex); };
        column.onInfo = [this](const juce::String& text)
        {
            if (onInfo)
            {
                onInfo(text);
            }
        };
        addAndMakeVisible(column);
    }
}

int StepGrid::getSelectedStep() const
{
    return m_selectedStep;
}

void StepGrid::setSelectedStep(int stepIndex)
{
    m_selectedStep = juce::jlimit(0, kMaxSteps - 1, stepIndex);
}

void StepGrid::moveSelectedStep(int numSteps, int patternLength)
{
    const int length = juce::jlimit(kMinSteps, kMaxSteps, patternLength);
    int target = m_selectedStep + numSteps;
    if (target < 0)
    {
        target += length;
    }
    if (target >= length)
    {
        target -= length;
    }
    setSelectedStep(juce::jlimit(0, length - 1, target));
}

void StepGrid::update(int currentStep, juce::uint32 invalidStepMask, int patternLength)
{
    if (m_selectedStep >= patternLength)
    {
        m_selectedStep = juce::jmax(0, patternLength - 1);
    }
    int framedStep = m_selectedStep; // the transport stands: frame the selected step
    if (currentStep >= 0)
    {
        framedStep = currentStep;    // it plays: frame the step that sounds
    }
    for (int stepIndex = 0; stepIndex < kMaxSteps; ++stepIndex)
    {
        StepColumn& column = *m_columns[static_cast<std::size_t>(stepIndex)];
        column.setCurrent(stepIndex == framedStep);
        const juce::uint32 bit = juce::uint32(1) << stepIndex;
        column.setInvalid((invalidStepMask & bit) != 0);
        column.setInsidePattern(stepIndex < patternLength);
    }
}

void StepGrid::resized()
{
    const auto area = getLocalBounds();
    const int columnWidth = area.getWidth() / kStepsPerRow;
    const int rowHeight = area.getHeight() / kNumRows;
    for (int stepIndex = 0; stepIndex < kMaxSteps; ++stepIndex)
    {
        const int row = stepIndex / kStepsPerRow;
        const int column = stepIndex % kStepsPerRow;
        m_columns[static_cast<std::size_t>(stepIndex)]->setBounds(column * columnWidth, row * rowHeight, columnWidth, rowHeight);
    }
}
} // namespace dfd
