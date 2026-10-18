/**
 * @file PatternSelector.h
 * @brief two rows of four small buttons for the four patterns
 *
 * Upper row ("Play"): switches each pattern on or off for playback; the patterns that are on are played from left to right.
 * Lower row ("Edit"): selects the pattern that the step grid shows; the edited pattern is lit in a brighter red, and a yellow
 * frame marks the pattern that is playing right now.
 */
#pragma once

#include <array>
#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../src/StepScheduler.h"

namespace dfd
{
// a small button with a number: lit (on / edited), optionally brighter, and optionally with the yellow "playing" frame
class PatternButton : public juce::Button
{
public:
    explicit PatternButton(const juce::String& buttonText);

    void setLit(bool shouldBeLit, bool brighter);
    void setFollowsToggleState(bool shouldFollow); // lit = the toggle state of the button (the play buttons)
    void setPlayingFrame(bool shouldShowFrame);
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    bool m_followsToggleState = false;
    bool m_lit = false;
    bool m_brighter = false;
    bool m_playingFrame = false;
};

class PatternSelector : public juce::Component
{
public:
    explicit PatternSelector(juce::AudioProcessorValueTreeState& apvts);

    int getEditPattern() const; // 0 - 3
    std::function<void(int)> onEditPatternChanged;

    // called by the GUI timer: the pattern that plays now, -1 if none
    void setPlayingPattern(int patternIndex);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void setEditPatternValue(int patternIndex);

    juce::ParameterAttachment m_editAttachment;
    std::array<std::unique_ptr<PatternButton>, kNumPatterns> m_playButtons;
    std::array<std::unique_ptr<PatternButton>, kNumPatterns> m_editButtons;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, kNumPatterns> m_playAttachments;
    int m_editPattern = 0;
    int m_playingPattern = -1;
};
} // namespace dfd
