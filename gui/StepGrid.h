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
    ModeButton();

    // the parameter of the mode (the grid shows one of four patterns, so the button is connected to another parameter after a switch)
    void setParameter(juce::RangedAudioParameter& parameter);

    int getMode() const;
    void setStepActive(bool isActive);
    void setInvalid(bool isInvalid); // play to end is set, but the sample is longer than the pattern
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    float currentMode() const;

    std::unique_ptr<juce::ParameterAttachment> m_attachment;
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

// what the small slider under every step shows (the "pages" of the grid)
enum class GridPage
{
    Note,
    Velocity,
    Pan
};

class StepColumn : public juce::Component
{
public:
    StepColumn(int stepIndex, juce::AudioProcessorValueTreeState& apvts);

    std::function<void(int)> onSelected;                 // the user clicked into this column
    std::function<void(const juce::String&)> onInfo;     // text for the info line (hover or change of a control)

    // connects the controls to the parameters of this step in the given pattern and page
    void bind(int patternIndex, GridPage page);
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
    StepParam pageParameter() const;

    int m_stepIndex;
    int m_patternIndex = 0;
    GridPage m_page = GridPage::Note;
    juce::AudioProcessorValueTreeState& m_apvts;
    StepButton m_stepButton;
    ModeButton m_modeButton;
    juce::Slider m_valueSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> m_stepAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> m_valueAttachment;
};

class StepGrid : public juce::Component
{
public:
    explicit StepGrid(juce::AudioProcessorValueTreeState& apvts);

    std::function<void(const juce::String&)> onInfo;

    // which pattern the grid shows and edits, and which parameter the small sliders show
    void setEditPattern(int patternIndex);
    void setPage(GridPage page);

    // the selected step is the one MIDI input and the cursor keys work on
    int getSelectedStep() const;
    void setSelectedStep(int stepIndex);
    void moveSelectedStep(int numSteps, int patternLength);

    // called by the GUI timer: framedStep is the step with the yellow frame (-1: none)
    void update(int framedStep, juce::uint32 invalidStepMask, int patternLength);
    void resized() override;

private:
    int m_selectedStep = 0;
    int m_editPattern = 0;
    GridPage m_page = GridPage::Note;
    std::array<std::unique_ptr<StepColumn>, kMaxSteps> m_columns;
};
} // namespace dfd
