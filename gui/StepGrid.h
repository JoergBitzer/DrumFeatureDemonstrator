/**
 * @file StepGrid.h
 * @brief the step grid: per step a step button, a mode button (forward / reverse / reverse + play to end),
 *        a pitch knob and a velocity knob; all connected to the parameters
 */
#pragma once

#include <array>
#include <functional>
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

    int getMode() const;
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

    std::function<void(int)> onSelected;                 // the user clicked into this column
    std::function<void(const juce::String&)> onInfo;     // text for the info line (hover or change of a control)

    void setCurrent(bool isCurrentStep);
    void setInvalid(bool isInvalid);
    void setInsidePattern(bool isInside);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;

private:
    void updateModeButton();
    void reportControl(const juce::Component* control);
    juce::String describeControl(const juce::Component* control) const;

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

    std::function<void(const juce::String&)> onInfo;

    // the selected step is the one MIDI input and the cursor keys work on; it is framed while the transport stands
    int getSelectedStep() const;
    void setSelectedStep(int stepIndex);
    void moveSelectedStep(int numSteps, int patternLength);

    // called by the GUI timer: currentStep is -1 if the transport stands
    void update(int currentStep, juce::uint32 invalidStepMask, int patternLength);
    void resized() override;

private:
    int m_selectedStep = 0;
    std::array<std::unique_ptr<StepColumn>, kMaxSteps> m_columns;
};
} // namespace dfd
