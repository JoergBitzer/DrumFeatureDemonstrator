#include "StepParameters.h"

namespace dfd
{
namespace
{
constexpr int kDigitsInStepNumber = 2;
const char* const kActiveSuffix = "Active";
const char* const kModeSuffix = "Mode";
const char* const kPitchSuffix = "Pitch";
const char* const kVelocitySuffix = "Velocity";

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
    case StepParam::Pitch:
        return kPitchSuffix;
    case StepParam::Velocity:
        return kVelocitySuffix;
    }
    return kActiveSuffix;
}
} // namespace

std::string stepParameterId(int stepIndex, StepParam param)
{
    return "Step" + stepNumberText(stepIndex) + suffixOf(param);
}

ParameterSpec stepParameterSpec(int stepIndex, StepParam param)
{
    const std::string stepName = "Step " + stepNumberText(stepIndex);
    ParameterSpec spec;
    spec.ID = stepParameterId(stepIndex, param);
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
    case StepParam::Pitch:
        spec.name = stepName + " pitch";
        spec.unitName = "st";
        spec.minValue = -kMaxPitchSemitones;
        spec.maxValue = kMaxPitchSemitones;
        spec.defaultValue = 0.0f;
        spec.numDecimalPlaces = 1;
        spec.help = "Pitch of this step in semitones, relative to the base note.";
        break;
    case StepParam::Velocity:
        spec.name = stepName + " velocity";
        spec.minValue = 0.0f;
        spec.maxValue = 1.0f;
        spec.defaultValue = 1.0f;
        spec.numDecimalPlaces = 2;
        spec.help = "Gain of this step (0 - 1).";
        break;
    }
    return spec;
}
} // namespace dfd
