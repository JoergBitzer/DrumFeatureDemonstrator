/**
 * @file StepParameters.h
 * @brief the 4 parameters of every step (active, mode, pitch, velocity): ids and specs
 *
 * The spec has the same members as the parameter structs in DrumFeatureDemonstrator.h, so jade::makeParameter()
 * accepts it. Pure C++ (no JUCE types).
 */
#pragma once

#include <string>

namespace dfd
{
enum class StepParam
{
    Active,
    Mode,
    Pitch,
    Velocity
};

constexpr int kNumParamsPerStep = 4;
constexpr float kMaxPitchSemitones = 24.0f;
constexpr float kModeForward = 0.0f;
constexpr float kModeReverse = 1.0f;
constexpr float kModeReverseToEnd = 2.0f;

struct ParameterSpec
{
    std::string ID;
    std::string name;
    std::string unitName;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    int numDecimalPlaces = 0;
    bool logFrequency = false;
    std::string help;
};

// ids like "Step01Active", "Step01Mode", "Step01Pitch", "Step01Velocity" (steps are counted from 1 in the id)
std::string stepParameterId(int stepIndex, StepParam param);
ParameterSpec stepParameterSpec(int stepIndex, StepParam param);
} // namespace dfd
