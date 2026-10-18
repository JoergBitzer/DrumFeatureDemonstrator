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
    Note,
    Velocity,
    Pan
};

constexpr int kNumParamsPerStep = 5;
constexpr float kMinStepNote = 12.0f;  // C0
constexpr float kMaxStepNote = 108.0f; // C8
constexpr float kDefaultStepNote = 60.0f; // C4
constexpr float kMinVelocity = 1.0f;
constexpr float kMaxVelocity = 127.0f;
constexpr float kDefaultVelocity = 100.0f;
constexpr float kMinPan = -1.0f; // hard left
constexpr float kMaxPan = 1.0f;  // hard right
constexpr float kCentrePan = 0.0f;
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

// ids like "Step01Active", "Step01Mode", "Step01Note", "Step01Velocity", "Step01Pan" (steps are counted from 1 in the id)
std::string stepParameterId(int stepIndex, StepParam param);
ParameterSpec stepParameterSpec(int stepIndex, StepParam param);
} // namespace dfd
