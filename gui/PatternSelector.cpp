#include "PatternSelector.h"

namespace dfd
{
namespace
{
constexpr float kCornerSize = 4.0f;
constexpr float kOffMix = 0.10f;
constexpr float kBrighterAmount = 0.35f;
constexpr float kFrameWidth = 2.0f;
constexpr float kHoverAmount = 0.12f;
constexpr float kLabelWidthFraction = 0.22f;
constexpr float kGap = 3.0f;
constexpr float kLabelFontFactor = 0.62f;
constexpr float kLabelAlpha = 0.7f;
const juce::Colour kPlayingFrameColour(0xFFF2D43C); // the same yellow as the frame of the playing step
} // namespace

// ---------------------------------------------------------------------------------------------- PatternButton
PatternButton::PatternButton(const juce::String& buttonText) : juce::Button(buttonText)
{
}

void PatternButton::setLit(bool shouldBeLit, bool brighter)
{
    if (m_lit != shouldBeLit || m_brighter != brighter)
    {
        m_lit = shouldBeLit;
        m_brighter = brighter;
        repaint();
    }
}

void PatternButton::setFollowsToggleState(bool shouldFollow)
{
    m_followsToggleState = shouldFollow;
    repaint();
}

void PatternButton::setPlayingFrame(bool shouldShowFrame)
{
    if (m_playingFrame != shouldShowFrame)
    {
        m_playingFrame = shouldShowFrame;
        repaint();
    }
}

void PatternButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    const juce::Colour textColour = getLookAndFeel().findColour(juce::Label::textColourId);
    const juce::Colour accent = getLookAndFeel().findColour(juce::Slider::rotarySliderFillColourId);

    juce::Colour fill = background.interpolatedWith(textColour, kOffMix);
    bool lit = m_lit;
    if (m_followsToggleState)
    {
        lit = getToggleState();
    }
    if (lit)
    {
        fill = accent;
        if (m_brighter)
        {
            fill = accent.brighter(kBrighterAmount);
        }
    }
    if (shouldDrawButtonAsHighlighted)
    {
        fill = fill.brighter(kHoverAmount);
    }
    if (shouldDrawButtonAsDown)
    {
        fill = fill.darker(kHoverAmount);
    }
    g.setColour(fill);
    g.fillRoundedRectangle(bounds, kCornerSize);
    g.setColour(textColour);
    g.setFont(bounds.getHeight() * kLabelFontFactor);
    g.drawText(getButtonText(), getLocalBounds(), juce::Justification::centred);
    if (m_playingFrame)
    {
        g.setColour(kPlayingFrameColour);
        g.drawRoundedRectangle(bounds, kCornerSize, kFrameWidth);
    }
}

// ---------------------------------------------------------------------------------------------- PatternSelector
PatternSelector::PatternSelector(PatternData& data) : m_data(data)
{
    for (int index = 0; index < kNumPatterns; ++index)
    {
        const juce::String number(index + 1);
        auto playButton = std::make_unique<PatternButton>(number);
        playButton->setClickingTogglesState(true);
        playButton->setTooltip("Play pattern " + number + ": the patterns that are on are played one after the other, from 1 to 4.");
        playButton->setFollowsToggleState(true);
        PatternButton* playButtonPointer = playButton.get();
        playButton->onClick = [this, index, playButtonPointer]()
        {
            m_data.setPlaying(index, playButtonPointer->getToggleState());
            if (onPlaySwitchesEdited)
            {
                onPlaySwitchesEdited();
            }
        };
        addAndMakeVisible(*playButton);
        m_playButtons[static_cast<std::size_t>(index)] = std::move(playButton);

        auto editButton = std::make_unique<PatternButton>(number);
        editButton->setTooltip("Edit pattern " + number + ": the step grid shows this pattern. A yellow frame marks the pattern that is playing.");
        editButton->onClick = [this, index]() { setEditPatternValue(index); };
        addAndMakeVisible(*editButton);
        m_editButtons[static_cast<std::size_t>(index)] = std::move(editButton);
    }
    refresh();
    m_editButtons[static_cast<std::size_t>(m_editPattern)]->setLit(true, true);
}

void PatternSelector::refresh()
{
    for (int index = 0; index < kNumPatterns; ++index)
    {
        m_playButtons[static_cast<std::size_t>(index)]->setToggleState(m_data.isPlaying(index), juce::dontSendNotification);
    }
}

int PatternSelector::getEditPattern() const
{
    return m_editPattern;
}

void PatternSelector::setEditPatternValue(int patternIndex)
{
    m_editPattern = juce::jlimit(0, kNumPatterns - 1, patternIndex);
    for (int index = 0; index < kNumPatterns; ++index)
    {
        m_editButtons[static_cast<std::size_t>(index)]->setLit(index == m_editPattern, true);
    }
    if (onEditPatternChanged)
    {
        onEditPatternChanged(m_editPattern);
    }
}

void PatternSelector::setPlayingPattern(int patternIndex)
{
    if (patternIndex == m_playingPattern)
    {
        return;
    }
    m_playingPattern = patternIndex;
    for (int index = 0; index < kNumPatterns; ++index)
    {
        m_editButtons[static_cast<std::size_t>(index)]->setPlayingFrame(index == m_playingPattern);
    }
}

void PatternSelector::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const int rowHeight = bounds.getHeight() / 2;
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId).withAlpha(kLabelAlpha));
    g.setFont(static_cast<float>(rowHeight) * kLabelFontFactor);
    const int labelWidth = static_cast<int>(static_cast<float>(bounds.getWidth()) * kLabelWidthFraction);
    g.drawText("Play", 0, 0, labelWidth, rowHeight, juce::Justification::centredLeft);
    g.drawText("Edit", 0, rowHeight, labelWidth, rowHeight, juce::Justification::centredLeft);
}

void PatternSelector::resized()
{
    const auto bounds = getLocalBounds();
    const int rowHeight = bounds.getHeight() / 2;
    const int labelWidth = static_cast<int>(static_cast<float>(bounds.getWidth()) * kLabelWidthFraction);
    const int buttonWidth = (bounds.getWidth() - labelWidth) / kNumPatterns;
    for (int index = 0; index < kNumPatterns; ++index)
    {
        const int left = labelWidth + index * buttonWidth;
        const int gap = static_cast<int>(kGap) / 2;
        m_playButtons[static_cast<std::size_t>(index)]->setBounds(left + gap, 0, buttonWidth - 2 * gap, rowHeight - gap);
        m_editButtons[static_cast<std::size_t>(index)]->setBounds(left + gap, rowHeight + gap, buttonWidth - 2 * gap, rowHeight - gap);
    }
}
} // namespace dfd
