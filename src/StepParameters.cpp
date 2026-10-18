#include "StepParameters.h"

namespace dfd
{
namespace
{
constexpr int kDigitsInStepNumber = 2;
const char* const kActiveSuffix = "Active";
const char* const kModeSuffix = "Mode";
const char* const kNoteSuffix = "Note";
const char* const kVelocitySuffix = "Velocity";
const char* const kPanSuffix = "Pan";

std::string stepNumberText(int stepIndex)
{
    std::string number = std::to_string(stepIndex + 1);
    while (static_cast<int>(number.size()) < kDigitsInStepNumber)
    {
        number = "0" + number;
    }
    return number;
}

const char* suffixOf(StepParam param)
{
    switch (param)
    {
    case StepParam::Active:
        return kActiveSuffix;
    case StepParam::Mode:
        return kModeSuffix;
    case StepParam::Note:
        return kNoteSuffix;
    case StepParam::Velocity:
        return kVelocitySuffix;
    case StepParam::Pan:
        return kPanSuffix;
    }
    return kActiveSuffix;
}
} // namespace

std::string stepParameterId(int patternIndex, int stepIndex, StepParam param)
{
    std::string prefix;
    if (patternIndex > 0)
    {
        prefix = "P" + std::to_string(patternIndex + 1);
    }
    return prefix + "Step" + stepNumberText(stepIndex) + suffixOf(param);
}

ParameterSpec stepParameterSpec(int patternIndex, int stepIndex, StepParam param)
{
    const std::string stepName = "Pattern " + std::to_string(patternIndex + 1) + " step " + stepNumberText(stepIndex);
    ParameterSpec spec;
    spec.ID = stepParameterId(patternIndex, stepIndex, param);
    switch (param)
    {
    case StepParam::Active:
        spec.name = stepName + " on";
        spec.minValue = 0.0f;
        spec.maxValue = 1.0f;
        spec.defaultValue = 0.0f;
        spec.help = "Plays the sample on this step (1) or not (0).";
        break;
    case StepParam::Mode:
        spec.name = stepName + " mode";
        spec.minValue = kModeForward;
        spec.maxValue = kModeReverseToEnd;
        spec.defaultValue = kModeForward;
        spec.help = "0 = forward, 1 = reverse, 2 = reverse and play to end (the sample ends exactly at the end of this step).";
        break;
    case StepParam::Note:
        spec.name = stepName + " note";
        spec.minValue = kMinStepNote;
        spec.maxValue = kMaxStepNote;
        spec.defaultValue = kDefaultStepNote;
        spec.numDecimalPlaces = 0;
        spec.help = "MIDI note of this step (60 = C4). The sample sounds at its original pitch when the note equals the base note.";
        break;
    case StepParam::Velocity:
        spec.name = stepName + " velocity";
        spec.minValue = kMinVelocity;
        spec.maxValue = kMaxVelocity;
        spec.defaultValue = kDefaultVelocity;
        spec.numDecimalPlaces = 0;
        spec.help = "MIDI velocity of this step (1 - 127); the volume is velocity / 127.";
        break;
    case StepParam::Pan:
        spec.name = stepName + " pan";
        spec.minValue = kMinPan;
        spec.maxValue = kMaxPan;
        spec.defaultValue = kCentrePan;
        spec.numDecimalPlaces = 2;
        spec.help = "Pan of this step: -1 = left, 0 = centre, 1 = right (balance: the centre keeps the full level of both channels).";
        break;
    }
    return spec;
}

std::string playPatternParameterId(int patternIndex)
{
    return "PlayPattern" + std::to_string(patternIndex + 1);
}

ParameterSpec playPatternParameterSpec(int patternIndex)
{
    ParameterSpec spec;
    spec.ID = playPatternParameterId(patternIndex);
    spec.name = "Play pattern " + std::to_string(patternIndex + 1);
    spec.minValue = 0.0f;
    spec.maxValue = 1.0f;
    spec.defaultValue = 0.0f;
    if (patternIndex == 0)
    {
        spec.defaultValue = 1.0f;
    }
    spec.help = "Plays this pattern (1) or not (0). The patterns that are on are played one after the other, from 1 to 4.";
    return spec;
}

std::string editPatternParameterId()
{
    return "EditPattern";
}

ParameterSpec editPatternParameterSpec()
{
    ParameterSpec spec;
    spec.ID = editPatternParameterId();
    spec.name = "Edit pattern";
    spec.minValue = 1.0f;
    spec.maxValue = static_cast<float>(kNumPatternsInParameters);
    spec.defaultValue = 1.0f;
    spec.help = "The pattern that the step grid shows and edits (1 - 4).";
    return spec;
}
} // namespace dfd
