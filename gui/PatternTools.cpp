#include "PatternTools.h"

#include "../src/PatternLibrary.h"

namespace dfd
{
namespace
{
constexpr float kCornerSize = 4.0f;
constexpr float kOffMix = 0.10f;
constexpr float kHoverAmount = 0.12f;
constexpr float kDisabledAlpha = 0.35f;
constexpr float kPadding = 0.22f;       // of the button size: space around the icon
constexpr float kLineWidth = 0.09f;     // of the icon size
constexpr float kGapFraction = 0.12f;   // of the button size: space between two buttons
constexpr float kHeightFraction = 0.62f; // of the height of the tools area
constexpr float kPi = 3.14159265f;
constexpr float kHalf = 0.5f;
constexpr int kLibraryMenuIdOffset = 1;

// all coordinates are fractions (0 - 1) of the icon square
juce::Rectangle<float> rectOf(const juce::Rectangle<float>& icon, float x, float y, float width, float height)
{
    return {icon.getX() + x * icon.getWidth(), icon.getY() + y * icon.getHeight(), width * icon.getWidth(), height * icon.getHeight()};
}

juce::Point<float> pointOf(const juce::Rectangle<float>& icon, float x, float y)
{
    return {icon.getX() + x * icon.getWidth(), icon.getY() + y * icon.getHeight()};
}
} // namespace

IconButton::IconButton(ToolIcon icon) : juce::Button({}), m_icon(icon)
{
}

void IconButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const juce::Colour background = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    const juce::Colour textColour = getLookAndFeel().findColour(juce::Label::textColourId);

    juce::Colour fill = background.interpolatedWith(textColour, kOffMix);
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

    float alpha = 1.0f;
    if (isEnabled() == false)
    {
        alpha = kDisabledAlpha;
    }
    g.setColour(textColour.withAlpha(alpha));
    const float padding = kPadding * bounds.getWidth();
    const juce::Rectangle<float> icon = bounds.reduced(padding);
    const float lineWidth = kLineWidth * icon.getWidth() * 2.0f;
    const juce::PathStrokeType stroke(lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path path;

    switch (m_icon)
    {
    case ToolIcon::Copy:
        // two sheets, the front one in front of the back one
        path.addRoundedRectangle(rectOf(icon, 0.0f, 0.0f, 0.62f, 0.68f), lineWidth);
        g.strokePath(path, stroke);
        g.setColour(fill);
        g.fillRoundedRectangle(rectOf(icon, 0.34f, 0.30f, 0.66f, 0.70f), lineWidth);
        g.setColour(textColour.withAlpha(alpha));
        path.clear();
        path.addRoundedRectangle(rectOf(icon, 0.34f, 0.30f, 0.66f, 0.70f), lineWidth);
        g.strokePath(path, stroke);
        break;
    case ToolIcon::Paste:
        // a board with a clip and two lines of text
        path.addRoundedRectangle(rectOf(icon, 0.06f, 0.14f, 0.88f, 0.86f), lineWidth);
        g.strokePath(path, stroke);
        g.fillRoundedRectangle(rectOf(icon, 0.30f, 0.0f, 0.40f, 0.28f), lineWidth);
        g.fillRect(rectOf(icon, 0.24f, 0.50f, 0.52f, 0.09f));
        g.fillRect(rectOf(icon, 0.24f, 0.72f, 0.52f, 0.09f));
        break;
    case ToolIcon::Clear:
        // a cross
        path.startNewSubPath(pointOf(icon, 0.1f, 0.1f));
        path.lineTo(pointOf(icon, 0.9f, 0.9f));
        path.startNewSubPath(pointOf(icon, 0.9f, 0.1f));
        path.lineTo(pointOf(icon, 0.1f, 0.9f));
        g.strokePath(path, stroke);
        break;
    case ToolIcon::ShiftLeft:
        path.addTriangle(pointOf(icon, 0.0f, 0.5f), pointOf(icon, 0.45f, 0.1f), pointOf(icon, 0.45f, 0.9f));
        g.fillPath(path);
        g.fillRect(rectOf(icon, 0.40f, 0.38f, 0.60f, 0.24f));
        break;
    case ToolIcon::ShiftRight:
        path.addTriangle(pointOf(icon, 1.0f, 0.5f), pointOf(icon, 0.55f, 0.1f), pointOf(icon, 0.55f, 0.9f));
        g.fillPath(path);
        g.fillRect(rectOf(icon, 0.0f, 0.38f, 0.60f, 0.24f));
        break;
    case ToolIcon::Undo:
        // an arc with an arrow head at its left end
        path.addCentredArc(icon.getCentreX(), icon.getCentreY() + 0.08f * icon.getHeight(), 0.36f * icon.getWidth(), 0.34f * icon.getHeight(), 0.0f,
                           -kPi * kHalf, kPi * 1.0f, true);
        g.strokePath(path, stroke);
        path.clear();
        path.addTriangle(pointOf(icon, 0.0f, 0.12f), pointOf(icon, 0.52f, 0.0f), pointOf(icon, 0.52f, 0.30f));
        g.fillPath(path);
        break;
    case ToolIcon::Library:
        // a list: three lines with a dot in front of each
        for (int row = 0; row < 3; ++row)
        {
            const float y = 0.04f + 0.36f * static_cast<float>(row);
            g.fillRect(rectOf(icon, 0.0f, y, 0.16f, 0.16f));
            g.fillRect(rectOf(icon, 0.30f, y, 0.70f, 0.16f));
        }
        break;
    }
}

PatternTools::PatternTools()
{
    const ToolIcon icons[] = {ToolIcon::Copy, ToolIcon::Paste, ToolIcon::Clear, ToolIcon::ShiftLeft, ToolIcon::ShiftRight, ToolIcon::Undo, ToolIcon::Library};
    const char* const tips[] = {"Copy the edited pattern.",
                                "Paste the copied pattern into the edited pattern.",
                                "Clear the edited pattern (all steps off).",
                                "Shift the edited pattern one step to the left (steps that leave come back at the end).",
                                "Shift the edited pattern one step to the right (steps that leave come back at the start).",
                                "Undo the last of these actions (copy excepted).",
                                "Pattern library: typical patterns for the edited pattern (BD, SD, HH, Perc, Chromatic)."};
    for (std::size_t index = 0; index < m_buttons.size(); ++index)
    {
        m_buttons[index] = std::make_unique<IconButton>(icons[index]);
        m_buttons[index]->setTooltip(tips[index]);
        addAndMakeVisible(*m_buttons[index]);
    }
    m_pasteButton = m_buttons[1].get();
    m_undoButton = m_buttons[5].get();
    m_buttons[0]->onClick = [this]() { if (onCopy) onCopy(); };
    m_buttons[1]->onClick = [this]() { if (onPaste) onPaste(); };
    m_buttons[2]->onClick = [this]() { if (onClear) onClear(); };
    m_buttons[3]->onClick = [this]() { if (onShiftLeft) onShiftLeft(); };
    m_buttons[4]->onClick = [this]() { if (onShiftRight) onShiftRight(); };
    m_buttons[5]->onClick = [this]() { if (onUndo) onUndo(); };
    m_buttons[6]->onClick = [this]() { showLibraryMenu(); };
    setPasteEnabled(false);
    setUndoEnabled(false);
}

void PatternTools::setPasteEnabled(bool isEnabled)
{
    m_pasteButton->setEnabled(isEnabled);
}

void PatternTools::setUndoEnabled(bool isEnabled)
{
    m_undoButton->setEnabled(isEnabled);
}

void PatternTools::showLibraryMenu()
{
    juce::PopupMenu menu;
    for (int categoryIndex = 0; categoryIndex < numLibraryCategories(); ++categoryIndex)
    {
        juce::PopupMenu categoryMenu;
        for (int entryIndex = 0; entryIndex < numLibraryEntries(); ++entryIndex)
        {
            const LibraryEntry& entry = libraryEntry(entryIndex);
            if (juce::String(entry.category) == juce::String(libraryCategoryName(categoryIndex)))
            {
                categoryMenu.addItem(kLibraryMenuIdOffset + entryIndex, entry.name);
            }
        }
        menu.addSubMenu(libraryCategoryName(categoryIndex), categoryMenu);
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(m_buttons[6].get()),
                       [this](int result)
                       {
                           if (result >= kLibraryMenuIdOffset && onLibraryEntryChosen)
                           {
                               onLibraryEntryChosen(result - kLibraryMenuIdOffset);
                           }
                       });
}

void PatternTools::resized()
{
    const auto area = getLocalBounds();
    const int size = static_cast<int>(kHeightFraction * static_cast<float>(area.getHeight()));
    const int gap = static_cast<int>(kGapFraction * static_cast<float>(size));
    const int top = area.getCentreY() - size / 2;
    int left = area.getX();
    for (auto& button : m_buttons)
    {
        button->setBounds(left, top, size, size);
        left += size + gap;
    }
}
} // namespace dfd
