#include <cmath>
#include <vector>

#include <juce_core/juce_core.h>

#include "../src/Sequencer.h"
#include "../src/VoiceBank.h"

namespace
{
constexpr double kRate48k = 48000.0;
constexpr double kBpm120 = 120.0;
constexpr double kSamplesPerBeat120At48k = 24000.0;
constexpr double kSamplesPerStep120At48k = 6000.0;
constexpr int kBlockSize = 512;

dfd::SampleRegion makeRegion(double length, double rate)
{
    dfd::SampleRegion region;
    region.lengthInFileSamples = length;
    region.fileSampleRate = rate;
    return region;
}

dfd::BlockTransport makeTransport(double positionPpq, double bpm)
{
    dfd::BlockTransport transport;
    transport.isPlaying = true;
    transport.hasPosition = true;
    transport.positionPpq = positionPpq;
    transport.bpm = bpm;
    return transport;
}

void setStep(dfd::Pattern& pattern, int stepIndex, dfd::StepMode mode, double pitch)
{
    dfd::StepSettings& step = dfd::stepAt(pattern, stepIndex);
    step.active = true;
    step.mode = mode;
    step.noteNumber = pattern.baseNote + pitch;
}

int totalSounds(dfd::Sequencer& sequencer, const dfd::Pattern& pattern, const dfd::SampleRegion& region,
                double startPpq, int numBlocks, int blockSize, int& totalMissed)
{
    std::vector<dfd::SoundStart> soundStarts;
    soundStarts.reserve(dfd::kMaxSteps);
    dfd::SequencerResult result;
    int numSounds = 0;
    double positionPpq = startPpq;
    totalMissed = 0;
    for (int blockIndex = 0; blockIndex < numBlocks; ++blockIndex)
    {
        sequencer.process(pattern, region, kRate48k, makeTransport(positionPpq, kBpm120), blockSize, soundStarts, result);
        numSounds += static_cast<int>(soundStarts.size());
        totalMissed += result.missedPlayToEnd;
        positionPpq += blockSize / kSamplesPerBeat120At48k;
    }
    return numSounds;
}
} // namespace

class SequencerTests : public juce::UnitTest
{
public:
    SequencerTests() : juce::UnitTest("Sequencer", "DrumFeatureDemonstrator") {}

    void runTest() override
    {
        testNotPlaying();
        testContinuousPlayback();
        testMissedAtStartAndJump();
        testCurrentStep();
        testInternalClock();
        testPlayToEndEndsOnTheStepBoundary();
    }

private:
    void testNotPlaying()
    {
        beginTest("nothing is scheduled while the transport stands");
        dfd::Pattern pattern;
        setStep(pattern, 0, dfd::StepMode::Forward, 0.0);
        dfd::Sequencer sequencer;
        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        dfd::SequencerResult result;
        dfd::BlockTransport transport = makeTransport(0.0, kBpm120);
        transport.isPlaying = false;
        sequencer.process(pattern, makeRegion(1000.0, kRate48k), kRate48k, transport, kBlockSize, soundStarts, result);
        expectEquals(static_cast<int>(soundStarts.size()), 0);
        expectEquals(result.currentStep, -1);

        transport.isPlaying = true;
        transport.hasPosition = false;
        sequencer.process(pattern, makeRegion(1000.0, kRate48k), kRate48k, transport, kBlockSize, soundStarts, result);
        expectEquals(static_cast<int>(soundStarts.size()), 0);
    }

    void testContinuousPlayback()
    {
        beginTest("continuous playback: every sound once, no notification");
        dfd::Pattern pattern;
        setStep(pattern, 0, dfd::StepMode::Forward, 0.0);
        setStep(pattern, 4, dfd::StepMode::Reverse, 0.0);
        setStep(pattern, 8, dfd::StepMode::Forward, 7.0);
        setStep(pattern, 12, dfd::StepMode::Forward, -7.0);
        dfd::Sequencer sequencer;
        const int blocksPerPass = static_cast<int>(4 * kSamplesPerBeat120At48k / kBlockSize);
        int missed = 0;
        const int numSounds = totalSounds(sequencer, pattern, makeRegion(3000.0, kRate48k), 0.0, 4 * blocksPerPass, kBlockSize, missed);
        // 4 passes of 4 sounds, the end of the last pass is not complete: at least 3 passes
        expect(numSounds >= 12 && numSounds <= 16, "sounds " + juce::String(numSounds));
        expectEquals(missed, 0);
    }

    void testMissedAtStartAndJump()
    {
        beginTest("a start or jump reports the play-to-end sounds that are already running");
        dfd::Pattern pattern;
        setStep(pattern, 15, dfd::StepMode::ReverseToEnd, 0.0);
        const dfd::SampleRegion region = makeRegion(2 * kSamplesPerStep120At48k, kRate48k); // 2 steps: ppq 3.5 .. 4.0

        dfd::Sequencer sequencer;
        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        dfd::SequencerResult result;
        sequencer.process(pattern, region, kRate48k, makeTransport(3.75, kBpm120), kBlockSize, soundStarts, result);
        expectEquals(result.missedPlayToEnd, 1);
        expectEquals(static_cast<int>(soundStarts.size()), 0);

        // the next block follows without a gap: no notification
        const double nextPosition = 3.75 + kBlockSize / kSamplesPerBeat120At48k;
        sequencer.process(pattern, region, kRate48k, makeTransport(nextPosition, kBpm120), kBlockSize, soundStarts, result);
        expectEquals(result.missedPlayToEnd, 0);

        // a jump back into the run-up
        sequencer.process(pattern, region, kRate48k, makeTransport(3.8, kBpm120), kBlockSize, soundStarts, result);
        expectEquals(result.missedPlayToEnd, 1);

        // after a stop the next block counts as a start again
        dfd::BlockTransport stopped = makeTransport(3.8, kBpm120);
        stopped.isPlaying = false;
        sequencer.process(pattern, region, kRate48k, stopped, kBlockSize, soundStarts, result);
        sequencer.process(pattern, region, kRate48k, makeTransport(3.8, kBpm120), kBlockSize, soundStarts, result);
        expectEquals(result.missedPlayToEnd, 1);

        // a start before the run-up misses nothing, and the sound is scheduled later
        sequencer.process(pattern, region, kRate48k, makeTransport(2.0, kBpm120), kBlockSize, soundStarts, result);
        expectEquals(result.missedPlayToEnd, 0);
    }

    void testCurrentStep()
    {
        beginTest("current step");
        dfd::Pattern pattern;
        dfd::Sequencer sequencer;
        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        dfd::SequencerResult result;
        const dfd::SampleRegion region = makeRegion(1000.0, kRate48k);
        sequencer.process(pattern, region, kRate48k, makeTransport(0.0, kBpm120), kBlockSize, soundStarts, result);
        expectEquals(result.currentStep, 0);
        sequencer.process(pattern, region, kRate48k, makeTransport(1.3, kBpm120), kBlockSize, soundStarts, result);
        expectEquals(result.currentStep, 5);
        sequencer.process(pattern, region, kRate48k, makeTransport(4.25, kBpm120), kBlockSize, soundStarts, result);
        expectEquals(result.currentStep, 1); // second pass
        pattern.numSteps = 11;
        sequencer.process(pattern, region, kRate48k, makeTransport(3.0, kBpm120), kBlockSize, soundStarts, result);
        expectEquals(result.currentStep, 1); // step 12 of 11
    }

    void testInternalClock()
    {
        beginTest("internal clock");
        dfd::InternalClock clock;
        clock.setBpm(kBpm120);
        clock.advance(24000, kRate48k);
        expectWithinAbsoluteError(clock.getTransport().positionPpq, 0.0, 1.0e-12); // stands while not playing
        expect(clock.getTransport().isPlaying == false);
        clock.setPlaying(true);
        clock.advance(24000, kRate48k);
        expectWithinAbsoluteError(clock.getTransport().positionPpq, 1.0, 1.0e-9);
        clock.setBpm(60.0);
        clock.advance(48000, kRate48k);
        expectWithinAbsoluteError(clock.getTransport().positionPpq, 2.0, 1.0e-9);
        clock.setPlaying(false);
        clock.advance(48000, kRate48k);
        expectWithinAbsoluteError(clock.getTransport().positionPpq, 2.0, 1.0e-9); // pause keeps the position
        clock.rewind();
        expectWithinAbsoluteError(clock.getTransport().positionPpq, 0.0, 1.0e-12);
    }

    // The whole chain without JUCE audio: clock -> sequencer -> voice bank -> output.
    // A reversed sample with a distinct last sample must end exactly at the end of its step.
    void testPlayToEndEndsOnTheStepBoundary()
    {
        beginTest("rendered: the reversed sound ends at the end of its step");
        const double pitches[] = {0.0, 5.0, -7.0, 12.0, -12.0};
        const double sampleRates[] = {48000.0, 44100.0};
        for (double pitch : pitches)
        {
            for (double fileRate : sampleRates)
            {
                checkRenderedEnd(pitch, fileRate);
            }
        }
    }

    void checkRenderedEnd(double pitch, double fileRate)
    {
        constexpr int kSampleLength = 9000;
        constexpr int kLastStepIndex = 15;
        constexpr int kNumPasses = 3;

        dfd::SampleData sample;
        sample.sampleRate = fileRate;
        sample.channels.push_back(std::vector<float>(kSampleLength, 1.0f));

        dfd::Pattern pattern;
        setStep(pattern, kLastStepIndex, dfd::StepMode::ReverseToEnd, pitch);
        const dfd::SampleRegion region = makeRegion(kSampleLength, fileRate);

        dfd::Sequencer sequencer;
        dfd::VoiceBank voiceBank;
        dfd::InternalClock clock;
        clock.setBpm(kBpm120);
        clock.setPlaying(true);

        const int totalSamples = static_cast<int>(kNumPasses * 4 * kSamplesPerBeat120At48k);
        std::vector<float> output(static_cast<std::size_t>(totalSamples), 0.0f);
        std::vector<dfd::SoundStart> soundStarts;
        soundStarts.reserve(dfd::kMaxSteps);
        dfd::SequencerResult result;

        for (int position = 0; position < totalSamples; position += kBlockSize)
        {
            const int numSamples = std::min(kBlockSize, totalSamples - position);
            sequencer.process(pattern, region, kRate48k, clock.getTransport(), numSamples, soundStarts, result);
            for (const dfd::SoundStart& soundStart : soundStarts)
            {
                dfd::VoiceStart voiceStart;
                voiceStart.sample = &sample;
                voiceStart.regionStart = 0;
                voiceStart.regionEnd = kSampleLength;
                voiceStart.playbackRatio = soundStart.playbackRatio;
                voiceStart.reverse = soundStart.reverse;
                voiceStart.gain = soundStart.gain;
                voiceBank.addEvent(soundStart.sampleOffsetInBlock, voiceStart);
            }
            float* outputs[1] = {output.data() + position};
            voiceBank.render(outputs, 1, numSamples);
            clock.advance(numSamples, kRate48k);
        }

        // the sound of the last step of pass 0 starts before 0: skipped. Passes 1 and 2 play.
        // the end of the step of pass p: (p + 1) * 4 beats
        for (int pass = 1; pass < kNumPasses; ++pass)
        {
            const double endOfStep = (pass + 1) * 4.0 * kSamplesPerBeat120At48k;
            const int lastSoundSample = lastNonZeroBefore(output, static_cast<int>(endOfStep) + kBlockSize, static_cast<int>(endOfStep) - 2 * kBlockSize);
            const double duration = kSampleLength / dfd::playbackRatio(pitch, fileRate, kRate48k);
            // the last played sample lies one resampling step before the end of the region
            const double tolerance = 1.0 / dfd::playbackRatio(pitch, fileRate, kRate48k) + 1.5;
            expect(std::abs(lastSoundSample + 1 - endOfStep) <= tolerance,
                   "pitch " + juce::String(pitch) + " rate " + juce::String(fileRate) + " pass " + juce::String(pass)
                       + ": last sample " + juce::String(lastSoundSample) + " expected end " + juce::String(endOfStep)
                       + " duration " + juce::String(duration));
        }
    }

    static int lastNonZeroBefore(const std::vector<float>& values, int searchEnd, int searchStart)
    {
        int last = -1;
        const int limit = std::min(searchEnd, static_cast<int>(values.size()));
        for (int index = std::max(searchStart, 0); index < limit; ++index)
        {
            if (std::abs(values[static_cast<std::size_t>(index)]) > 0.0f)
            {
                last = index;
            }
        }
        return last;
    }
};

static SequencerTests s_sequencerTests;
