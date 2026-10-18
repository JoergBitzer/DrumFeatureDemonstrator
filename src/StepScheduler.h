/**
 * @file StepScheduler.h
 * @brief pure scheduling of the step pattern, including "play to end" (see docs/design/W2-play-to-end.md)
 *
 * No JUCE types, no audio, no GUI: only numbers in, sound starts out, so it can be tested with known values.
 */
#pragma once

#include <array>
#include <vector>

namespace dfd
{
constexpr int kMinSteps = 8;
constexpr int kMaxSteps = 32;
constexpr int kDefaultSteps = 16;
constexpr int kNumPatterns = 4;
constexpr int kStepsPerBeat = 4; // 1/16 notes
constexpr int kMaxSoundStartsPerBlock = 256; // scheduleBlock stops here, so a reserved output vector never reallocates
constexpr double kSemitonesPerOctave = 12.0;
constexpr double kSecondsPerMinute = 60.0;
constexpr double kDefaultBaseNote = 60.0; // MIDI note of the original pitch of the sample (C4)
constexpr float kMaxMidiVelocity = 127.0f;

enum class StepMode
{
    Forward,
    Reverse,
    ReverseToEnd // reverse, started so that the sample ends at the end of the step
};

struct StepSettings
{
    bool active = false;
    StepMode mode = StepMode::Forward;
    double noteNumber = kDefaultBaseNote; // MIDI note: the sample sounds at its original pitch if it equals the base note
    float velocity = kMaxMidiVelocity;    // MIDI velocity 1 - 127, the gain is velocity / 127
    float pan = 0.0f;                     // -1 left, 0 centre, 1 right
};

struct Pattern
{
    int numSteps = kDefaultSteps;
    double baseNote = kDefaultBaseNote;
    std::array<StepSettings, kMaxSteps> steps;
};

// four patterns that share the length and the base note; the patterns that are switched on are played one after the other,
// from the first to the last (a "chain"): with patterns 1 and 3 on, the sequence is 1, 3, 1, 3, ...
struct PatternBank
{
    int numSteps = kDefaultSteps;
    double baseNote = kDefaultBaseNote;
    std::array<std::array<StepSettings, kMaxSteps>, kNumPatterns> steps;
    std::array<bool, kNumPatterns> playing{{true, false, false, false}};
};

struct SampleRegion
{
    double lengthInFileSamples = 0.0;
    double fileSampleRate = 0.0;
};

struct Transport
{
    double bpm = 0.0;
    double hostSampleRate = 0.0;
};

struct SoundStart
{
    int patternIndex = 0;
    int stepIndex = 0;
    int sampleOffsetInBlock = 0;
    double playbackRatio = 1.0;
    bool reverse = false;
    bool playsToEnd = false;
    float gain = 1.0f;
    float pan = 0.0f;
};

// number of steps of the pattern, limited to kMinSteps..kMaxSteps
int numUsedSteps(const Pattern& pattern);

// a bank with this one pattern (as pattern 0, the only one that plays); for tests and simple uses
PatternBank makeBank(const Pattern& pattern);
int numUsedSteps(const PatternBank& bank);
StepSettings& stepAt(PatternBank& bank, int patternIndex, int stepIndex);
const StepSettings& stepAt(const PatternBank& bank, int patternIndex, int stepIndex);
// number of patterns that are switched on, and the index of the pattern at a place of the chain (0 .. count - 1)
int numPlayingPatterns(const PatternBank& bank);
int playingPatternAt(const PatternBank& bank, int chainSlot);

// access by an int index (the array index type is unsigned)
StepSettings& stepAt(Pattern& pattern, int stepIndex);
const StepSettings& stepAt(const Pattern& pattern, int stepIndex);

// semitones of the step relative to the base note of the pattern
double stepSemitones(const Pattern& pattern, const StepSettings& step);
double stepSemitones(const PatternBank& bank, const StepSettings& step);

double playbackRatio(double pitchSemitones, double fileSampleRate, double hostSampleRate);
double durationInHostSamples(const SampleRegion& region, double playbackRatio);
double samplesPerBeat(const Transport& transport);
double samplesPerStep(const Transport& transport);

// true if the sample at the pitch of the step fits into one pattern (the condition for ReverseToEnd)
bool isPlayToEndAllowed(const Pattern& pattern, const StepSettings& step, const SampleRegion& region, const Transport& transport);
bool isPlayToEndAllowed(const PatternBank& bank, const StepSettings& step, const SampleRegion& region, const Transport& transport);

// Sound starts of the block [blockStartPpq, blockStartPpq + numSamples). The output vector is cleared first;
// reserve kMaxSoundStartsPerBlock beforehand, then no allocation happens (further sound starts of a very long block are dropped).
void scheduleBlock(const Pattern& pattern, const SampleRegion& region, const Transport& transport,
                   double blockStartPpq, int numSamples, std::vector<SoundStart>& soundStarts);
// the same for the chain of the playing patterns of a bank (the starts carry the pattern they belong to)
void scheduleBlock(const PatternBank& bank, const SampleRegion& region, const Transport& transport,
                   double blockStartPpq, int numSamples, std::vector<SoundStart>& soundStarts);

// number of ReverseToEnd sounds that should be running at positionPpq, but whose start is already gone
int countMissedPlayToEnd(const Pattern& pattern, const SampleRegion& region, const Transport& transport, double positionPpq);
int countMissedPlayToEnd(const PatternBank& bank, const SampleRegion& region, const Transport& transport, double positionPpq);
} // namespace dfd
