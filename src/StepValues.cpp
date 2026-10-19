#include "StepValues.h"

#include "NoteValues.h"
#include "StepScheduler.h"

namespace dfd
{
StepValueSpec stepValueSpec(StepParam param)
{
    StepValueSpec spec;
    switch (param)
    {
    case StepParam::Active:
        spec.name = "On";
        spec.minValue = 0.0f;
        spec.maxValue = 1.0f;
        spec.defaultValue = 0.0f;
        spec.help = "Plays the sample on this step (1) or not (0).";
        break;
    case StepParam::Mode:
        spec.name = "Mode";
        spec.minValue = kModeForward;
        spec.maxValue = kModeReverseToEnd;
        spec.defaultValue = kModeForward;
        spec.help = "0 = forward, 1 = reverse, 2 = reverse and play to end (the sample ends exactly at the end of this step).";
        break;
    case StepParam::Note:
        spec.name = "Note";
        spec.minValue = kMinStepNote;
        spec.maxValue = kMaxStepNote;
        spec.defaultValue = kDefaultStepNote;
        spec.numDecimalPlaces = 0;
        spec.help = "MIDI note of this step (60 = C4). The sample sounds at its original pitch when the note equals the base note.";
        break;
    case StepParam::Velocity:
        spec.name = "Velocity";
        spec.minValue = kMinVelocity;
        spec.maxValue = kMaxVelocity;
        spec.defaultValue = kDefaultVelocity;
        spec.numDecimalPlaces = 0;
        spec.help = "MIDI velocity of this step (1 - 127); the volume is velocity / 127.";
        break;
    case StepParam::Pan:
        spec.name = "Pan";
        spec.minValue = kMinPan;
        spec.maxValue = kMaxPan;
        spec.defaultValue = kCentrePan;
        spec.numDecimalPlaces = 2;
        spec.help = "Pan of this step: -1 = left, 0 = centre, 1 = right (balance: the centre keeps the full level of both channels).";
        break;
    }
    return spec;
}

PatternSettingSpec patternSettingSpec(PatternSetting setting)
{
    PatternSettingSpec spec;
    switch (setting)
    {
    case PatternSetting::Length:
        spec.name = "Pattern length";
        spec.minValue = static_cast<float>(kMinSteps);
        spec.maxValue = static_cast<float>(kMaxSteps);
        spec.defaultValue = static_cast<float>(kDefaultSteps);
        spec.help = "Number of steps of the pattern (8 - 32); the same for all four patterns.";
        break;
    case PatternSetting::NoteValue:
        spec.name = "Step note value";
        spec.minValue = 0.0f;
        spec.maxValue = static_cast<float>(kNumNoteValues - 1);
        spec.defaultValue = static_cast<float>(kDefaultNoteValueIndex);
        spec.help = "The note value that one step of the pattern represents: 1/1 to 1/64, each normal, dotted or as a triplet.";
        break;
    case PatternSetting::SyncToHost:
        spec.name = "Sync to host";
        spec.minValue = 0.0f;
        spec.maxValue = 1.0f;
        spec.defaultValue = 1.0f;
        spec.help = "1 = follow the tempo and position of the DAW, 0 = internal tempo and play button.";
        break;
    case PatternSetting::Declick:
        spec.name = "Declick";
        spec.minValue = 0.0f;
        spec.maxValue = 1.0f;
        spec.defaultValue = 0.0f;
        spec.help = "If on, the start and end handles snap to a zero crossing of the sample when you move them (no click at the region edges).";
        break;
    }
    return spec;
}
} // namespace dfd
