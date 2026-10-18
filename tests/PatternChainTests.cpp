#include <cmath>
#include <vector>

#include <juce_core/juce_core.h>

#include "../src/Sequencer.h"

namespace
{
constexpr double kRate48k = 48000.0;
constexpr double kBpm120 = 120.0;
constexpr double kSamplesPerBeat = 24000.0; // 120 bpm at 48 kHz
constexpr double kSamplesPerStep = 6000.0;
constexpr double kBeatsPerPattern16 = 4.0;

dfd::Transport makeTransport()
{
    dfd::Transport transport;
    transport.bpm = kBpm120;
    transport.hostSampleRate = kRate48k;
    return transport;
}

dfd::SampleRegion makeRegion(double length)
{
    dfd::SampleRegion region;
    region.lengthInFileSamples = length;
    region.fileSampleRate = kRate48k;
    return region;
}

void setStep(dfd::PatternBank& bank, int patternIndex, int stepIndex, dfd::StepMode mode)
{
    dfd::StepSettings& step = dfd::stepAt(bank, patternIndex, stepIndex);
    step.active = true;
    step.mode = mode;
}

void setPlaying(dfd::PatternBank& bank, bool first, bool second, bool third, bool fourth)
{
    bank.playing = {{first, second, third, fourth}};
}

std::vector<dfd::SoundStart> schedule(const dfd::PatternBank& bank, const dfd::SampleRegion& region, double startPpq, double lengthInBeats)
{
    std::vector<dfd::SoundStart> soundStarts;
    soundStarts.reserve(dfd::kMaxSoundStartsPerBlock);
    dfd::scheduleBlock(bank, region, makeTransport(), startPpq, static_cast<int>(lengthInBeats * kSamplesPerBeat), soundStarts);
    return soundStarts;
}
} // namespace

class PatternChainTests : public juce::UnitTest
{
public:
    PatternChainTests() : juce::UnitTest("PatternChain", "DrumFeatureDemonstrator") {}

    void runTest() override
    {
        testPlayingPatternsAreChained();
        testSwitchedOffPatternsAreSkipped();
        testNothingPlaysWithoutAPlayingPattern();
        testPlayToEndAcrossThePatternBoundary();
        testSequencerReportsThePattern();
        testMissedSoundsInTheChain();
        testShortPatternsAndFourPatterns();
    }

private:
    void testPlayingPatternsAreChained()
    {
        beginTest("patterns 1 and 3 on: played one after the other, from left to right");
        dfd::PatternBank bank;
        setPlaying(bank, true, false, true, false);
        setStep(bank, 0, 0, dfd::StepMode::Forward);
        setStep(bank, 2, 0, dfd::StepMode::Forward);
        setStep(bank, 1, 0, dfd::StepMode::Forward); // pattern 2 is off: never heard

        const std::vector<dfd::SoundStart> starts = schedule(bank, makeRegion(1000.0), 0.0, 2 * kBeatsPerPattern16);
        expectEquals(static_cast<int>(starts.size()), 2);
        if (starts.size() == 2)
        {
            expectEquals(starts[0].patternIndex, 0);
            expectEquals(starts[0].sampleOffsetInBlock, 0);
            expectEquals(starts[1].patternIndex, 2);
            expectEquals(starts[1].sampleOffsetInBlock, static_cast<int>(kBeatsPerPattern16 * kSamplesPerBeat));
        }
        // the chain repeats: the next cycle starts again with pattern 1
        const std::vector<dfd::SoundStart> next = schedule(bank, makeRegion(1000.0), 2 * kBeatsPerPattern16, kBeatsPerPattern16);
        expectEquals(static_cast<int>(next.size()), 1);
        if (next.size() == 1)
        {
            expectEquals(next[0].patternIndex, 0);
        }
    }

    void testSwitchedOffPatternsAreSkipped()
    {
        beginTest("a switched-off pattern in the middle leaves no gap");
        dfd::PatternBank bank;
        setPlaying(bank, true, false, false, true);
        setStep(bank, 0, 0, dfd::StepMode::Forward);
        setStep(bank, 3, 0, dfd::StepMode::Forward);
        const std::vector<dfd::SoundStart> starts = schedule(bank, makeRegion(1000.0), 0.0, 2 * kBeatsPerPattern16);
        expectEquals(static_cast<int>(starts.size()), 2);
        if (starts.size() == 2)
        {
            expectEquals(starts[1].patternIndex, 3);
            expectEquals(starts[1].sampleOffsetInBlock, static_cast<int>(kBeatsPerPattern16 * kSamplesPerBeat)); // right behind pattern 1
        }
    }

    void testNothingPlaysWithoutAPlayingPattern()
    {
        beginTest("no pattern switched on: nothing plays, nothing is missed");
        dfd::PatternBank bank;
        setPlaying(bank, false, false, false, false);
        setStep(bank, 0, 0, dfd::StepMode::ReverseToEnd);
        expectEquals(static_cast<int>(schedule(bank, makeRegion(1000.0), 0.0, 8.0).size()), 0);
        expectEquals(dfd::countMissedPlayToEnd(bank, makeRegion(2 * kSamplesPerStep), makeTransport(), 0.2), 0);

        dfd::Sequencer sequencer;
        std::vector<dfd::SoundStart> starts;
        starts.reserve(dfd::kMaxSoundStartsPerBlock);
        dfd::SequencerResult result;
        dfd::BlockTransport transport;
        transport.isPlaying = true;
        transport.hasPosition = true;
        transport.bpm = kBpm120;
        sequencer.process(bank, makeRegion(1000.0), kRate48k, transport, 512, starts, result);
        expect(result.transportRunning);
        expectEquals(result.currentPattern, -1);
        expectEquals(result.currentStep, -1);
    }

    void testPlayToEndAcrossThePatternBoundary()
    {
        beginTest("play to end on step 1 of pattern 3 starts inside the time of pattern 1");
        dfd::PatternBank bank;
        setPlaying(bank, true, false, true, false);
        setStep(bank, 2, 0, dfd::StepMode::ReverseToEnd);
        const dfd::SampleRegion region = makeRegion(2 * kSamplesPerStep); // 2 steps = 0.5 beat
        // step 1 of the second pattern of the chain: ppq 4.0 .. 4.25; the sound is 0.5 beat long: it starts at 3.75
        const std::vector<dfd::SoundStart> starts = schedule(bank, region, 3.0, 1.0);
        expectEquals(static_cast<int>(starts.size()), 1);
        if (starts.size() == 1)
        {
            expectEquals(starts[0].patternIndex, 2);
            expectEquals(starts[0].sampleOffsetInBlock, static_cast<int>(0.75 * kSamplesPerBeat));
            expect(starts[0].playsToEnd);
        }
    }

    void testSequencerReportsThePattern()
    {
        beginTest("the sequencer reports the pattern and the step that play");
        dfd::PatternBank bank;
        setPlaying(bank, true, false, true, false);
        dfd::Sequencer sequencer;
        std::vector<dfd::SoundStart> starts;
        starts.reserve(dfd::kMaxSoundStartsPerBlock);
        dfd::SequencerResult result;
        dfd::BlockTransport transport;
        transport.isPlaying = true;
        transport.hasPosition = true;
        transport.bpm = kBpm120;

        struct Case
        {
            double positionPpq;
            int pattern;
            int step;
        };
        const Case cases[] = {{0.0, 0, 0}, {3.9, 0, 15}, {4.0, 2, 0}, {7.5, 2, 14}, {8.0, 0, 0}, {-0.25, 2, 15}};
        for (const Case& testCase : cases)
        {
            transport.positionPpq = testCase.positionPpq;
            sequencer.process(bank, makeRegion(1000.0), kRate48k, transport, 64, starts, result);
            expectEquals(result.currentPattern, testCase.pattern, "pattern at " + juce::String(testCase.positionPpq));
            expectEquals(result.currentStep, testCase.step, "step at " + juce::String(testCase.positionPpq));
        }
        transport.isPlaying = false;
        sequencer.process(bank, makeRegion(1000.0), kRate48k, transport, 64, starts, result);
        expect(result.transportRunning == false);
        expectEquals(result.currentPattern, -1);
    }

    void testMissedSoundsInTheChain()
    {
        beginTest("a jump into the run-up of a play-to-end sound of the second pattern counts it");
        dfd::PatternBank bank;
        setPlaying(bank, true, false, true, false);
        setStep(bank, 2, 0, dfd::StepMode::ReverseToEnd);
        const dfd::SampleRegion region = makeRegion(2 * kSamplesPerStep); // runs from ppq 3.75 to 4.25
        expectEquals(dfd::countMissedPlayToEnd(bank, region, makeTransport(), 3.9), 1);
        expectEquals(dfd::countMissedPlayToEnd(bank, region, makeTransport(), 3.5), 0);
        expectEquals(dfd::countMissedPlayToEnd(bank, region, makeTransport(), 4.5), 0);
    }

    void testShortPatternsAndFourPatterns()
    {
        beginTest("pattern length 8 with all four patterns on");
        dfd::PatternBank bank;
        bank.numSteps = 8;
        setPlaying(bank, true, true, true, true);
        for (int patternIndex = 0; patternIndex < dfd::kNumPatterns; ++patternIndex)
        {
            setStep(bank, patternIndex, 7, dfd::StepMode::Forward);
        }
        // every pattern lasts 2 beats, the last step of each sounds at 1.75, 3.75, 5.75, 7.75
        const std::vector<dfd::SoundStart> starts = schedule(bank, makeRegion(1000.0), 0.0, 8.0);
        expectEquals(static_cast<int>(starts.size()), 4);
        if (starts.size() == 4)
        {
            for (int index = 0; index < 4; ++index)
            {
                expectEquals(starts[static_cast<std::size_t>(index)].patternIndex, index);
                const double expectedOffset = (2.0 * index + 1.75) * kSamplesPerBeat;
                expectWithinAbsoluteError(static_cast<double>(starts[static_cast<std::size_t>(index)].sampleOffsetInBlock), expectedOffset, 1.0);
            }
        }
    }
};

static PatternChainTests s_patternChainTests;
