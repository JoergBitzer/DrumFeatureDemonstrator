#include "WaveformView.h"

#include "../src/DebugLog.h"

#include <algorithm>
#include <cmath>

namespace dfd
{
namespace
{
constexpr float kRegionGrabDistance = 8.0f;
constexpr float kFadeGrabDistance = 10.0f;
constexpr float kFadeZoneFraction = 0.25f;   // the top quarter belongs to the fade handles, the rest to start and end
constexpr float kMinRegionFraction = 0.001f;
constexpr float kLineWidth = 2.0f;
constexpr float kFadeHandleSize = 10.0f;
constexpr float kKnobWidth = 8.0f;
constexpr float kKnobHeight = 28.0f;
constexpr float kEnvelopeTopMargin = 6.0f;
constexpr float kShadeAlpha = 0.55f;
constexpr float kWaveAlpha = 0.9f;
constexpr float kBorderAlpha = 0.5f;
constexpr float kDropAlpha = 0.25f;
constexpr float kGuideAlpha = 0.35f;
constexpr float kLabelBackgroundAlpha = 0.85f;
constexpr float kCornerSize = 4.0f;
constexpr int kZeroCrossingRadiusSamples = 2048;
constexpr float kHalf = 0.5f;
constexpr float kFontSizeFactor = 0.12f;
constexpr float kMaxFontSize = 16.0f;
constexpr float kLabelFontHeight = 12.0f;
constexpr float kLabelPadding = 4.0f;
constexpr float kLabelGap = 6.0f;
constexpr float kLabelBottomMargin = 4.0f;
constexpr float kNoSampleSeconds = 1.0f;
constexpr float kMaxFadeSeconds = 10.0f;
constexpr int kDecimalsOfSeconds = 3;
const juce::Colour kHandleColour(0xFFF0A030);
} // namespace

WaveformView::WaveformView(juce::RangedAudioParameter& startParameter, juce::RangedAudioParameter& endParameter,
                           juce::RangedAudioParameter& fadeInParameter, juce::RangedAudioParameter& fadeOutParameter)
    : m_startAttachment(startParameter,
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
                      }),
      m_fadeInAttachment(fadeInParameter,
                         [this](float value)
                         {
                             m_fadeInSeconds = value;
                             repaint();
                         }),
      m_fadeOutAttachment(fadeOutParameter,
                          [this](float value)
                          {
                              m_fadeOutSeconds = value;
                              repaint();
                          })
{
    m_startAttachment.sendInitialUpdate();
    m_endAttachment.sendInitialUpdate();
    m_fadeInAttachment.sendInitialUpdate();
    m_fadeOutAttachment.sendInitialUpdate();
    setTooltip("Waveform: drag the start and end line (middle and lower part) to set the played region, drag the small squares at the top "
               "for the fade in and fade out time. Drop a sample file here to load it.");
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

float WaveformView::sampleSeconds() const
{
    if (m_sample == nullptr || m_sample->sampleRate <= 0.0)
    {
        return kNoSampleSeconds;
    }
    return static_cast<float>(static_cast<double>(m_sample->numSamples()) / m_sample->sampleRate);
}

float WaveformView::fractionToX(float fraction) const
{
    return fraction * static_cast<float>(getWidth());
}

float WaveformView::secondsToWidth(float seconds) const
{
    return seconds / sampleSeconds() * static_cast<float>(getWidth());
}

float WaveformView::xToFraction(float x) const
{
    if (getWidth() <= 0)
    {
        return 0.0f;
    }
    return juce::jlimit(0.0f, 1.0f, x / static_cast<float>(getWidth()));
}

// both fades together cannot be longer than the region (the plug-in shortens them in the same way)
void WaveformView::effectiveFades(float& fadeInSeconds, float& fadeOutSeconds) const
{
    const float regionSeconds = (m_endFraction - m_startFraction) * sampleSeconds();
    fadeInSeconds = m_fadeInSeconds;
    fadeOutSeconds = m_fadeOutSeconds;
    const float total = fadeInSeconds + fadeOutSeconds;
    if (total > regionSeconds && total > 0.0f)
    {
        const float scale = std::max(0.0f, regionSeconds) / total;
        fadeInSeconds *= scale;
        fadeOutSeconds *= scale;
    }
}

juce::String WaveformView::secondsText(float seconds)
{
    return juce::String(seconds, kDecimalsOfSeconds) + " s";
}

void WaveformView::drawTimeLabel(juce::Graphics& g, float lineX, float seconds, bool labelOnTheRight) const
{
    const juce::Font font{juce::FontOptions{kLabelFontHeight}};
    const juce::String text = secondsText(seconds);
    const float width = juce::GlyphArrangement::getStringWidth(font, text) + 2.0f * kLabelPadding;
    const float height = kLabelFontHeight + kLabelPadding;
    float left = lineX - width - kLabelGap;
    if (labelOnTheRight)
    {
        left = lineX + kLabelGap;
    }
    left = juce::jlimit(0.0f, static_cast<float>(getWidth()) - width, left);
    const juce::Rectangle<float> box(left, static_cast<float>(getHeight()) - height - kLabelBottomMargin, width, height);
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    g.setColour(background.withAlpha(kLabelBackgroundAlpha));
    g.fillRoundedRectangle(box, kCornerSize);
    g.setColour(kHandleColour);
    g.drawRoundedRectangle(box, kCornerSize, 1.0f);
    g.setFont(font);
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId));
    g.drawText(text, box, juce::Justification::centred);
}

void WaveformView::drawFadeLabel(juce::Graphics& g, float handleX, float seconds, bool labelOnTheRight) const
{
    const juce::Font font{juce::FontOptions{kLabelFontHeight}};
    const juce::String text = "fade " + secondsText(seconds);
    const float width = juce::GlyphArrangement::getStringWidth(font, text) + 2.0f * kLabelPadding;
    const float height = kLabelFontHeight + kLabelPadding;
    float left = handleX - width - kFadeHandleSize;
    if (labelOnTheRight)
    {
        left = handleX + kFadeHandleSize;
    }
    left = juce::jlimit(0.0f, static_cast<float>(getWidth()) - width, left);
    const juce::Rectangle<float> box(left, kEnvelopeTopMargin, width, height);
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    g.setColour(background.withAlpha(kLabelBackgroundAlpha));
    g.fillRoundedRectangle(box, kCornerSize);
    g.setColour(kHandleColour);
    g.drawRoundedRectangle(box, kCornerSize, 1.0f);
    g.setFont(font);
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId));
    g.drawText(text, box, juce::Justification::centred);
}

void WaveformView::paint(juce::Graphics& g)
{
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    const juce::Colour textColour = getLookAndFeel().findColour(juce::Label::textColourId);
    const auto bounds = getLocalBounds().toFloat();
    const float height = bounds.getHeight();
    g.setColour(background.darker(0.4f));
    g.fillRoundedRectangle(bounds, kCornerSize);

    const float middle = height * kHalf;
    if (m_sample == nullptr || m_peakMax.empty())
    {
        g.setColour(textColour.withAlpha(kBorderAlpha));
        g.setFont(juce::jmin(kMaxFontSize, height * kFontSizeFactor * 2.0f));
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
    g.fillRect(0.0f, 0.0f, startX, height);
    g.fillRect(endX, 0.0f, bounds.getWidth() - endX, height);

    // faint guide lines of the region handles (the envelope below is slanted if there is a fade)
    g.setColour(kHandleColour.withAlpha(kGuideAlpha));
    g.fillRect(startX - 0.5f * kLineWidth, 0.0f, kLineWidth, height);
    g.fillRect(endX - 0.5f * kLineWidth, 0.0f, kLineWidth, height);

    // the orange envelope: start line -> fade-in handle -> fade-out handle -> end line
    float fadeIn = 0.0f;
    float fadeOut = 0.0f;
    effectiveFades(fadeIn, fadeOut);
    const float fadeInX = startX + secondsToWidth(fadeIn);
    const float fadeOutX = endX - secondsToWidth(fadeOut);
    juce::Path envelope;
    envelope.startNewSubPath(startX, height);
    envelope.lineTo(fadeInX, kEnvelopeTopMargin);
    envelope.lineTo(fadeOutX, kEnvelopeTopMargin);
    envelope.lineTo(endX, height);
    g.setColour(kHandleColour);
    g.strokePath(envelope, juce::PathStrokeType(kLineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // knobs of the start and end line (middle of the display) and squares of the fades (top)
    const float knobY = middle - 0.5f * kKnobHeight;
    g.fillRoundedRectangle(startX - 0.5f * kKnobWidth, knobY, kKnobWidth, kKnobHeight, kCornerSize);
    g.fillRoundedRectangle(endX - 0.5f * kKnobWidth, knobY, kKnobWidth, kKnobHeight, kCornerSize);
    g.fillRect(fadeInX - 0.5f * kFadeHandleSize, kEnvelopeTopMargin - 0.5f * kFadeHandleSize, kFadeHandleSize, kFadeHandleSize);
    g.fillRect(fadeOutX - 0.5f * kFadeHandleSize, kEnvelopeTopMargin - 0.5f * kFadeHandleSize, kFadeHandleSize, kFadeHandleSize);

    // times: the start and the end line show their time in the sample
    drawTimeLabel(g, startX, m_startFraction * sampleSeconds(), true);
    drawTimeLabel(g, endX, m_endFraction * sampleSeconds(), false);
    if (m_hoveredHandle == Handle::FadeIn || m_draggedHandle == Handle::FadeIn)
    {
        drawFadeLabel(g, fadeInX, fadeIn, true);
    }
    if (m_hoveredHandle == Handle::FadeOut || m_draggedHandle == Handle::FadeOut)
    {
        drawFadeLabel(g, fadeOutX, fadeOut, false);
    }

    g.setColour(textColour.withAlpha(kBorderAlpha));
    g.drawRoundedRectangle(bounds.reduced(kHalf), kCornerSize, 1.0f);
    if (m_fileIsOver)
    {
        g.setColour(kHandleColour.withAlpha(kDropAlpha));
        g.fillRoundedRectangle(bounds, kCornerSize);
    }
}

WaveformView::Handle WaveformView::handleAt(float x, float y) const
{
    float fadeIn = 0.0f;
    float fadeOut = 0.0f;
    effectiveFades(fadeIn, fadeOut);
    const float startX = fractionToX(m_startFraction);
    const float endX = fractionToX(m_endFraction);
    const float fadeInX = startX + secondsToWidth(fadeIn);
    const float fadeOutX = endX - secondsToWidth(fadeOut);

    const bool inFadeZone = y < kFadeZoneFraction * static_cast<float>(getHeight());
    if (inFadeZone)
    {
        const float distanceToFadeIn = std::abs(x - fadeInX);
        const float distanceToFadeOut = std::abs(x - fadeOutX);
        if (distanceToFadeIn > kFadeGrabDistance && distanceToFadeOut > kFadeGrabDistance)
        {
            return Handle::None;
        }
        if (distanceToFadeIn < distanceToFadeOut)
        {
            return Handle::FadeIn;
        }
        if (distanceToFadeOut < distanceToFadeIn)
        {
            return Handle::FadeOut;
        }
        // both squares at the same place (no room between them): the side of the middle decides
        if (x < 0.5f * (startX + endX))
        {
            return Handle::FadeIn;
        }
        return Handle::FadeOut;
    }

    const float distanceToStart = std::abs(x - startX);
    const float distanceToEnd = std::abs(x - endX);
    if (distanceToStart > kRegionGrabDistance && distanceToEnd > kRegionGrabDistance)
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

void WaveformView::setHandle(Handle handle, float x)
{
    const float fraction = xToFraction(x);
    if (handle == Handle::Start)
    {
        const float limit = m_endFraction - kMinRegionFraction;
        m_startAttachment.setValueAsPartOfGesture(juce::jlimit(0.0f, limit, snapToZeroCrossing(fraction)));
    }
    if (handle == Handle::End)
    {
        const float limit = m_startFraction + kMinRegionFraction;
        m_endAttachment.setValueAsPartOfGesture(juce::jlimit(limit, 1.0f, snapToZeroCrossing(fraction)));
    }
    if (handle == Handle::FadeIn)
    {
        // the fade-in handle moves between the start line and the fade-out handle
        const float regionSeconds = (m_endFraction - m_startFraction) * sampleSeconds();
        const float seconds = (x - fractionToX(m_startFraction)) / static_cast<float>(getWidth()) * sampleSeconds();
        const float limit = std::min(kMaxFadeSeconds, std::max(0.0f, regionSeconds - m_fadeOutSeconds));
        m_fadeInAttachment.setValueAsPartOfGesture(juce::jlimit(0.0f, limit, seconds));
    }
    if (handle == Handle::FadeOut)
    {
        const float regionSeconds = (m_endFraction - m_startFraction) * sampleSeconds();
        const float seconds = (fractionToX(m_endFraction) - x) / static_cast<float>(getWidth()) * sampleSeconds();
        const float limit = std::min(kMaxFadeSeconds, std::max(0.0f, regionSeconds - m_fadeInSeconds));
        m_fadeOutAttachment.setValueAsPartOfGesture(juce::jlimit(0.0f, limit, seconds));
    }
}

void WaveformView::beginGestureOf(Handle handle)
{
    if (handle == Handle::Start)
    {
        m_startAttachment.beginGesture();
    }
    if (handle == Handle::End)
    {
        m_endAttachment.beginGesture();
    }
    if (handle == Handle::FadeIn)
    {
        m_fadeInAttachment.beginGesture();
    }
    if (handle == Handle::FadeOut)
    {
        m_fadeOutAttachment.beginGesture();
    }
}

void WaveformView::endGestureOf(Handle handle)
{
    if (handle == Handle::Start)
    {
        m_startAttachment.endGesture();
    }
    if (handle == Handle::End)
    {
        m_endAttachment.endGesture();
    }
    if (handle == Handle::FadeIn)
    {
        m_fadeInAttachment.endGesture();
    }
    if (handle == Handle::FadeOut)
    {
        m_fadeOutAttachment.endGesture();
    }
}

void WaveformView::mouseMove(const juce::MouseEvent& event)
{
    const Handle hovered = handleAt(event.position.x, event.position.y);
    if (hovered != m_hoveredHandle)
    {
        m_hoveredHandle = hovered;
        repaint();
    }
    if (hovered == Handle::None)
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    else
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }
}

void WaveformView::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    m_hoveredHandle = Handle::None;
    repaint();
}

void WaveformView::mouseDown(const juce::MouseEvent& event)
{
    m_draggedHandle = handleAt(event.position.x, event.position.y);
    beginGestureOf(m_draggedHandle);
}

void WaveformView::mouseDrag(const juce::MouseEvent& event)
{
    setHandle(m_draggedHandle, event.position.x);
}

void WaveformView::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    endGestureOf(m_draggedHandle);
    m_draggedHandle = Handle::None;
    repaint();
}

bool WaveformView::isInterestedInFileDrag(const juce::StringArray& files)
{
    // any file: the loader decides whether it can read it, and the user gets a message if not
    debugLog("waveform: isInterestedInFileDrag, files: " + files.joinIntoString(" | "));
    return files.size() > 0;
}

void WaveformView::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(files, x, y);
    debugLog("waveform: fileDragEnter");
    m_fileIsOver = true;
    repaint();
}

void WaveformView::fileDragExit(const juce::StringArray& files)
{
    juce::ignoreUnused(files);
    m_fileIsOver = false;
    repaint();
}

juce::File WaveformView::fileFromDroppedText(const juce::String& text)
{
    const juce::String trimmed = text.trim().unquoted();
    if (trimmed.startsWithIgnoreCase("file://"))
    {
        return juce::URL(trimmed).getLocalFile();
    }
    return juce::File(trimmed);
}

bool WaveformView::isInterestedInTextDrag(const juce::String& text)
{
    debugLog("waveform: isInterestedInTextDrag: " + text);
    return fileFromDroppedText(text).existsAsFile();
}

void WaveformView::textDropped(const juce::String& text, int x, int y)
{
    juce::ignoreUnused(x, y);
    const juce::File file = fileFromDroppedText(text);
    if (file.existsAsFile() && onFileDropped)
    {
        onFileDropped(file);
    }
}

void WaveformView::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(x, y);
    debugLog("waveform: filesDropped, files: " + files.joinIntoString(" | "));
    m_fileIsOver = false;
    repaint();
    if (files.size() > 0 && onFileDropped)
    {
        onFileDropped(juce::File(files[0]));
    }
}
} // namespace dfd
