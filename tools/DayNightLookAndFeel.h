/*
    DayNightLookAndFeel.h
    Author: J. Bitzer @ TGM, Jade Hochschule
    Date: 2026-09-30
    Description: Day/night theme for the plugin GUI (switched on with WITH_DAYNIGHT in CMakeLists.txt).
    - DayNightLookAndFeel: colours for all standard JUCE components (sliders, buttons, combo boxes,
      labels, popup menus, background) in a day (light) and a night (dark) version, and a knob
      in the Jade style (grey disc, red pointer).
    - ThemeButton: the small button in the preset bar that switches the theme. It shows the icon of
      the theme a click switches to (a sun in night mode, a moon in day mode), drawn as a path, not
      as a text symbol (some Windows fonts lack the Unicode sun/moon).
    - createUserSettings(): the per-user settings file (in the preset folder) where the choice is stored.

    Your own components follow the theme if they take their colours from the LookAndFeel, e.g.
    g.setColour(getLookAndFeel().findColour(juce::Label::textColourId)) -- see DrumFeatureDemonstratorGUI::paint().
    License: MIT
*/
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_data_structures/juce_data_structures.h>

namespace jade
{
class DayNightLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum class Theme { Day, Night };

    explicit DayNightLookAndFeel(Theme theme = Theme::Night);

    void setTheme(Theme newTheme);
    Theme getTheme() const noexcept { return m_theme; }

    // bar sliders (juce::Slider::LinearBar): with a border and a handle, so the lowest value is visible, too
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float minSliderPos,
                          float maxSliderPos, juce::Slider::SliderStyle style, juce::Slider& slider) override;
    void drawSmallHorizontalSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos, juce::Slider& slider);
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override;

private:
    Theme m_theme;
};

class ThemeButton : public juce::Button
{
public:
    ThemeButton() : juce::Button("Theme") {}

    // the theme that is active now; the button shows the other one
    void setCurrentTheme(DayNightLookAndFeel::Theme theme);

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    bool m_showSun = true; // night is the default, so the button offers the day theme
};

// Per-user settings file "user.settings" (juce::PropertiesFile, XML) in the given folder, shared by
// all instances. The editor uses the preset folder (PresetHandler::getUserPresetsFolder()), so
// presets and settings are in the same place. Not ".xml": the preset handler loads every .xml
// file in that folder as a preset.
std::unique_ptr<juce::PropertiesFile> createUserSettings(const juce::File& folder);
} // namespace jade
