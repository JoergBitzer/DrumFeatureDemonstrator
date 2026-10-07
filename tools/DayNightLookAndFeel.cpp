/*
    DayNightLookAndFeel.cpp -- see DayNightLookAndFeel.h
    License: MIT
*/
#include "DayNightLookAndFeel.h"

namespace jade
{
namespace
{
    const juce::Colour kRed { 0xffd01818 };  // knob pointer, value arc, highlights
    const juce::Colour kSun { 0xffffc633 };  // theme button icons
    const juce::Colour kMoon { 0xff1f2438 };

    // colour sets of LookAndFeel_V4: they cover all standard JUCE components
    juce::LookAndFeel_V4::ColourScheme nightScheme()
    {
        return { 0xff202225,   // windowBackground
                 0xff34373c,   // widgetBackground
                 0xff2a2c30,   // menuBackground
                 0xff5a5d63,   // outline
                 0xffe8e8e8,   // defaultText
                 0xff45484e,   // defaultFill
                 0xffffffff,   // highlightedText
                 kRed.getARGB(), // highlightedFill
                 0xffe8e8e8 }; // menuText
    }

    juce::LookAndFeel_V4::ColourScheme dayScheme()
    {
        return { 0xffffffff, 0xffe4e4e4, 0xfff4f4f4, 0xffa0a0a0, 0xff202020,
                 0xffcfcfcf, 0xffffffff, kRed.getARGB(), 0xff202020 };
    }
}

DayNightLookAndFeel::DayNightLookAndFeel(Theme theme)
    : m_theme(theme)
{
    setTheme(theme);
}

void DayNightLookAndFeel::setTheme(Theme newTheme)
{
    m_theme = newTheme;
    setColourScheme(m_theme == Theme::Day ? dayScheme() : nightScheme());
    setColour(juce::Slider::rotarySliderFillColourId, kRed);
    setColour(juce::Slider::thumbColourId, kRed);
}

void DayNightLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float arcWidth = juce::jmax(2.0f, 0.12f * radius);
    const float arcRadius = radius - arcWidth / 2.0f;

    // track and value arc
    juce::Path track, value;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, angle, true);
    const juce::PathStrokeType stroke(arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour(slider.findColour(juce::Slider::rotarySliderOutlineColourId));
    g.strokePath(track, stroke);
    g.setColour(slider.findColour(juce::Slider::rotarySliderFillColourId).withAlpha(slider.isEnabled() ? 1.0f : 0.4f));
    g.strokePath(value, stroke);

    // grey disc and pointer
    const float discRadius = radius - 2.0f * arcWidth;
    g.setColour(findColour(juce::ResizableWindow::backgroundColourId).contrasting(0.35f));
    g.fillEllipse(juce::Rectangle<float>(2.0f * discRadius, 2.0f * discRadius).withCentre(centre));
    juce::Path pointer;
    pointer.addRoundedRectangle(-0.5f * arcWidth, -discRadius, arcWidth, 0.6f * discRadius, 0.5f * arcWidth);
    g.setColour(slider.findColour(juce::Slider::thumbColourId));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre));
}

void ThemeButton::setCurrentTheme(DayNightLookAndFeel::Theme theme)
{
    m_showSun = theme == DayNightLookAndFeel::Theme::Night;
    setTitle(m_showSun ? "Switch to day theme" : "Switch to night theme"); // accessibility
    repaint();
}

void ThemeButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto area = getLocalBounds().toFloat().reduced(1.0f);
    auto fill = findColour(juce::TextButton::buttonColourId);
    if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
        fill = fill.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.1f);
    g.setColour(fill);
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(area, 4.0f, 1.0f);

    const float size = juce::jmin(area.getWidth(), area.getHeight());
    const auto centre = area.getCentre();
    if (m_showSun)
    {
        // disc plus eight rays
        g.setColour(kSun);
        const float discRadius = 0.17f * size;
        g.fillEllipse(juce::Rectangle<float>(2.0f * discRadius, 2.0f * discRadius).withCentre(centre));
        juce::Path rays;
        for (int i = 0; i < 8; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
            const auto dir = juce::Point<float>(std::sin(a), -std::cos(a));
            rays.startNewSubPath(centre + dir * (0.26f * size));
            rays.lineTo(centre + dir * (0.38f * size));
        }
        g.strokePath(rays, juce::PathStrokeType(0.07f * size, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        // crescent: a disc with a second, offset disc cut out (clip with even-odd winding)
        g.setColour(kMoon);
        const float radius = 0.30f * size;
        const auto disc = juce::Rectangle<float>(2.0f * radius, 2.0f * radius).withCentre(centre);
        const auto cutOut = juce::Rectangle<float>(1.7f * radius, 1.7f * radius)
                                .withCentre(centre + juce::Point<float>(0.55f * radius, -0.35f * radius));
        juce::Path outsideCutOut;
        outsideCutOut.setUsingNonZeroWinding(false);
        outsideCutOut.addRectangle(getLocalBounds().toFloat());
        outsideCutOut.addEllipse(cutOut);
        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(outsideCutOut);
        g.fillEllipse(disc);
    }
}

std::unique_ptr<juce::PropertiesFile> createUserSettings(const juce::File& folder)
{
    juce::PropertiesFile::Options options;
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    options.millisecondsBeforeSaving = 0; // save immediately when a value changes
    return std::make_unique<juce::PropertiesFile>(folder.getChildFile("user.settings"), options);
}
} // namespace jade
