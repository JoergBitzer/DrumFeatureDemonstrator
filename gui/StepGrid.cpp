#include "StepGrid.h"

#include <algorithm>
#include <cmath>

#include "../src/NoteNames.h"

namespace dfd
{
namespace
{
constexpr int kBeatLength = 4;
constexpr float kCornerSize = 4.0f;
constexpr float kInsidePatternAlpha = 1.0f;
constexpr float kOutsidePatternAlpha = 0.3f;
constexpr float kOffAlpha = 0.5f;
constexpr float kHalf = 0.5f;
constexpr float kArrowHeightFactor = 0.55f;
constexpr float kArrowWidthFactor = 0.42f;
constexpr float kBarWidth = 2.0f;
constexpr float kGapFactor = 0.06f;
constexpr float kNumberHeight = 11.0f;
constexpr float kNumberFontSize = 10.0f;
constexpr float kBorderWidth = 2.0f;
constexpr float kDarkerAmount = 0.3f;
constexpr float kBrighterAmount = 0.18f;
constexpr float kOffMix = 0.10f;   // how far an off button moves from the background towards the text colour
constexpr float kBeatMix = 0.20f;
constexpr float kStripMix = 0.07f;
constexpr float kStripBeatMix = 0.14f;
constexpr float kStripBarAlpha = 0.9f;
constexpr float kStripBorderAlpha = 0.5f;
constexpr float kStripBarGap = 1.0f;
constexpr float kStepsAreaFraction = 0.50f;   // of the height of a row: numbers and buttons; the rest is the strip
constexpr float kRowGapFraction = 0.05f;
constexpr float kCentrePanTolerance = 0.005f;
constexpr float kPercent = 100.0f;
const juce::Colour kInvalidColour(0xFFE05050);
const juce::Colour kCurrentColour(0xFFF2D43C); // yellow frame of the current / selected step

juce::String stepName(int stepIndex)
{
    return juce::String(stepIndex + 1);
}

juce::String describeValue(int patternIndex, int stepIndex, GridPage page, float value)
{
    const juce::String stepText = "Pattern " + juce::String(patternIndex + 1) + ", step " + juce::String(stepIndex + 1);
    if (page == GridPage::Note)
    {
        const int note = static_cast<int>(std::lround(value));
        return stepText + " note: " + juce::String(noteName(note)) + " (MIDI note " + juce::String(note) + ")";
    }
    if (page == GridPage::Velocity)
    {
        return stepText + " velocity: " + juce::String(static_cast<int>(std::lround(value)));
    }
    if (std::abs(value) < kCentrePanTolerance)
    {
        return stepText + " pan: centre";
    }
    juce::String side = "right ";
    if (value < 0.0f)
    {
        side = "left ";
    }
    return stepText + " pan: " + side + juce::String(static_cast<int>(std::lround(std::abs(value) * kPercent))) + " %";
}
} // namespace

// ---------------------------------------------------------------------------------------------- StepStateButton
StepStateButton::StepStateButton()
{
    setTooltip("Click: off, forward, reverse, reverse + play to end (the sample then ends exactly at the end of this step), off, ...");
}

juce::String StepStateButton::stateName(int state)
{
    const juce::StringArray names{"off", "forward", "reverse", "reverse and play to end (ends at the end of the step)"};
    return names[juce::jlimit(0, kNumStates - 1, state)];
}

void StepStateButton::bind(juce::AudioProcessorValueTreeState& apvts, int patternIndex, int stepIndex)
{
    m_activeAttachment.reset();
    m_modeAttachment.reset();
    m_activeAttachment = std::make_unique<juce::ParameterAttachment>(
        *apvts.getParameter(stepParameterId(patternIndex, stepIndex, StepParam::Active)),
        [this](float value)
        {
            m_active = value;
            repaint();
        });
    m_modeAttachment = std::make_unique<juce::ParameterAttachment>(
        *apvts.getParameter(stepParameterId(patternIndex, stepIndex, StepParam::Mode)),
        [this](float value)
        {
            m_mode = value;
            repaint();
        });
    m_activeAttachment->sendInitialUpdate();
    m_modeAttachment->sendInitialUpdate();
}

int StepStateButton::getState() const
{
    if (m_active < kHalf)
    {
        return kStateOff;
    }
    return kStateForward + juce::jlimit(0, 2, static_cast<int>(std::lround(m_mode)));
}

void StepStateButton::setCurrent(bool isCurrentStep)
{
    if (m_current != isCurrentStep)
    {
        m_current = isCurrentStep;
        repaint();
    }
}

void StepStateButton::setBeatAccent(bool isOnBeat)
{
    m_beatAccent = isOnBeat;
}

void StepStateButton::setInvalid(bool isInvalid)
{
    if (m_invalid != isInvalid)
    {
        m_invalid = isInvalid;
        repaint();
    }
}

void StepStateButton::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    if (m_activeAttachment == nullptr || m_modeAttachment == nullptr)
    {
        return;
    }
    const int newState = (getState() + 1) % kNumStates;
    if (newState == kStateOff)
    {
        m_activeAttachment->setValueAsCompleteGesture(0.0f);
    }
    else
    {
        m_modeAttachment->setValueAsCompleteGesture(static_cast<float>(newState - kStateForward));
        m_activeAttachment->setValueAsCompleteGesture(1.0f);
    }
    if (onStateChangedByUser)
    {
        onStateChangedByUser();
    }
}

void StepStateButton::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    const juce::Colour textColour = getLookAndFeel().findColour(juce::Label::textColourId);
    const juce::Colour onColour = getLookAndFeel().findColour(juce::Slider::rotarySliderFillColourId);
    const int state = getState();

    juce::Colour fill = background.interpolatedWith(textColour, kOffMix);
    if (m_beatAccent)
    {
        fill = background.interpolatedWith(textColour, kBeatMix);
    }
    if (state != kStateOff)
    {
        fill = onColour;
    }
    if (isMouseOver())
    {
        fill = fill.brighter(kBrighterAmount);
    }
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, kCornerSize);

    if (state != kStateOff)
    {
        // the icon of the state: forward arrow, reverse arrow, reverse arrow with a bar (play to end)
        const float centreX = bounds.getCentreX();
        const float centreY = bounds.getCentreY();
        const float arrowHeight = bounds.getHeight() * kArrowHeightFactor;
        const float arrowWidth = bounds.getWidth() * kArrowWidthFactor;
        juce::Colour iconColour = onColour.contrasting(1.0f);
        if (state == kStateReverseToEnd)
        {
            iconColour = kCurrentColour;
        }
        if (m_invalid)
        {
            iconColour = kInvalidColour;
        }
        g.setColour(iconColour);
        juce::Path arrow;
        if (state == kStateForward)
        {
            arrow.addTriangle(centreX - arrowWidth * kHalf, centreY - arrowHeight * kHalf, centreX - arrowWidth * kHalf,
                              centreY + arrowHeight * kHalf, centreX + arrowWidth * kHalf, centreY);
        }
        else
        {
            float shift = 0.0f;
            if (state == kStateReverseToEnd)
            {
                shift = arrowWidth * kHalf;
            }
            arrow.addTriangle(centreX + arrowWidth * kHalf - shift, centreY - arrowHeight * kHalf,
                              centreX + arrowWidth * kHalf - shift, centreY + arrowHeight * kHalf,
                              centreX - arrowWidth * kHalf - shift, centreY);
        }
        g.fillPath(arrow);
        if (state == kStateReverseToEnd)
        {
            const float barX = centreX + arrowWidth * kHalf + kBarWidth;
            g.fillRect(barX, centreY - arrowHeight * kHalf, kBarWidth, arrowHeight);
        }
    }
    if (m_invalid)
    {
        g.setColour(kInvalidColour);
        g.drawRoundedRectangle(bounds, kCornerSize, 1.0f);
    }
    if (m_current)
    {
        g.setColour(kCurrentColour);
        g.drawRoundedRectangle(bounds, kCornerSize, kBorderWidth);
    }
}

// ---------------------------------------------------------------------------------------------- StepColumn
StepColumn::StepColumn(int stepIndex, juce::AudioProcessorValueTreeState& apvts) : m_stepIndex(stepIndex), m_apvts(apvts)
{
    m_stateButton.setBeatAccent((stepIndex % kBeatLength) == 0);
    m_stateButton.onStateChangedByUser = [this]() { reportState(); };
    addAndMakeVisible(m_stateButton);
    bind(0);
    // a click anywhere in the column selects the step
    addMouseListener(this, true);
}

void StepColumn::bind(int patternIndex)
{
    m_patternIndex = patternIndex;
    m_stateButton.bind(m_apvts, patternIndex, m_stepIndex);
    repaint();
}

void StepColumn::reportState()
{
    if (onInfo)
    {
        onInfo("Pattern " + juce::String(m_patternIndex + 1) + ", step " + juce::String(m_stepIndex + 1) + ": "
               + StepStateButton::stateName(m_stateButton.getState()));
    }
}

void StepColumn::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    if (onSelected)
    {
        onSelected(m_stepIndex);
    }
}

void StepColumn::mouseEnter(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    reportState();
}

void StepColumn::setCurrent(bool isCurrentStep)
{
    m_stateButton.setCurrent(isCurrentStep);
}

void StepColumn::setInvalid(bool isInvalid)
{
    m_stateButton.setInvalid(isInvalid);
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
    const int gap = static_cast<int>(static_cast<float>(area.getWidth()) * kGapFactor);
    area.removeFromTop(static_cast<int>(kNumberHeight));
    m_stateButton.setBounds(area.reduced(gap, 0));
}

// ---------------------------------------------------------------------------------------------- ValueStrip
ValueStrip::ValueStrip(juce::AudioProcessorValueTreeState& apvts, int firstStep, int numColumns)
    : m_apvts(apvts), m_firstStep(firstStep), m_numColumns(numColumns)
{
    setTooltip("The bars show the note, velocity or pan of every step (the buttons below choose which). Draw over them with the mouse: "
               "a line from left to right sets all the bars it crosses. Double-click puts a bar back to its default.");
    bind(0, GridPage::Note);
}

StepParam ValueStrip::pageParameter() const
{
    if (m_page == GridPage::Velocity)
    {
        return StepParam::Velocity;
    }
    if (m_page == GridPage::Pan)
    {
        return StepParam::Pan;
    }
    return StepParam::Note;
}

void ValueStrip::bind(int patternIndex, GridPage page)
{
    releaseGestures();
    m_patternIndex = patternIndex;
    m_page = page;
    for (int column = 0; column < m_numColumns; ++column)
    {
        const std::size_t slot = static_cast<std::size_t>(column);
        juce::RangedAudioParameter* parameter = m_apvts.getParameter(stepParameterId(patternIndex, m_firstStep + column, pageParameter()));
        m_attachments[slot].reset();
        m_attachments[slot] = std::make_unique<juce::ParameterAttachment>(*parameter,
                                                                          [this, slot](float value)
                                                                          {
                                                                              m_values[slot] = value;
                                                                              repaint();
                                                                          });
        m_attachments[slot]->sendInitialUpdate();
        if (column == 0)
        {
            m_range = parameter->getNormalisableRange();
            m_defaultValue = m_range.convertFrom0to1(parameter->getDefaultValue());
        }
    }
    repaint();
}

void ValueStrip::setPatternLength(int length)
{
    if (length != m_patternLength)
    {
        m_patternLength = length;
        repaint();
    }
}

int ValueStrip::columnAt(float x) const
{
    const float columnWidth = static_cast<float>(getWidth()) / static_cast<float>(m_numColumns);
    return juce::jlimit(0, m_numColumns - 1, static_cast<int>(x / columnWidth));
}

// the value for a vertical position: the bottom is the minimum, the top the maximum
float ValueStrip::valueAtY(float y) const
{
    const float proportion = 1.0f - juce::jlimit(0.0f, 1.0f, y / static_cast<float>(getHeight()));
    return m_range.convertFrom0to1(proportion);
}

void ValueStrip::setColumnValue(int column, float value)
{
    const std::size_t slot = static_cast<std::size_t>(column);
    if (m_inGesture[slot] == false)
    {
        m_attachments[slot]->beginGesture();
        m_inGesture[slot] = true;
    }
    m_attachments[slot]->setValueAsPartOfGesture(value);
}

void ValueStrip::releaseGestures()
{
    for (int column = 0; column < m_numColumns; ++column)
    {
        const std::size_t slot = static_cast<std::size_t>(column);
        if (m_inGesture[slot] && m_attachments[slot] != nullptr)
        {
            m_attachments[slot]->endGesture();
        }
        m_inGesture[slot] = false;
    }
    m_dragging = false;
}

juce::String ValueStrip::describe(int column) const
{
    return describeValue(m_patternIndex, m_firstStep + column, m_page, m_values[static_cast<std::size_t>(column)]);
}

void ValueStrip::mouseDown(const juce::MouseEvent& event)
{
    m_dragging = true;
    m_lastPosition = event.position;
    const int column = columnAt(event.position.x);
    setColumnValue(column, valueAtY(event.position.y));
    if (onInfo)
    {
        onInfo(describe(column));
    }
}

void ValueStrip::mouseDrag(const juce::MouseEvent& event)
{
    if (m_dragging == false)
    {
        return;
    }
    // every column between the last and the new position gets the value of the straight line between the two points,
    // so a fast drag does not skip bars
    const int firstColumn = columnAt(m_lastPosition.x);
    const int lastColumn = columnAt(event.position.x);
    const int step = (lastColumn >= firstColumn) ? 1 : -1;
    const float deltaX = event.position.x - m_lastPosition.x;
    const float columnWidth = static_cast<float>(getWidth()) / static_cast<float>(m_numColumns);
    for (int column = firstColumn; column != lastColumn + step; column += step)
    {
        float y = event.position.y;
        if (std::abs(deltaX) > 0.0f)
        {
            const float centreX = (static_cast<float>(column) + kHalf) * columnWidth;
            const float fraction = juce::jlimit(0.0f, 1.0f, (centreX - m_lastPosition.x) / deltaX);
            y = m_lastPosition.y + fraction * (event.position.y - m_lastPosition.y);
        }
        setColumnValue(column, valueAtY(y));
    }
    m_lastPosition = event.position;
    if (onInfo)
    {
        onInfo(describe(lastColumn));
    }
}

void ValueStrip::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    releaseGestures();
}

void ValueStrip::mouseMove(const juce::MouseEvent& event)
{
    if (onInfo)
    {
        onInfo(describe(columnAt(event.position.x)));
    }
}

void ValueStrip::mouseDoubleClick(const juce::MouseEvent& event)
{
    const int column = columnAt(event.position.x);
    const std::size_t slot = static_cast<std::size_t>(column);
    m_attachments[slot]->setValueAsCompleteGesture(m_defaultValue);
    if (onInfo)
    {
        onInfo(describe(column));
    }
}

void ValueStrip::drawBar(juce::Graphics& g, int column) const
{
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    const juce::Colour textColour = getLookAndFeel().findColour(juce::Label::textColourId);
    const juce::Colour accent = getLookAndFeel().findColour(juce::Slider::thumbColourId);
    const float columnWidth = static_cast<float>(getWidth()) / static_cast<float>(m_numColumns);
    const float height = static_cast<float>(getHeight());
    const float left = static_cast<float>(column) * columnWidth;
    const bool onBeat = ((m_firstStep + column) % kBeatLength) == 0;
    float alpha = kInsidePatternAlpha;
    if (m_firstStep + column >= m_patternLength)
    {
        alpha = kOutsidePatternAlpha;
    }

    // the cell
    g.setColour(background.interpolatedWith(textColour, onBeat ? kStripBeatMix : kStripMix).withAlpha(alpha));
    g.fillRect(left, 0.0f, columnWidth, height);

    // the bar: from the bottom (from the middle for the pan) to the value
    const float proportion = m_range.convertTo0to1(m_values[static_cast<std::size_t>(column)]);
    const float valueY = height * (1.0f - proportion);
    float baselineY = height;
    if (m_page == GridPage::Pan)
    {
        baselineY = height * kHalf;
    }
    const float top = std::min(valueY, baselineY);
    const float bottom = std::max(valueY, baselineY);
    g.setColour(accent.withAlpha(kStripBarAlpha * alpha));
    g.fillRect(left + kStripBarGap, top, columnWidth - 2.0f * kStripBarGap, std::max(1.0f, bottom - top));
}

void ValueStrip::paint(juce::Graphics& g)
{
    const juce::Colour textColour = getLookAndFeel().findColour(juce::Label::textColourId);
    for (int column = 0; column < m_numColumns; ++column)
    {
        drawBar(g, column);
    }
    if (m_page == GridPage::Pan)
    {
        g.setColour(textColour.withAlpha(kStripBorderAlpha));
        const float middle = static_cast<float>(getHeight()) * kHalf;
        g.drawHorizontalLine(static_cast<int>(middle), 0.0f, static_cast<float>(getWidth()));
    }
    g.setColour(textColour.withAlpha(kStripBorderAlpha));
    g.drawRect(getLocalBounds().toFloat(), 1.0f);
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
    for (int row = 0; row < kNumRows; ++row)
    {
        m_strips[static_cast<std::size_t>(row)] = std::make_unique<ValueStrip>(apvts, row * kStepsPerRow, kStepsPerRow);
        ValueStrip& strip = *m_strips[static_cast<std::size_t>(row)];
        strip.onInfo = [this](const juce::String& text)
        {
            if (onInfo)
            {
                onInfo(text);
            }
        };
        addAndMakeVisible(strip);
    }
}

void StepGrid::setEditPattern(int patternIndex)
{
    const int clamped = juce::jlimit(0, kNumPatterns - 1, patternIndex);
    if (clamped == m_editPattern)
    {
        return;
    }
    m_editPattern = clamped;
    for (auto& column : m_columns)
    {
        column->bind(m_editPattern);
    }
    for (auto& strip : m_strips)
    {
        strip->bind(m_editPattern, m_page);
    }
}

void StepGrid::setPage(GridPage page)
{
    if (page == m_page)
    {
        return;
    }
    m_page = page;
    for (auto& strip : m_strips)
    {
        strip->bind(m_editPattern, m_page);
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

void StepGrid::update(int framedStep, juce::uint32 invalidStepMask, int patternLength)
{
    if (m_selectedStep >= patternLength)
    {
        m_selectedStep = juce::jmax(0, patternLength - 1);
    }
    for (int stepIndex = 0; stepIndex < kMaxSteps; ++stepIndex)
    {
        StepColumn& column = *m_columns[static_cast<std::size_t>(stepIndex)];
        column.setCurrent(stepIndex == framedStep);
        const juce::uint32 bit = juce::uint32(1) << stepIndex;
        column.setInvalid((invalidStepMask & bit) != 0);
        column.setInsidePattern(stepIndex < patternLength);
    }
    for (auto& strip : m_strips)
    {
        strip->setPatternLength(patternLength);
    }
}

void StepGrid::resized()
{
    const auto area = getLocalBounds();
    const int columnWidth = area.getWidth() / kStepsPerRow;
    const int rowHeight = area.getHeight() / kNumRows;
    const int rowGap = static_cast<int>(static_cast<float>(rowHeight) * kRowGapFraction);
    const int stepsHeight = static_cast<int>(static_cast<float>(rowHeight) * kStepsAreaFraction);
    for (int row = 0; row < kNumRows; ++row)
    {
        const int rowTop = row * rowHeight;
        for (int column = 0; column < kStepsPerRow; ++column)
        {
            const int stepIndex = row * kStepsPerRow + column;
            m_columns[static_cast<std::size_t>(stepIndex)]->setBounds(column * columnWidth, rowTop, columnWidth, stepsHeight);
        }
        m_strips[static_cast<std::size_t>(row)]->setBounds(0, rowTop + stepsHeight, columnWidth * kStepsPerRow, rowHeight - stepsHeight - rowGap);
    }
}
} // namespace dfd
