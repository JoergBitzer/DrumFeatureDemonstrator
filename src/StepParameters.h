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
constexpr int kNumPatternsInParameters = 4; // the same as dfd::kNumPatterns in StepScheduler.h (that header is not included here)
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

// ids like "Step01Active", "Step01Mode", "Step01Note", "Step01Velocity", "Step01Pan" for the first pattern (steps are counted
// from 1 in the id); the other patterns have a prefix: "P2Step01Active", "P3Step01Active", "P4Step01Active"
std::string stepParameterId(int patternIndex, int stepIndex, StepParam param);
ParameterSpec stepParameterSpec(int patternIndex, int stepIndex, StepParam param);

// which patterns are played ("PlayPattern1" ... "PlayPattern4": 1 = on) and which one is edited ("EditPattern": 1 - 4)
std::string playPatternParameterId(int patternIndex);
ParameterSpec playPatternParameterSpec(int patternIndex);
std::string editPatternParameterId();
ParameterSpec editPatternParameterSpec();
} // namespace dfd
