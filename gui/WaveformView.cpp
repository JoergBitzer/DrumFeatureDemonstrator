#include "WaveformView.h"

#include <algorithm>
#include <cmath>

namespace dfd
{
namespace
{
constexpr float kHandleGrabDistance = 8.0f;
constexpr float kMinRegionFraction = 0.001f;
constexpr float kHandleWidth = 2.0f;
constexpr float kHandleKnobSize = 12.0f;
constexpr float kShadeAlpha = 0.55f;
constexpr float kWaveAlpha = 0.9f;
constexpr float kBorderAlpha = 0.5f;
constexpr float kDropAlpha = 0.25f;
constexpr float kCornerSize = 4.0f;
constexpr int kZeroCrossingRadiusSamples = 2048;
constexpr float kHalf = 0.5f;
constexpr float kFontSizeFactor = 0.12f;
constexpr float kMaxFontSize = 16.0f;
const juce::Colour kHandleColour(0xFFF0A030);
} // namespace

WaveformView::WaveformView(juce::RangedAudioParameter& startParameter, juce::RangedAudioParameter& endParameter)
    : m_startParameter(startParameter),
      m_endParameter(endParameter),
      m_startAttachment(startParameter,
                        [this](float value)
                        {
                            m_startFraction = value;
                            repaint();
                        }),
      m_endAttachment(endParameter,
                      [this](float value)
                      {
                          m_endFraction = value;
                          repaint();
                      })
{
    m_startAttachment.sendInitialUpdate();
    m_endAttachment.sendInitialUpdate();
    setTooltip("Waveform of the sample: drag the two handles to set start and end of the played region. Drop a sample file here to load it.");
}

void WaveformView::setSample(std::shared_ptr<const SampleData> sample)
{
    m_sample = std::move(sample);
    rebuildPeaks();
    repaint();
}

void WaveformView::setDeclickEnabled(bool shouldSnap)
{
    m_snapToZeroCrossing = shouldSnap;
}

void WaveformView::resized()
{
    rebuildPeaks();
}

void WaveformView::rebuildPeaks()
{
    m_peakMin.clear();
    m_peakMax.clear();
    const int numColumns = getWidth();
    if (m_sample == nullptr || numColumns <= 0 || m_sample->numSamples() == 0)
    {
        return;
    }
    const int numSamples = m_sample->numSamples();
    for (int column = 0; column < numColumns; ++column)
    {
        const int first = static_cast<int>(static_cast<long long>(column) * numSamples / numColumns);
        const int last = std::max(first + 1, static_cast<int>(static_cast<long long>(column + 1) * numSamples / numColumns));
        float lowest = 0.0f;
        float highest = 0.0f;
        for (int index = first; index < std::min(last, numSamples); ++index)
        {
            const float value = m_sample->monoValueAt(index);
            lowest = std::min(lowest, value);
            highest = std::max(highest, value);
        }
        m_peakMin.push_back(lowest);
        m_peakMax.push_back(highest);
    }
}

float WaveformView::fractionToX(float fraction) const
{
    return fraction * static_cast<float>(getWidth());
}

float WaveformView::xToFraction(float x) const
{
    if (getWidth() <= 0)
    {
        return 0.0f;
    }
    return juce::jlimit(0.0f, 1.0f, x / static_cast<float>(getWidth()));
}

void WaveformView::drawHandle(juce::Graphics& g, float x, bool isStart) const
{
    const float height = static_cast<float>(getHeight());
    g.setColour(kHandleColour);
    g.fillRect(x - kHandleWidth * kHalf, 0.0f, kHandleWidth, height);
    juce::Path knob;
    if (isStart)
    {
        knob.addTriangle(x, 0.0f, x + kHandleKnobSize, 0.0f, x, kHandleKnobSize);
    }
    else
    {
        knob.addTriangle(x, 0.0f, x - kHandleKnobSize, 0.0f, x, kHandleKnobSize);
    }
    g.fillPath(knob);
}

void WaveformView::paint(juce::Graphics& g)
{
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    const juce::Colour textColour = getLookAndFeel().findColour(juce::Label::textColourId);
    const auto bounds = getLocalBounds().toFloat();
    g.setColour(background.darker(0.4f));
    g.fillRoundedRectangle(bounds, kCornerSize);

    const float middle = bounds.getHeight() * kHalf;
    if (m_sample == nullptr || m_peakMax.empty())
    {
        g.setColour(textColour.withAlpha(kBorderAlpha));
        g.setFont(juce::jmin(kMaxFontSize, bounds.getHeight() * kFontSizeFactor * 2.0f));
        g.drawFittedText("Drop a sample here or use Load sample", getLocalBounds(), juce::Justification::centred, 1);
    }
    else
    {
        g.setColour(textColour.withAlpha(kWaveAlpha));
        for (std::size_t column = 0; column < m_peakMax.size(); ++column)
        {
            const float top = middle - m_peakMax[column] * middle;
            const float bottom = middle - m_peakMin[column] * middle;
            g.drawVerticalLine(static_cast<int>(column), top, std::max(bottom, top + 1.0f));
        }
    }

    // the part outside the region is shaded
    const float startX = fractionToX(m_startFraction);
    const float endX = fractionToX(m_endFraction);
    g.setColour(background.darker(0.4f).withAlpha(kShadeAlpha));
    g.fillRect(0.0f, 0.0f, startX, bounds.getHeight());
    g.fillRect(endX, 0.0f, bounds.getWidth() - endX, bounds.getHeight());
    drawHandle(g, startX, true);
    drawHandle(g, endX, false);

    g.setColour(textColour.withAlpha(kBorderAlpha));
    g.drawRoundedRectangle(bounds.reduced(kHalf), kCornerSize, 1.0f);
    if (m_fileIsOver)
    {
        g.setColour(kHandleColour.withAlpha(kDropAlpha));
        g.fillRoundedRectangle(bounds, kCornerSize);
    }
}

WaveformView::Handle WaveformView::handleAt(float x) const
{
    const float distanceToStart = std::abs(x - fractionToX(m_startFraction));
    const float distanceToEnd = std::abs(x - fractionToX(m_endFraction));
    if (distanceToStart > kHandleGrabDistance && distanceToEnd > kHandleGrabDistance)
    {
        return Handle::None;
    }
    if (distanceToStart <= distanceToEnd)
    {
        return Handle::Start;
    }
    return Handle::End;
}

float WaveformView::snapToZeroCrossing(float fraction) const
{
    if (m_snapToZeroCrossing == false || m_sample == nullptr || m_sample->numSamples() == 0)
    {
        return fraction;
    }
    const int numSamples = m_sample->numSamples();
    const int position = static_cast<int>(std::lround(fraction * static_cast<float>(numSamples)));
    const int snapped = findNearestZeroCrossing(*m_sample, position, kZeroCrossingRadiusSamples);
    return static_cast<float>(snapped) / static_cast<float>(numSamples);
}

void WaveformView::setHandleFraction(Handle handle, float fraction)
{
    if (handle == Handle::Start)
    {
        const float limit = m_endFraction - kMinRegionFraction;
        const float value = juce::jlimit(0.0f, limit, snapToZeroCrossing(fraction));
        m_startAttachment.setValueAsPartOfGesture(value);
    }
    if (handle == Handle::End)
    {
        const float limit = m_startFraction + kMinRegionFraction;
        const float value = juce::jlimit(limit, 1.0f, snapToZeroCrossing(fraction));
        m_endAttachment.setValueAsPartOfGesture(value);
    }
}

void WaveformView::mouseMove(const juce::MouseEvent& event)
{
    if (handleAt(event.position.x) == Handle::None)
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    else
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }
}

void WaveformView::mouseDown(const juce::MouseEvent& event)
{
    m_draggedHandle = handleAt(event.position.x);
    if (m_draggedHandle == Handle::Start)
    {
        m_startAttachment.beginGesture();
    }
    if (m_draggedHandle == Handle::End)
    {
        m_endAttachment.beginGesture();
    }
}

void WaveformView::mouseDrag(const juce::MouseEvent& event)
{
    setHandleFraction(m_draggedHandle, xToFraction(event.position.x));
}

void WaveformView::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    if (m_draggedHandle == Handle::Start)
    {
        m_startAttachment.endGesture();
    }
    if (m_draggedHandle == Handle::End)
    {
        m_endAttachment.endGesture();
    }
    m_draggedHandle = Handle::None;
}

bool WaveformView::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() > 0;
}

void WaveformView::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(files, x, y);
    m_fileIsOver = true;
    repaint();
}

void WaveformView::fileDragExit(const juce::StringArray& files)
{
    juce::ignoreUnused(files);
    m_fileIsOver = false;
    repaint();
}

void WaveformView::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(x, y);
    m_fileIsOver = false;
    repaint();
    if (files.size() > 0 && onFileDropped)
    {
        onFileDropped(juce::File(files[0]));
    }
}
} // namespace dfd
