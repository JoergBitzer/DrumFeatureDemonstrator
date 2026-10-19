/**
 * @file StepValues.h
 * @brief the 5 values of every step (active, mode, note, velocity, pan): ranges, defaults and help texts
 *
 * The values are not host parameters (see PatternData.h); this header only describes them. Pure C++ (no JUCE types).
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

constexpr int kNumValuesPerStep = 5;
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

struct StepValueSpec
{
    std::string name;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
    int numDecimalPlaces = 0;
    std::string help;
};

StepValueSpec stepValueSpec(StepParam param);
} // namespace dfd
