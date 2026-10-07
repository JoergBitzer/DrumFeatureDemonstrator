/*
    LogFrequencyRange.h
    Author: J. Bitzer @ TGM, Jade Hochschule
    Date: 2026-09-30
    Description: A true logarithmic NormalisableRange for frequency parameters (equal
    frequency ratios get equal knob rotation), e.g. for cutoff or crossover frequencies
    spanning several octaves, where a linear or skewed range would cram the musically
    useful low end into a sliver of the knob. Values snap to whole Hz.

    Usage:
        std::make_unique<juce::AudioParameterFloat>(juce::ParameterID { "cutoff", 1 }, "Cutoff",
            jade::makeLogFrequencyRange(20.0f, 20000.0f), 1000.0f)

    Why the snap function clamps: a custom snapToLegalValue must clamp into the range,
    as JUCE's default one does. Otherwise an out-of-range value (e.g. a slider's stale
    value before its attachment has synced) reaches convertTo0To1's log formula
    unclamped, the result lies outside [0, 1], and NormalisableRange::clampTo0To1
    asserts -- found as an intermittent pluginval crash in StereoWidener.
    License: MIT
*/
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace jade
{
inline juce::NormalisableRange<float> makeLogFrequencyRange(float minHz, float maxHz)
{
    return juce::NormalisableRange<float>(minHz, maxHz,
        [](float rangeStart, float rangeEnd, float normalised) // convertFrom0To1
        {
            return rangeStart * std::pow(rangeEnd / rangeStart, normalised);
        },
        [](float rangeStart, float rangeEnd, float value) // convertTo0To1
        {
            return std::log(value / rangeStart) / std::log(rangeEnd / rangeStart);
        },
        [](float rangeStart, float rangeEnd, float value) // snapToLegalValue: clamp, then whole Hz
        {
            return std::round(juce::jlimit(rangeStart, rangeEnd, value));
        });
}
} // namespace jade
