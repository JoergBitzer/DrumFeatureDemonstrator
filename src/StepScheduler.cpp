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
constexpr int kPassesBefore = 1; // look-ahead: a sound starts at most one pass of the chain before its step
constexpr int kPassesAfter = 1;

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

// the whole chain (all playing patterns one after the other) in steps and in beats
int chainLengthInSteps(const PatternBank& bank)
{
    return numPlayingPatterns(bank) * numUsedSteps(bank);
}

double chainLengthInBeats(const PatternBank& bank)
{
    return chainLengthInSteps(bank) * bank.stepLengthInBeats;
}

int clampedRepeat(int repeat)
{
    if (repeat < kMinRepeats)
    {
        return kMinRepeats;
    }
    if (repeat > kMaxRepeats)
    {
        return kMaxRepeats;
    }
    return repeat;
}

// start of hit number hitIndex of a step in beats (ppq): globalStepIndex counts the steps of the whole chain from the position 0 on
double startInBeats(const StepSettings& step, long long globalStepIndex, double stepLengthInBeats, double durationInBeats, int hitIndex)
{
    const double stepStart = static_cast<double>(globalStepIndex) * stepLengthInBeats;
    const int repeatCount = clampedRepeat(step.repeat);
    if (step.mode == StepMode::ReverseToEnd)
    {
        const double anchor = stepStart + repeatEndFraction(repeatCount, step.ramp, hitIndex) * stepLengthInBeats;
        return anchor - durationInBeats;
    }
    return stepStart + repeatStartFraction(repeatCount, step.ramp, hitIndex) * stepLengthInBeats;
}

bool isStepScheduled(const PatternBank& bank, const StepSettings& step, const SampleRegion& region, const Transport& transport)
{
    if (step.active == false)
    {
        return false;
    }
    if (step.mode == StepMode::ReverseToEnd)
    {
        return isPlayToEndAllowed(bank, step, region, transport);
    }
    return true;
}

double durationInBeats(const PatternBank& bank, const StepSettings& step, const SampleRegion& region, const Transport& transport)
{
    const double ratio = playbackRatio(stepSemitones(bank, step), region.fileSampleRate, transport.hostSampleRate);
    return durationInHostSamples(region, ratio) / samplesPerBeat(transport);
}

int firstPass(const PatternBank& bank, double positionPpq)
{
    const double passIndex = std::floor(positionPpq / chainLengthInBeats(bank));
    return static_cast<int>(passIndex) - kPassesBefore;
}

int lastPass(const PatternBank& bank, double positionPpq)
{
    const double passIndex = std::floor(positionPpq / chainLengthInBeats(bank));
    return static_cast<int>(passIndex) + kPassesAfter;
}

long long globalStepIndexOf(const PatternBank& bank, int pass, int chainSlot, int stepIndex)
{
    const long long chainSteps = chainLengthInSteps(bank);
    return static_cast<long long>(pass) * chainSteps + static_cast<long long>(chainSlot) * numUsedSteps(bank) + stepIndex;
}
} // namespace

double repeatStartFraction(int repeatCount, double ramp, int hitIndex)
{
    const int count = clampedRepeat(repeatCount);
    if (hitIndex <= 0)
    {
        return 0.0;
    }
    if (hitIndex >= count)
    {
        return 1.0;
    }
    const double clampedRamp = std::fmax(-1.0, std::fmin(1.0, ramp));
    const double exponent = std::pow(2.0, -clampedRamp * kRampExponentRange);
    return std::pow(static_cast<double>(hitIndex) / static_cast<double>(count), exponent);
}

double repeatEndFraction(int repeatCount, double ramp, int hitIndex)
{
    return repeatStartFraction(repeatCount, ramp, hitIndex + 1);
}

PatternBank makeBank(const Pattern& pattern)
{
    PatternBank bank;
    bank.numSteps = pattern.numSteps;
    bank.baseNote = pattern.baseNote;
    bank.stepLengthInBeats = pattern.stepLengthInBeats;
    bank.steps[0] = pattern.steps;
    return bank;
}

int numUsedSteps(const PatternBank& bank)
{
    if (bank.numSteps < kMinSteps)
    {
        return kMinSteps;
    }
    if (bank.numSteps > kMaxSteps)
    {
        return kMaxSteps;
    }
    return bank.numSteps;
}

StepSettings& stepAt(PatternBank& bank, int patternIndex, int stepIndex)
{
    return bank.steps[static_cast<std::size_t>(patternIndex)][static_cast<std::size_t>(stepIndex)];
}

const StepSettings& stepAt(const PatternBank& bank, int patternIndex, int stepIndex)
{
    return bank.steps[static_cast<std::size_t>(patternIndex)][static_cast<std::size_t>(stepIndex)];
}

int numPlayingPatterns(const PatternBank& bank)
{
    int count = 0;
    for (bool isPlaying : bank.playing)
    {
        if (isPlaying)
        {
            ++count;
        }
    }
    return count;
}

int playingPatternAt(const PatternBank& bank, int chainSlot)
{
    int slot = 0;
    for (int patternIndex = 0; patternIndex < kNumPatterns; ++patternIndex)
    {
        if (bank.playing[static_cast<std::size_t>(patternIndex)])
        {
            if (slot == chainSlot)
            {
                return patternIndex;
            }
            ++slot;
        }
    }
    return -1;
}

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

double stepSemitones(const PatternBank& bank, const StepSettings& step)
{
    return step.noteNumber - bank.baseNote;
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
    return samplesPerStep(transport, kDefaultStepLengthInBeats);
}

double samplesPerStep(const Transport& transport, double stepLengthInBeats)
{
    return samplesPerBeat(transport) * stepLengthInBeats;
}

bool isPlayToEndAllowed(const Pattern& pattern, const StepSettings& step, const SampleRegion& region,
                        const Transport& transport)
{
    return isPlayToEndAllowed(makeBank(pattern), step, region, transport);
}

bool isPlayToEndAllowed(const PatternBank& bank, const StepSettings& step, const SampleRegion& region,
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
    const double ratio = playbackRatio(stepSemitones(bank, step), region.fileSampleRate, transport.hostSampleRate);
    const double duration = durationInHostSamples(region, ratio);
    const double patternLength = numUsedSteps(bank) * samplesPerStep(transport, bank.stepLengthInBeats);
    return duration <= patternLength;
}

void scheduleBlock(const Pattern& pattern, const SampleRegion& region, const Transport& transport,
                   double blockStartPpq, int numSamples, std::vector<SoundStart>& soundStarts)
{
    scheduleBlock(makeBank(pattern), region, transport, blockStartPpq, numSamples, soundStarts);
}

void scheduleBlock(const PatternBank& bank, const SampleRegion& region, const Transport& transport,
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
    const int chainCount = numPlayingPatterns(bank);
    if (chainCount == 0)
    {
        return;
    }

    const double beatLength = samplesPerBeat(transport);
    const double blockEndPpq = blockStartPpq + numSamples / beatLength;
    const int usedSteps = numUsedSteps(bank);

    for (int pass = firstPass(bank, blockStartPpq); pass <= lastPass(bank, blockEndPpq); ++pass)
    {
        for (int chainSlot = 0; chainSlot < chainCount; ++chainSlot)
        {
            const int patternIndex = playingPatternAt(bank, chainSlot);
            for (int stepIndex = 0; stepIndex < usedSteps; ++stepIndex)
            {
                const StepSettings& step = stepAt(bank, patternIndex, stepIndex);
                if (isStepScheduled(bank, step, region, transport) == false)
                {
                    continue;
                }
                const double duration = durationInBeats(bank, step, region, transport);
                const long long globalStepIndex = globalStepIndexOf(bank, pass, chainSlot, stepIndex);
                for (int hitIndex = 0; hitIndex < clampedRepeat(step.repeat); ++hitIndex)
                {
                    const double start = startInBeats(step, globalStepIndex, bank.stepLengthInBeats, duration, hitIndex);
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
                    soundStart.patternIndex = patternIndex;
                    soundStart.stepIndex = stepIndex;
                    soundStart.sampleOffsetInBlock = static_cast<int>(offsetInSamples);
                    soundStart.playbackRatio = playbackRatio(stepSemitones(bank, step), region.fileSampleRate, transport.hostSampleRate);
                    soundStart.reverse = (step.mode != StepMode::Forward);
                    soundStart.playsToEnd = (step.mode == StepMode::ReverseToEnd);
                    soundStart.gain = step.velocity / kMaxMidiVelocity;
                    soundStart.pan = step.pan;
                    soundStarts.push_back(soundStart);
                }
            }
        }
    }
}

int countMissedPlayToEnd(const Pattern& pattern, const SampleRegion& region, const Transport& transport,
                         double positionPpq)
{
    return countMissedPlayToEnd(makeBank(pattern), region, transport, positionPpq);
}

int countMissedPlayToEnd(const PatternBank& bank, const SampleRegion& region, const Transport& transport,
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
    const int chainCount = numPlayingPatterns(bank);
    if (chainCount == 0)
    {
        return 0;
    }

    int missedSounds = 0;
    const int usedSteps = numUsedSteps(bank);
    for (int pass = firstPass(bank, positionPpq); pass <= lastPass(bank, positionPpq); ++pass)
    {
        for (int chainSlot = 0; chainSlot < chainCount; ++chainSlot)
        {
            const int patternIndex = playingPatternAt(bank, chainSlot);
            for (int stepIndex = 0; stepIndex < usedSteps; ++stepIndex)
            {
                const StepSettings& step = stepAt(bank, patternIndex, stepIndex);
                if (step.mode != StepMode::ReverseToEnd)
                {
                    continue;
                }
                if (isStepScheduled(bank, step, region, transport) == false)
                {
                    continue;
                }
                const double duration = durationInBeats(bank, step, region, transport);
                const long long globalStepIndex = globalStepIndexOf(bank, pass, chainSlot, stepIndex);
                for (int hitIndex = 0; hitIndex < clampedRepeat(step.repeat); ++hitIndex)
                {
                    const double start = startInBeats(step, globalStepIndex, bank.stepLengthInBeats, duration, hitIndex);
                    const double anchor = start + duration;
                    if (start < positionPpq && positionPpq < anchor)
                    {
                        ++missedSounds;
                    }
                }
            }
        }
    }
    return missedSounds;
}
} // namespace dfd
