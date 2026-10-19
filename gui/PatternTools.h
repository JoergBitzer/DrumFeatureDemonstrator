/**
 * @file PatternTools.h
 * @brief a row of small icon buttons for the edited pattern: copy, paste, clear, shift left, shift right, undo, pattern library
 *
 * The icons are drawn as paths (no text, no symbols from a font). The buttons only call back; DrumFeatureDemonstratorGUI does the work
 * on the PatternData. The library button opens a menu with the categories (BD, SD, HH, Perc, Chromatic) and their patterns.
 */
#pragma once

#include <array>
#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

namespace dfd
{
enum class ToolIcon
{
    Copy,
    Paste,
    Clear,
    ShiftLeft,
    ShiftRight,
    Undo,
    Library
};

class IconButton : public juce::Button
{
public:
    explicit IconButton(ToolIcon icon);

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    ToolIcon m_icon;
};

class PatternTools : public juce::Component
{
public:
    PatternTools();

    std::function<void()> onCopy;
    std::function<void()> onPaste;
    std::function<void()> onClear;
    std::function<void()> onShiftLeft;
    std::function<void()> onShiftRight;
    std::function<void()> onUndo;
    std::function<void(int)> onLibraryEntryChosen; // index in the pattern library

    void setPasteEnabled(bool isEnabled);  // false while nothing was copied
    void setUndoEnabled(bool isEnabled);   // false while there is nothing to undo

    void resized() override;

private:
    void showLibraryMenu();

    std::array<std::unique_ptr<IconButton>, 7> m_buttons;
    IconButton* m_pasteButton = nullptr;
    IconButton* m_undoButton = nullptr;
};
} // namespace dfd
