/**
 * @file StepGrid.h
 * @brief the step grid: per step a step button, a mode button (forward / reverse / reverse + play to end),
 *        a pitch knob and a velocity knob; all connected to the parameters
 */
#pragma once

#include <array>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../src/StepParameters.h"
#include "../src/StepScheduler.h"

namespace dfd
{
// cycles through the three modes of a step (parameter value 0, 1, 2)
class ModeButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ModeButton(juce::RangedAudioParameter& parameter);

    void setStepActive(bool isActive);
    void setInvalid(bool isInvalid); // play to end is set, but the sample is longer than the pattern
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    float currentMode() const;

    juce::ParameterAttachment m_attachment;
    float m_value = 0.0f;
    bool m_stepActive = false;
    bool m_invalid = false;
};

// a step button that lights up when the step is on
class StepButton : public juce::ToggleButton
{
public:
    void setCurrent(bool isCurrentStep);
    void setBeatAccent(bool isOnBeat);
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    bool m_current = false;
    bool m_beatAccent = false;
};

class StepColumn : public juce::Component
{
public:
    StepColumn(int stepIndex, juce::AudioProcessorValueTreeState& apvts);

    void setCurrent(bool isCurrentStep);
    void setInvalid(bool isInvalid);
    void setInsidePattern(bool isInside);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void updateModeButton();

    int m_stepIndex;
    StepButton m_stepButton;
    ModeButton m_modeButton;
    juce::Slider m_noteSlider;
    juce::Slider m_velocitySlider;
    juce::AudioProcessorValueTreeState::ButtonAttachment m_stepAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment m_noteAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment m_velocityAttachment;
};

class StepGrid : public juce::Component
{
public:
    explicit StepGrid(juce::AudioProcessorValueTreeState& apvts);

    // called by the GUI timer
    void update(int currentStep, juce::uint32 invalidStepMask, int patternLength);
    void resized() override;

private:
    std::array<std::unique_ptr<StepColumn>, kMaxSteps> m_columns;
};
} // namespace dfd
