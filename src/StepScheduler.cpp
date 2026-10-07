#include "StepScheduler.h"

#include <cmath>

namespace dfd
{
int numUsedSteps(const Pattern& pattern)
{
    if (pattern.numSteps < kMinSteps)
    {
        return kMinSteps;
    }
    if (pattern.numSteps > kMaxSteps)
    {
        return kMaxSteps;
    }
    return pattern.numSteps;
}

namespace
{
constexpr int kPassesBefore = 1; // look-ahead: a sound starts at most one pattern pass before its step
constexpr int kPassesAfter = 1;
constexpr double kStepLengthInBeats = 1.0 / kStepsPerBeat;

bool isTransportValid(const Transport& transport)
{
    if (transport.bpm <= 0.0)
    {
        return false;
    }
    if (transport.hostSampleRate <= 0.0)
    {
        return false;
    }
    return true;
}

bool isRegionValid(const SampleRegion& region)
{
    if (region.lengthInFileSamples <= 0.0)
    {
        return false;
    }
    if (region.fileSampleRate <= 0.0)
    {
        return false;
    }
    return true;
}

double patternLengthInBeats(const Pattern& pattern)
{
    return numUsedSteps(pattern) * kStepLengthInBeats;
}

// start of a sound in beats (ppq), for the step with the given index in the given pass
double startInBeats(const Pattern& pattern, const StepSettings& step, int stepIndex, int pass,
                    double durationInBeats)
{
    const double stepStart = (pass * numUsedSteps(pattern) + stepIndex) * kStepLengthInBeats;
    if (step.mode == StepMode::ReverseToEnd)
    {
        const double anchor = stepStart + kStepLengthInBeats;
        return anchor - durationInBeats;
    }
    return stepStart;
}

bool isStepScheduled(const Pattern& pattern, const StepSettings& step, const SampleRegion& region,
                     const Transport& transport)
{
    if (step.active == false)
    {
        return false;
    }
    if (step.mode == StepMode::ReverseToEnd)
    {
        return isPlayToEndAllowed(pattern, step, region, transport);
    }
    return true;
}

double durationInBeats(const Pattern& pattern, const StepSettings& step, const SampleRegion& region, const Transport& transport)
{
    const double ratio = playbackRatio(stepSemitones(pattern, step), region.fileSampleRate, transport.hostSampleRate);
    return durationInHostSamples(region, ratio) / samplesPerBeat(transport);
}

int firstPass(const Pattern& pattern, double positionPpq)
{
    const double passIndex = std::floor(positionPpq / patternLengthInBeats(pattern));
    return static_cast<int>(passIndex) - kPassesBefore;
}

int lastPass(const Pattern& pattern, double positionPpq)
{
    const double passIndex = std::floor(positionPpq / patternLengthInBeats(pattern));
    return static_cast<int>(passIndex) + kPassesAfter;
}
} // namespace

StepSettings& stepAt(Pattern& pattern, int stepIndex)
{
    return pattern.steps[static_cast<std::size_t>(stepIndex)];
}

const StepSettings& stepAt(const Pattern& pattern, int stepIndex)
{
    return pattern.steps[static_cast<std::size_t>(stepIndex)];
}

double stepSemitones(const Pattern& pattern, const StepSettings& step)
{
    return step.noteNumber - pattern.baseNote;
}

double playbackRatio(double pitchSemitones, double fileSampleRate, double hostSampleRate)
{
    const double pitchRatio = std::pow(2.0, pitchSemitones / kSemitonesPerOctave);
    return pitchRatio * fileSampleRate / hostSampleRate;
}

double durationInHostSamples(const SampleRegion& region, double ratio)
{
    return region.lengthInFileSamples / ratio;
}

double samplesPerBeat(const Transport& transport)
{
    return transport.hostSampleRate * kSecondsPerMinute / transport.bpm;
}

double samplesPerStep(const Transport& transport)
{
    return samplesPerBeat(transport) / kStepsPerBeat;
}

bool isPlayToEndAllowed(const Pattern& pattern, const StepSettings& step, const SampleRegion& region,
                        const Transport& transport)
{
    if (isRegionValid(region) == false)
    {
        return false;
    }
    if (isTransportValid(transport) == false)
    {
        return false;
    }
    const double ratio = playbackRatio(stepSemitones(pattern, step), region.fileSampleRate, transport.hostSampleRate);
    const double duration = durationInHostSamples(region, ratio);
    const double patternLength = numUsedSteps(pattern) * samplesPerStep(transport);
    return duration <= patternLength;
}

void scheduleBlock(const Pattern& pattern, const SampleRegion& region, const Transport& transport,
                   double blockStartPpq, int numSamples, std::vector<SoundStart>& soundStarts)
{
    soundStarts.clear();
    if (isRegionValid(region) == false)
    {
        return;
    }
    if (isTransportValid(transport) == false)
    {
        return;
    }
    if (numSamples <= 0)
    {
        return;
    }

    const double beatLength = samplesPerBeat(transport);
    const double blockEndPpq = blockStartPpq + numSamples / beatLength;
    const int usedSteps = numUsedSteps(pattern);

    for (int pass = firstPass(pattern, blockStartPpq); pass <= lastPass(pattern, blockEndPpq); ++pass)
    {
        for (int stepIndex = 0; stepIndex < usedSteps; ++stepIndex)
        {
            const StepSettings& step = stepAt(pattern, stepIndex);
            if (isStepScheduled(pattern, step, region, transport) == false)
            {
                continue;
            }
            const double duration = durationInBeats(pattern, step, region, transport);
            const double start = startInBeats(pattern, step, stepIndex, pass, duration);
            const double offsetInSamples = std::round((start - blockStartPpq) * beatLength);
            if (offsetInSamples < 0.0)
            {
                continue;
            }
            if (offsetInSamples >= numSamples)
            {
                continue;
            }

            if (static_cast<int>(soundStarts.size()) >= kMaxSoundStartsPerBlock)
            {
                return;
            }
            SoundStart soundStart;
            soundStart.stepIndex = stepIndex;
            soundStart.sampleOffsetInBlock = static_cast<int>(offsetInSamples);
            soundStart.playbackRatio = playbackRatio(stepSemitones(pattern, step), region.fileSampleRate, transport.hostSampleRate);
            soundStart.reverse = (step.mode != StepMode::Forward);
            soundStart.playsToEnd = (step.mode == StepMode::ReverseToEnd);
            soundStart.gain = step.velocity / kMaxMidiVelocity;
            soundStarts.push_back(soundStart);
        }
    }
}

int countMissedPlayToEnd(const Pattern& pattern, const SampleRegion& region, const Transport& transport,
                         double positionPpq)
{
    if (isRegionValid(region) == false)
    {
        return 0;
    }
    if (isTransportValid(transport) == false)
    {
        return 0;
    }

    int missedSounds = 0;
    const int usedSteps = numUsedSteps(pattern);
    for (int pass = firstPass(pattern, positionPpq); pass <= lastPass(pattern, positionPpq); ++pass)
    {
        for (int stepIndex = 0; stepIndex < usedSteps; ++stepIndex)
        {
            const StepSettings& step = stepAt(pattern, stepIndex);
            if (step.mode != StepMode::ReverseToEnd)
            {
                continue;
            }
            if (isStepScheduled(pattern, step, region, transport) == false)
            {
                continue;
            }
            const double duration = durationInBeats(pattern, step, region, transport);
            const double start = startInBeats(pattern, step, stepIndex, pass, duration);
            const double anchor = start + duration;
            if (start < positionPpq && positionPpq < anchor)
            {
                ++missedSounds;
            }
        }
    }
    return missedSounds;
}
} // namespace dfd
