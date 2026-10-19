/**
 * @file WaveformView.h
 * @brief shows the sample with the played region, its fade in / fade out and handles to change them; drop a sample file onto it
 *
 * The orange line is the envelope of the region: it rises from the start line to the fade-in handle at the top, runs along the
 * top and falls from the fade-out handle to the end line. Handles:
 *  - start and end line: grab them in the middle or lower part of the display (the time of the handle is shown next to it),
 *  - fade-in and fade-out handle (small squares at the top): drag them sideways to set the fade times,
 *  - shape handle (small circle in the middle of each fade line): drag it up or down to bend the fade curve from exponential (the
 *    circle below the straight line) through linear to anti-exponential (above it); a double-click makes the fade linear again.
 */
#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../src/SampleData.h"

namespace dfd
{
class WaveformView : public juce::Component, public juce::SettableTooltipClient, public juce::FileDragAndDropTarget,
                     public juce::TextDragAndDropTarget
{
public:
    WaveformView(juce::RangedAudioParameter& startParameter, juce::RangedAudioParameter& endParameter,
                 juce::RangedAudioParameter& fadeInParameter, juce::RangedAudioParameter& fadeOutParameter,
                 juce::RangedAudioParameter& fadeInShapeParameter, juce::RangedAudioParameter& fadeOutShapeParameter);

    void setSample(std::shared_ptr<const SampleData> sample);
    void setDeclickEnabled(bool shouldSnap);
    std::function<void(const juce::File&)> onFileDropped;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;

    // some file managers drop the path (or a file:// address) as text
    bool isInterestedInTextDrag(const juce::String& text) override;
    void textDropped(const juce::String& text, int x, int y) override;

private:
    enum class Handle
    {
        None,
        Start,
        End,
        FadeIn,
        FadeOut,
        FadeInShape,
        FadeOutShape
    };

    static juce::File fileFromDroppedText(const juce::String& text);
    void rebuildPeaks();
    float sampleSeconds() const;
    float fractionToX(float fraction) const;
    float secondsToWidth(float seconds) const;
    float xToFraction(float x) const;
    void effectiveFades(float& fadeInSeconds, float& fadeOutSeconds) const;
    Handle handleAt(float x, float y) const;
    float snapToZeroCrossing(float fraction) const;
    void setHandle(Handle handle, juce::Point<float> position);
    float curveY(float gain) const;
    float fadeWidthInPixels(bool forFadeIn) const;
    bool hasShapeHandle(bool forFadeIn) const;
    juce::Point<float> shapeHandlePosition(bool forFadeIn) const;
    void addFadeCurve(juce::Path& path, bool forFadeIn) const;
    void beginGestureOf(Handle handle);
    void endGestureOf(Handle handle);
    void drawTimeLabel(juce::Graphics& g, float lineX, float seconds, bool labelOnTheRight) const;
    void drawFadeLabel(juce::Graphics& g, float handleX, const juce::String& text, bool labelOnTheRight) const;
    static juce::String shapeText(float shape);
    static juce::String secondsText(float seconds);

    juce::ParameterAttachment m_startAttachment;
    juce::ParameterAttachment m_endAttachment;
    juce::ParameterAttachment m_fadeInAttachment;
    juce::ParameterAttachment m_fadeOutAttachment;
    juce::ParameterAttachment m_fadeInShapeAttachment;
    juce::ParameterAttachment m_fadeOutShapeAttachment;
    std::shared_ptr<const SampleData> m_sample;
    std::vector<float> m_peakMin;
    std::vector<float> m_peakMax;
    float m_startFraction = 0.0f;
    float m_endFraction = 1.0f;
    float m_fadeInSeconds = 0.0f;
    float m_fadeOutSeconds = 0.0f;
    float m_fadeInShape = 0.0f;
    float m_fadeOutShape = 0.0f;
    Handle m_draggedHandle = Handle::None;
    Handle m_hoveredHandle = Handle::None;
    bool m_snapToZeroCrossing = false;
    bool m_fileIsOver = false;
};
} // namespace dfd
