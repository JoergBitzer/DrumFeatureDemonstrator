/**
 * @file WaveformView.h
 * @brief shows the sample with two handles (start and end of the played region); drop a sample file onto it
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
    WaveformView(juce::RangedAudioParameter& startParameter, juce::RangedAudioParameter& endParameter);

    void setSample(std::shared_ptr<const SampleData> sample);
    void setDeclickEnabled(bool shouldSnap);
    std::function<void(const juce::File&)> onFileDropped;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;

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
        End
    };

    static juce::File fileFromDroppedText(const juce::String& text);
    void rebuildPeaks();
    float fractionToX(float fraction) const;
    float xToFraction(float x) const;
    Handle handleAt(float x) const;
    float snapToZeroCrossing(float fraction) const;
    void setHandleFraction(Handle handle, float fraction);
    void drawHandle(juce::Graphics& g, float x, bool isStart) const;

    juce::RangedAudioParameter& m_startParameter;
    juce::RangedAudioParameter& m_endParameter;
    juce::ParameterAttachment m_startAttachment;
    juce::ParameterAttachment m_endAttachment;
    std::shared_ptr<const SampleData> m_sample;
    std::vector<float> m_peakMin;
    std::vector<float> m_peakMax;
    float m_startFraction = 0.0f;
    float m_endFraction = 1.0f;
    Handle m_draggedHandle = Handle::None;
    bool m_snapToZeroCrossing = false;
    bool m_fileIsOver = false;
};
} // namespace dfd
