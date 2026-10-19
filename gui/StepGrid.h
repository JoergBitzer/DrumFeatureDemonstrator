/**
 * @file StepGrid.h
 * @brief the step grid: per step one button with four states (off / forward / reverse / reverse + play to end) and, below the buttons,
 *        a strip of vertical bars (note, velocity or pan of every step) that you can draw on with the mouse
 */
#pragma once

#include <array>
#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../src/PatternData.h"
#include "../src/StepScheduler.h"

namespace dfd
{
// what the bars under the steps show (the "pages" of the grid)
enum class GridPage
{
    Note,
    Velocity,
    Pan
};

// one button for the state of a step: off -> forward -> reverse -> reverse + play to end -> off (it sets the values "Active" and "Mode" of the PatternData)
class StepStateButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int kStateOff = 0;
    static constexpr int kStateForward = 1;
    static constexpr int kStateReverse = 2;
    static constexpr int kStateReverseToEnd = 3;
    static constexpr int kNumStates = 4;

    StepStateButton();

    void bind(PatternData& data, int patternIndex, int stepIndex);
    void refresh();                        // reads the values again (after a preset was loaded)
    void setCurrent(bool isCurrentStep);   // the yellow frame
    void setBeatAccent(bool isOnBeat);
    void setInvalid(bool isInvalid);       // play to end is set, but the sample is longer than the pattern
    int getState() const;
    static juce::String stateName(int state);

    std::function<void()> onStateChangedByUser;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    PatternData* m_data = nullptr;
    int m_patternIndex = 0;
    int m_stepIndex = 0;
    float m_active = 0.0f;
    float m_mode = 0.0f;
    bool m_current = false;
    bool m_beatAccent = false;
    bool m_invalid = false;
};

// step number and state button of one step
class StepColumn : public juce::Component
{
public:
    StepColumn(int stepIndex, PatternData& data);

    std::function<void(int)> onSelected;                 // the user clicked into this column
    std::function<void()> onEdited;                      // the user changed the state of the step
    std::function<void(const juce::String&)> onInfo;     // text for the info line

    void bind(int patternIndex);
    void refresh();
    void setCurrent(bool isCurrentStep);
    void setInvalid(bool isInvalid);
    void setInsidePattern(bool isInside);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;

private:
    void reportState();

    int m_stepIndex;
    int m_patternIndex = 0;
    PatternData& m_data;
    StepStateButton m_stateButton;
};

// vertical bars side by side, one per step: a mouse drag over the strip sets every bar it crosses (draw a line from left to right
// and all the values follow it); a double-click puts a bar back to its default
class ValueStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    ValueStrip(PatternData& data, int firstStep, int numColumns);

    std::function<void(const juce::String&)> onInfo;
    std::function<void()> onEdited;                      // the user changed bars

    void bind(int patternIndex, GridPage page);
    void refresh();                                      // reads the values again (after a preset was loaded)
    void setPatternLength(int length);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

private:
    int columnAt(float x) const;
    float valueAtY(float y) const;
    void setColumnValue(int column, float value);
    void notifyEdited();
    void drawBar(juce::Graphics& g, int column) const;
    juce::String describe(int column) const;
    StepParam pageParameter() const;

    PatternData& m_data;
    int m_firstStep;
    int m_numColumns;
    int m_patternIndex = 0;
    int m_patternLength = kDefaultSteps;
    GridPage m_page = GridPage::Note;
    juce::NormalisableRange<float> m_range;
    float m_defaultValue = 0.0f;
    std::array<float, kMaxSteps> m_values{};
    juce::Point<float> m_lastPosition;
    bool m_dragging = false;
};

class StepGrid : public juce::Component
{
public:
    explicit StepGrid(PatternData& data);

    std::function<void(const juce::String&)> onInfo;
    std::function<void()> onPatternDataEdited;           // the user changed a step or a bar

    // reads all values again (after a preset was loaded or a MIDI note was entered)
    void refreshFromData();

    // which pattern the grid shows and edits, and which parameter the bars show
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
    static constexpr int kStepsPerRow = 16;
    static constexpr int kNumRows = kMaxSteps / kStepsPerRow;

    int m_selectedStep = 0;
    int m_editPattern = 0;
    GridPage m_page = GridPage::Note;
    std::array<std::unique_ptr<StepColumn>, kMaxSteps> m_columns;
    std::array<std::unique_ptr<ValueStrip>, kNumRows> m_strips;
};
} // namespace dfd
