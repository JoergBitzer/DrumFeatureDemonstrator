#include <cmath>
#include <vector>

#include <juce_core/juce_core.h>

#include "../src/NoteNames.h"
#include "../src/StepScheduler.h"

namespace
{
constexpr double kRate48k = 48000.0;
constexpr double kBpm120 = 120.0;
constexpr double kSamplesPerStep120At48k = 6000.0; // 48000 * 60 / 120 / 4
constexpr double kBeatsPerPattern16 = 4.0;
constexpr double kOneSample = 1.0;
constexpr double kTinyTolerance = 1.0e-9;

dfd::Transport makeTransport(double bpm, double hostRate)
{
    dfd::Transport transport;
    transport.bpm = bpm;
    transport.hostSampleRate = hostRate;
    return transport;
}

dfd::SampleRegion makeRegion(double lengthInSamples, double fileRate)
{
    dfd::SampleRegion region;
    region.lengthInFileSamples = lengthInSamples;
    region.fileSampleRate = fileRate;
    return region;
}

void setStep(dfd::Pattern& pattern, int stepIndex, dfd::StepMode mode, double pitch)
{
    dfd::stepAt(pattern, stepIndex).active = true;
    dfd::stepAt(pattern, stepIndex).mode = mode;
    dfd::stepAt(pattern, stepIndex).noteNumber = pattern.baseNote + pitch;
}

// absolute start position of a sound in host samples, relative to ppq 0
double absoluteStart(const dfd::SoundStart& soundStart, double blockStartPpq, const dfd::Transport& transport)
{
    return blockStartPpq * dfd::samplesPerBeat(transport) + soundStart.sampleOffsetInBlock;
}
} // namespace

class StepSchedulerTests : public juce::UnitTest
{
public:
    StepSchedulerTests() : juce::UnitTest("StepScheduler", "DrumFeatureDemonstrator") {}

    void runTest() override
    {
        testRatioAndDuration();
        testForwardAndReverseStart();
        testPlayToEndIsExact();
        testLookAheadIntoPreviousPass();
        testEverySoundExactlyOnce();
        testOddPatternLength();
        testLengthLimit();
        testInactiveAndOutOfRangeSteps();
        testMissedSounds();
        testNotesRelativeToTheBaseNote();
        testNoteNames();
        testVeryLongBlockStaysWithinTheLimit();
        testGainAndBadInput();
    }

private:
    void testRatioAndDuration()
    {
        beginTest("ratio, duration and step length");
        expectWithinAbsoluteError(dfd::playbackRatio(0.0, kRate48k, kRate48k), 1.0, kTinyTolerance);
        expectWithinAbsoluteError(dfd::playbackRatio(12.0, kRate48k, kRate48k), 2.0, kTinyTolerance);
        expectWithinAbsoluteError(dfd::playbackRatio(-12.0, kRate48k, kRate48k), 0.5, kTinyTolerance);
        expectWithinAbsoluteError(dfd::playbackRatio(0.0, 44100.0, kRate48k), 44100.0 / 48000.0, kTinyTolerance);

        const dfd::SampleRegion region = makeRegion(24000.0, kRate48k);
        expectWithinAbsoluteError(dfd::durationInHostSamples(region, 2.0), 12000.0, kTinyTolerance);

        const dfd::Transport transport = makeTransport(kBpm120, kRate48k);
        expectWithinAbsoluteError(dfd::samplesPerStep(transport), kSamplesPerStep120At48k, kTinyTolerance);
    }

    void testForwardAndReverseStart()
    {
        beginTest("forward and reverse steps start at the step");
        dfd::Pattern pattern;
        setStep(pattern, 2, dfd::StepMode::Forward, 0.0);
        setStep(pattern, 5, dfd::StepMode::Reverse, 0.0);
        const dfd::SampleRegion region = makeRegion(12000.0, kRate48k);
        const dfd::Transport transport = makeTransport(kBpm120, kRate48k);

        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        const int blockLength = 24000; // 1 beat (120 bpm, 48 kHz) = 4 steps: contains step 2 of pass 0 in beat 0, step 5 is in beat 1
        dfd::scheduleBlock(pattern, region, transport, 0.0, blockLength, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), 1);
        expectEquals(soundStarts[0].stepIndex, 2);
        expectEquals(soundStarts[0].sampleOffsetInBlock, static_cast<int>(2 * kSamplesPerStep120At48k));
        expect(soundStarts[0].reverse == false);

        dfd::scheduleBlock(pattern, region, transport, 1.0, blockLength, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), 1);
        expectEquals(soundStarts[0].stepIndex, 5);
        expectEquals(soundStarts[0].sampleOffsetInBlock, static_cast<int>(1 * kSamplesPerStep120At48k));
        expect(soundStarts[0].reverse);
        expect(soundStarts[0].playsToEnd == false);
    }

    void testPlayToEndIsExact()
    {
        beginTest("play to end: start + duration = end of the step");
        const double pitches[] = {-24.0, -7.0, 0.0, 5.0, 24.0};
        const double tempi[] = {60.0, 93.7, 120.0, 200.0};
        const double hostRates[] = {44100.0, 48000.0, 96000.0};
        const double fileRates[] = {44100.0, 48000.0};
        const double lengths[] = {1000.0, 4410.0, 12345.0};
        constexpr int kStepIndex = 9;
        constexpr int kBlockLength = 4096;

        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        int numChecked = 0;
        for (double pitch : pitches)
        {
            for (double bpm : tempi)
            {
                for (double hostRate : hostRates)
                {
                    for (double fileRate : fileRates)
                    {
                        for (double length : lengths)
                        {
                            checkOnePlayToEnd(pitch, bpm, hostRate, fileRate, length, kStepIndex, kBlockLength,
                                              soundStarts, numChecked);
                        }
                    }
                }
            }
        }
        expect(numChecked > 0);
    }

    void checkOnePlayToEnd(double pitch, double bpm, double hostRate, double fileRate, double length, int stepIndex,
                           int blockLength, std::vector<dfd::SoundStart>& soundStarts, int& numChecked)
    {
        dfd::Pattern pattern;
        setStep(pattern, stepIndex, dfd::StepMode::ReverseToEnd, pitch);
        const dfd::SampleRegion region = makeRegion(length, fileRate);
        const dfd::Transport transport = makeTransport(bpm, hostRate);

        const double ratio = dfd::playbackRatio(pitch, fileRate, hostRate);
        const double duration = dfd::durationInHostSamples(region, ratio);
        const double anchorPpq = (2 * pattern.numSteps + stepIndex + 1) / static_cast<double>(dfd::kStepsPerBeat);
        const double startPpq = anchorPpq - duration / dfd::samplesPerBeat(transport);

        // the block that contains the computed start
        const double blockStart = std::floor(startPpq * dfd::samplesPerBeat(transport)) / dfd::samplesPerBeat(transport);
        dfd::scheduleBlock(pattern, region, transport, blockStart, blockLength, soundStarts);
        if (dfd::isPlayToEndAllowed(pattern, dfd::stepAt(pattern, stepIndex), region, transport) == false)
        {
            expectEquals(static_cast<int>(soundStarts.size()), 0);
            return;
        }
        expectEquals(static_cast<int>(soundStarts.size()), 1);
        if (soundStarts.size() != 1)
        {
            return;
        }
        const double endOfSound = absoluteStart(soundStarts[0], blockStart, transport) + duration;
        const double anchorInSamples = anchorPpq * dfd::samplesPerBeat(transport);
        expectWithinAbsoluteError(endOfSound, anchorInSamples, kOneSample);
        expect(soundStarts[0].reverse);
        expect(soundStarts[0].playsToEnd);
        ++numChecked;
    }

    void testLookAheadIntoPreviousPass()
    {
        beginTest("look-ahead: a sound for step 0 starts in the pass before");
        dfd::Pattern pattern;
        setStep(pattern, 0, dfd::StepMode::ReverseToEnd, 0.0);
        // duration = 2 steps: ends at the end of step 0 of pass 1 (ppq 4.25), starts at ppq 3.75 (step 14 of pass 0)
        const dfd::SampleRegion region = makeRegion(2 * kSamplesPerStep120At48k, kRate48k);
        const dfd::Transport transport = makeTransport(kBpm120, kRate48k);

        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        const double blockStartPpq = kBeatsPerPattern16 - 1.0; // ppq 3.0, the last beat of pass 0
        const int blockLength = 24000; // one beat
        dfd::scheduleBlock(pattern, region, transport, blockStartPpq, blockLength, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), 1);
        if (soundStarts.size() == 1)
        {
            expectEquals(soundStarts[0].stepIndex, 0);
            const double endOfSound = absoluteStart(soundStarts[0], blockStartPpq, transport)
                                      + dfd::durationInHostSamples(region, 1.0);
            const double anchorInSamples = (kBeatsPerPattern16 + 0.25) * dfd::samplesPerBeat(transport);
            expectWithinAbsoluteError(endOfSound, anchorInSamples, kOneSample);
        }
    }

    void testEverySoundExactlyOnce()
    {
        beginTest("every sound exactly once across consecutive blocks");
        dfd::Pattern pattern;
        setStep(pattern, 0, dfd::StepMode::Forward, 0.0);
        setStep(pattern, 3, dfd::StepMode::Reverse, 7.0);
        setStep(pattern, 7, dfd::StepMode::ReverseToEnd, -5.0);
        setStep(pattern, 15, dfd::StepMode::ReverseToEnd, 12.0);
        const dfd::SampleRegion region = makeRegion(9000.0, 44100.0);
        const dfd::Transport transport = makeTransport(97.3, kRate48k);

        const int blockSizes[] = {64, 100, 480, 512, 1000, 4096};
        constexpr int kNumPasses = 3;
        constexpr int kSoundsPerPass = 4;
        // start behind the first pass, so that no sound is lost to the start of the transport
        constexpr double kStartPpq = 2.0 * kBeatsPerPattern16;
        for (int blockSize : blockSizes)
        {
            const double lengthInSamples = kNumPasses * kBeatsPerPattern16 * dfd::samplesPerBeat(transport);
            const int numBlocks = static_cast<int>(lengthInSamples / blockSize);
            std::vector<dfd::SoundStart> soundStarts;
            soundStarts.reserve(dfd::kMaxSteps);
            int numSounds = 0;
            double blockStartPpq = kStartPpq;
            for (int blockIndex = 0; blockIndex < numBlocks; ++blockIndex)
            {
                dfd::scheduleBlock(pattern, region, transport, blockStartPpq, blockSize, soundStarts);
                numSounds += static_cast<int>(soundStarts.size());
                blockStartPpq += blockSize / dfd::samplesPerBeat(transport);
            }
            // the sounds in the last block of the range may start later, so allow the look-ahead at the end
            expect(numSounds >= (kNumPasses - 1) * kSoundsPerPass);
            expect(numSounds <= (kNumPasses + 1) * kSoundsPerPass);
            expectEquals(numSounds, countSoundsInRange(pattern, region, transport, blockSize, numBlocks, kStartPpq));
        }
    }

    // reference count: one big block over the same range
    int countSoundsInRange(const dfd::Pattern& pattern, const dfd::SampleRegion& region, const dfd::Transport& transport,
                           int blockSize, int numBlocks, double startPpq)
    {
        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        const int totalLength = blockSize * numBlocks;
        dfd::scheduleBlock(pattern, region, transport, startPpq, totalLength, soundStarts);
        return static_cast<int>(soundStarts.size());
    }

    void testOddPatternLength()
    {
        beginTest("pattern length 11: the pattern repeats after 11 steps");
        dfd::Pattern pattern;
        pattern.numSteps = 11;
        setStep(pattern, 0, dfd::StepMode::Forward, 0.0);
        setStep(pattern, 11, dfd::StepMode::Forward, 0.0); // beyond numSteps: ignored
        const dfd::SampleRegion region = makeRegion(1000.0, kRate48k);
        const dfd::Transport transport = makeTransport(kBpm120, kRate48k);

        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        const double secondPassPpq = 11.0 / dfd::kStepsPerBeat;
        const int oneStep = static_cast<int>(kSamplesPerStep120At48k);
        dfd::scheduleBlock(pattern, region, transport, secondPassPpq, oneStep, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), 1);
        if (soundStarts.size() == 1)
        {
            expectEquals(soundStarts[0].sampleOffsetInBlock, 0);
        }
        dfd::scheduleBlock(pattern, region, transport, 1.0 / dfd::kStepsPerBeat, 10 * oneStep, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), 0);
    }

    void testLengthLimit()
    {
        beginTest("length limit of play to end");
        dfd::Pattern pattern;
        setStep(pattern, 4, dfd::StepMode::ReverseToEnd, 0.0);
        const dfd::Transport transport = makeTransport(kBpm120, kRate48k);
        const double patternLength = pattern.numSteps * kSamplesPerStep120At48k; // 96000 samples

        const dfd::SampleRegion fits = makeRegion(patternLength, kRate48k);
        expect(dfd::isPlayToEndAllowed(pattern, dfd::stepAt(pattern, 4), fits, transport));
        const dfd::SampleRegion tooLong = makeRegion(patternLength + 1.0, kRate48k);
        expect(dfd::isPlayToEndAllowed(pattern, dfd::stepAt(pattern, 4), tooLong, transport) == false);

        // a higher pitch makes it fit again
        dfd::stepAt(pattern, 4).noteNumber = pattern.baseNote + 12.0;
        expect(dfd::isPlayToEndAllowed(pattern, dfd::stepAt(pattern, 4), tooLong, transport));

        // an invalid step is not scheduled
        dfd::stepAt(pattern, 4).noteNumber = pattern.baseNote;
        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        dfd::scheduleBlock(pattern, tooLong, transport, 0.0, 400000, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), 0);
    }

    void testInactiveAndOutOfRangeSteps()
    {
        beginTest("inactive steps do not play");
        dfd::Pattern pattern;
        dfd::stepAt(pattern, 1).active = false;
        dfd::stepAt(pattern, 1).mode = dfd::StepMode::Reverse;
        const dfd::SampleRegion region = makeRegion(1000.0, kRate48k);
        const dfd::Transport transport = makeTransport(kBpm120, kRate48k);
        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        dfd::scheduleBlock(pattern, region, transport, 0.0, 200000, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), 0);
    }

    void testMissedSounds()
    {
        beginTest("missed play-to-end sounds at a transport start and a jump");
        dfd::Pattern pattern;
        setStep(pattern, 15, dfd::StepMode::ReverseToEnd, 0.0);
        const dfd::SampleRegion region = makeRegion(2 * kSamplesPerStep120At48k, kRate48k); // 2 steps
        const dfd::Transport transport = makeTransport(kBpm120, kRate48k);
        // end at ppq 4.0, start at ppq 3.5
        expectEquals(dfd::countMissedPlayToEnd(pattern, region, transport, 3.75), 1);
        expectEquals(dfd::countMissedPlayToEnd(pattern, region, transport, 3.0), 0);
        expectEquals(dfd::countMissedPlayToEnd(pattern, region, transport, 4.0), 0);
        expectEquals(dfd::countMissedPlayToEnd(pattern, region, transport, 3.5), 0); // start exactly here: not missed

        // transport start at 0: the sound of pass -1 ends exactly at 0.0, nothing is running
        expectEquals(dfd::countMissedPlayToEnd(pattern, region, transport, 0.0), 0);
        expectEquals(dfd::countMissedPlayToEnd(pattern, region, transport, 0.25), 0);
        // the sound of step 15 of pass -1 runs from ppq -0.5 to 0.0: a position inside (a loop jump) misses it
        expectEquals(dfd::countMissedPlayToEnd(pattern, region, transport, -0.25), 1);
    }

    void testVeryLongBlockStaysWithinTheLimit()
    {
        beginTest("a very long block does not exceed the sound start limit (no reallocation)");
        dfd::Pattern pattern;
        pattern.numSteps = dfd::kMinSteps;
        for (int stepIndex = 0; stepIndex < dfd::kMinSteps; ++stepIndex)
        {
            setStep(pattern, stepIndex, dfd::StepMode::Forward, 0.0);
        }
        const dfd::SampleRegion region = makeRegion(100.0, kRate48k);
        const dfd::Transport transport = makeTransport(kBpm120, kRate48k);
        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSoundStartsPerBlock);
        const void* firstElement = soundStarts.data();
        const int tenMinutes = 10 * 60 * static_cast<int>(kRate48k);
        dfd::scheduleBlock(pattern, region, transport, 4.0, tenMinutes, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), dfd::kMaxSoundStartsPerBlock);
        expect(static_cast<const void*>(soundStarts.data()) == firstElement);
    }

    void testNotesRelativeToTheBaseNote()
    {
        beginTest("a step note is relative to the base note of the pattern");
        dfd::Pattern pattern;
        dfd::StepSettings& step = dfd::stepAt(pattern, 0);
        step.active = true;
        step.noteNumber = 60.0;
        const dfd::SampleRegion region = makeRegion(1000.0, kRate48k);
        const dfd::Transport transport = makeTransport(kBpm120, kRate48k);
        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSoundStartsPerBlock);

        dfd::scheduleBlock(pattern, region, transport, 0.0, 100, soundStarts); // base note 60, step note 60: original speed
        expectEquals(static_cast<int>(soundStarts.size()), 1);
        expectWithinAbsoluteError(soundStarts[0].playbackRatio, 1.0, kTinyTolerance);

        pattern.baseNote = 48.0; // the sample sits an octave lower: note 60 is an octave above it
        dfd::scheduleBlock(pattern, region, transport, 0.0, 100, soundStarts);
        expectWithinAbsoluteError(soundStarts[0].playbackRatio, 2.0, kTinyTolerance);

        step.noteNumber = 41.0; // 7 semitones below 48
        dfd::scheduleBlock(pattern, region, transport, 0.0, 100, soundStarts);
        expectWithinAbsoluteError(soundStarts[0].playbackRatio, std::pow(2.0, -7.0 / 12.0), kTinyTolerance);
    }

    void testNoteNames()
    {
        beginTest("note names");
        expectEquals(juce::String(dfd::noteName(60)), juce::String("C4"));
        expectEquals(juce::String(dfd::noteName(61)), juce::String("C#4"));
        expectEquals(juce::String(dfd::noteName(69)), juce::String("A4"));
        expectEquals(juce::String(dfd::noteName(12)), juce::String("C0"));
        expectEquals(juce::String(dfd::noteName(11)), juce::String("B-1"));
        expectEquals(juce::String(dfd::noteName(0)), juce::String("C-1"));
        expectEquals(juce::String(dfd::noteName(127)), juce::String("G9"));
    }

    void testGainAndBadInput()
    {
        beginTest("gain is the velocity, bad input gives no sounds");
        dfd::Pattern pattern;
        setStep(pattern, 0, dfd::StepMode::Forward, 0.0);
        dfd::stepAt(pattern, 0).velocity = dfd::kMaxMidiVelocity * 0.5f;
        dfd::stepAt(pattern, 0).pan = -0.25f;
        const dfd::SampleRegion region = makeRegion(1000.0, kRate48k);
        const dfd::Transport transport = makeTransport(kBpm120, kRate48k);
        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        dfd::scheduleBlock(pattern, region, transport, 0.0, 1000, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), 1);
        if (soundStarts.size() == 1)
        {
            expectWithinAbsoluteError(soundStarts[0].gain, 0.5f, 1.0e-6f);
            expectWithinAbsoluteError(soundStarts[0].pan, -0.25f, 1.0e-6f);
        }

        dfd::scheduleBlock(pattern, region, makeTransport(0.0, kRate48k), 0.0, 1000, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), 0);
        dfd::scheduleBlock(pattern, makeRegion(0.0, kRate48k), transport, 0.0, 1000, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), 0);
        dfd::scheduleBlock(pattern, region, transport, 0.0, 0, soundStarts);
        expectEquals(static_cast<int>(soundStarts.size()), 0);
    }
};

static StepSchedulerTests s_stepSchedulerTests;
