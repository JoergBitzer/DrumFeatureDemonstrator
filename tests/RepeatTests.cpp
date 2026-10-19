#include <cmath>
#include <vector>

#include <juce_core/juce_core.h>

#include "../src/StepScheduler.h"

namespace
{
constexpr double kRate48k = 48000.0;
constexpr double kBpm120 = 120.0;
constexpr double kSamplesPerBeat = 24000.0; // 120 bpm at 48 kHz
constexpr double kSamplesPerStep = 6000.0;  // a 1/16 step
constexpr double kTinyTolerance = 1.0e-9;
constexpr double kStrongRamp = 1.0;

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

std::vector<dfd::SoundStart> scheduleFirstBeat(const dfd::PatternBank& bank, const dfd::SampleRegion& region)
{
    std::vector<dfd::SoundStart> soundStarts;
    soundStarts.reserve(dfd::kMaxSoundStartsPerBlock);
    dfd::scheduleBlock(bank, region, makeTransport(), 0.0, static_cast<int>(kSamplesPerBeat), soundStarts);
    return soundStarts;
}

dfd::PatternBank makeBankWithStepZero(dfd::StepMode mode, int repeat, float ramp)
{
    dfd::PatternBank bank;
    dfd::StepSettings& step = dfd::stepAt(bank, 0, 0);
    step.active = true;
    step.mode = mode;
    step.repeat = repeat;
    step.ramp = ramp;
    return bank;
}
} // namespace

class RepeatTests : public juce::UnitTest
{
public:
    RepeatTests() : juce::UnitTest("Repeat", "DrumFeatureDemonstrator") {}

    void runTest() override
    {
        testFractions();
        testEvenRepeats();
        testRamp();
        testPlayToEndRepeats();
        testMissedPlayToEnd();
    }

private:
    void testFractions()
    {
        beginTest("repeat fractions: even spacing, the first hit at the start, the last part ends at the step end");
        for (int hitIndex = 0; hitIndex < 4; ++hitIndex)
        {
            expectWithinAbsoluteError(dfd::repeatStartFraction(4, 0.0, hitIndex), hitIndex / 4.0, kTinyTolerance);
        }
        expectWithinAbsoluteError(dfd::repeatEndFraction(4, 0.0, 3), 1.0, kTinyTolerance);
        expectWithinAbsoluteError(dfd::repeatEndFraction(4, kStrongRamp, 3), 1.0, kTinyTolerance);
        expectWithinAbsoluteError(dfd::repeatStartFraction(1, 0.5, 0), 0.0, kTinyTolerance);
        expectWithinAbsoluteError(dfd::repeatStartFraction(20, 0.0, 10), 1.0, kTinyTolerance); // 20 is limited to 8 repeats, hit 10 is past the end
    }

    void testEvenRepeats()
    {
        beginTest("4 even repeats of a forward step: four sounds a quarter of the step apart");
        const std::vector<dfd::SoundStart> soundStarts = scheduleFirstBeat(makeBankWithStepZero(dfd::StepMode::Forward, 4, 0.0f), makeRegion(kSamplesPerStep));
        expectEquals(static_cast<int>(soundStarts.size()), 4);
        for (std::size_t index = 0; index < soundStarts.size(); ++index)
        {
            expectEquals(soundStarts[index].sampleOffsetInBlock, static_cast<int>(static_cast<double>(index) * kSamplesPerStep / 4.0));
            expectEquals(soundStarts[index].stepIndex, 0);
        }

        beginTest("repeat 1 and a repeat above the limit: one sound, and at most eight sounds");
        expectEquals(static_cast<int>(scheduleFirstBeat(makeBankWithStepZero(dfd::StepMode::Forward, 1, 0.0f), makeRegion(kSamplesPerStep)).size()), 1);
        expectEquals(static_cast<int>(scheduleFirstBeat(makeBankWithStepZero(dfd::StepMode::Forward, 99, 0.0f), makeRegion(kSamplesPerStep)).size()), 8);
    }

    void testRamp()
    {
        beginTest("ramp: positive speeds up (gaps shrink), negative slows down (gaps grow), the first hit stays at the start");
        const std::vector<dfd::SoundStart> faster = scheduleFirstBeat(makeBankWithStepZero(dfd::StepMode::Forward, 4, 1.0f), makeRegion(kSamplesPerStep));
        const std::vector<dfd::SoundStart> slower = scheduleFirstBeat(makeBankWithStepZero(dfd::StepMode::Forward, 4, -1.0f), makeRegion(kSamplesPerStep));
        expectEquals(static_cast<int>(faster.size()), 4);
        expectEquals(static_cast<int>(slower.size()), 4);
        expectEquals(faster[0].sampleOffsetInBlock, 0);
        expectEquals(slower[0].sampleOffsetInBlock, 0);
        for (std::size_t index = 2; index < 4; ++index)
        {
            const int previousFasterGap = faster[index - 1].sampleOffsetInBlock - faster[index - 2].sampleOffsetInBlock;
            const int fasterGap = faster[index].sampleOffsetInBlock - faster[index - 1].sampleOffsetInBlock;
            expect(fasterGap < previousFasterGap);
            const int previousSlowerGap = slower[index - 1].sampleOffsetInBlock - slower[index - 2].sampleOffsetInBlock;
            const int slowerGap = slower[index].sampleOffsetInBlock - slower[index - 1].sampleOffsetInBlock;
            expect(slowerGap > previousSlowerGap);
        }
        expect(faster[3].sampleOffsetInBlock < static_cast<int>(kSamplesPerStep)); // all hits stay inside the step
        expect(slower[3].sampleOffsetInBlock < static_cast<int>(kSamplesPerStep));
    }

    void testPlayToEndRepeats()
    {
        beginTest("reverse + play to end with 2 repeats: each hit ends at the end of its part of the step");
        const double sampleLength = kSamplesPerStep / 2.0; // 3000 samples
        const std::vector<dfd::SoundStart> soundStarts = scheduleFirstBeat(makeBankWithStepZero(dfd::StepMode::ReverseToEnd, 2, 0.0f), makeRegion(sampleLength));
        expectEquals(static_cast<int>(soundStarts.size()), 2);
        // parts end at 3000 and 6000, the sample is 3000 long: the hits start at 0 and 3000
        expectEquals(soundStarts[0].sampleOffsetInBlock, 0);
        expectEquals(soundStarts[1].sampleOffsetInBlock, static_cast<int>(kSamplesPerStep / 2.0));
        expect(soundStarts[0].playsToEnd && soundStarts[1].playsToEnd);
    }

    void testMissedPlayToEnd()
    {
        beginTest("missed play-to-end sounds are counted for every repeat");
        const double sampleLength = kSamplesPerStep / 2.0;
        const dfd::PatternBank bank = makeBankWithStepZero(dfd::StepMode::ReverseToEnd, 2, 0.0f);
        const dfd::SampleRegion region = makeRegion(sampleLength);
        const double stepBeats = dfd::kDefaultStepLengthInBeats;
        // a quarter of the step into the first hit (which runs from 0 to half a step): one is running
        expectEquals(dfd::countMissedPlayToEnd(bank, region, makeTransport(), stepBeats * 0.25), 1);
        // three quarters into the step: the second hit (half to the full step) is running
        expectEquals(dfd::countMissedPlayToEnd(bank, region, makeTransport(), stepBeats * 0.75), 1);
    }
};

static RepeatTests s_repeatTests;
