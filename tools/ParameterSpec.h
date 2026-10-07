/*
    ParameterSpec.h
    Author: J. Bitzer @ TGM, Jade Hochschule
    Date: 2026-09-30
    Description: helpers for the parameter definitions in DrumFeatureDemonstrator.h. Each parameter is
    defined once, in a struct like g_paramExample:

        const struct
        {
            const std::string ID = "ExampleID";
            const std::string name = "Example";
            const std::string unitName = "xyz";
            const float minValue = 1.f;
            const float maxValue = 2.f;
            const float defaultValue = 1.2f;
            const int numDecimalPlaces = 1;   // display precision (also the step size: 0.1)
            const bool logFrequency = false;  // true: logarithmic range for frequencies (whole Hz)
            const std::string help = "What this control does.";
        } g_paramExample;

    and everything else is made from it:
    - makeParameter(g_paramExample): the AudioParameterFloat, with the value shown as text with
      the unit ("1.5 xyz") -- in the DAW, and on your slider (via the attachment)
    - helpText(g_paramExample): "Example (1.0 - 2.0 xyz, default 1.2): What this control does."
      e.g. as tooltip: m_slider.setTooltip(jade::helpText(g_paramExample));
    License: MIT
*/
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <map>
#include "LogFrequencyRange.h"

namespace jade
{
// help texts (the "help" line) of all parameters made with makeParameter(), by parameter ID
// (e.g. to generate the list of controls for a manual)
inline std::map<juce::String, juce::String>& parameterHelpTexts()
{
    static std::map<juce::String, juce::String> texts;
    return texts;
}

template <class Spec>
juce::String valueText(const Spec& p, float value)
{
    const auto text = juce::String(value, p.numDecimalPlaces);
    return p.unitName.empty() ? text : text + " " + juce::String(p.unitName);
}

template <class Spec>
juce::String helpText(const Spec& p)
{
    return juce::String(p.name) + " (" + juce::String(p.minValue, p.numDecimalPlaces) + " - "
         + valueText(p, p.maxValue) + ", default " + valueText(p, p.defaultValue) + "): " + juce::String(p.help);
}

template <class Spec>
std::unique_ptr<juce::AudioParameterFloat> makeParameter(const Spec& p)
{
    const auto range = p.logFrequency
        ? makeLogFrequencyRange(p.minValue, p.maxValue)
        : juce::NormalisableRange<float>(p.minValue, p.maxValue, std::pow(10.0f, (float) -p.numDecimalPlaces));

    parameterHelpTexts()[juce::String(p.ID)] = juce::String(p.help);

    // the unit is part of the text (not a separate label, which some DAWs would show twice)
    return std::make_unique<juce::AudioParameterFloat>(
        juce::String(p.ID), juce::String(p.name), range, p.defaultValue,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([p](float value, int) { return valueText(p, value); })
            .withValueFromStringFunction([](const juce::String& text) { return text.getFloatValue(); }));
}
} // namespace jade
