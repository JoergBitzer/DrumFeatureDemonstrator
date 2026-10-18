#include <atomic>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <new>
#include <vector>

#include <juce_core/juce_core.h>

#include "../src/SampleData.h"
#include "../src/SampleHolder.h"
#include "../src/VoiceBank.h"

// counts allocations while g_countAllocations is set (for the "no allocation in render" test)
namespace
{
std::atomic<bool> g_countAllocations{false};
std::atomic<int> g_numAllocations{0};
} // namespace

void* operator new(std::size_t size)
{
    if (g_countAllocations.load())
    {
        ++g_numAllocations;
    }
    void* memory = std::malloc(size);
    if (memory == nullptr)
    {
        throw std::bad_alloc();
    }
    return memory;
}

void operator delete(void* memory) noexcept
{
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace
{
constexpr double kRate48k = 48000.0;
constexpr float kFloatTolerance = 1.0e-5f;
constexpr double kPi = 3.14159265358979323846;

dfd::SampleData makeMonoSample(const std::vector<float>& values, double rate)
{
    dfd::SampleData sample;
    sample.channels.push_back(values);
    sample.sampleRate = rate;
    return sample;
}

std::vector<float> makeRamp(int length)
{
    std::vector<float> ramp;
    for (int index = 0; index < length; ++index)
    {
        ramp.push_back(static_cast<float>(index));
    }
    return ramp;
}

std::vector<float> makeSine(int length, double frequency, double rate)
{
    std::vector<float> sine;
    for (int index = 0; index < length; ++index)
    {
        sine.push_back(static_cast<float>(std::sin(2.0 * kPi * frequency * index / rate)));
    }
    return sine;
}

dfd::VoiceStart makeStart(const dfd::SampleData& sample, double ratio, bool reverse)
{
    dfd::VoiceStart voiceStart;
    voiceStart.sample = &sample;
    voiceStart.regionStart = 0;
    voiceStart.regionEnd = sample.numSamples();
    voiceStart.playbackRatio = ratio;
    voiceStart.reverse = reverse;
    voiceStart.gain = 1.0f;
    return voiceStart;
}

// renders numSamples of one mono output channel
std::vector<float> renderMono(dfd::VoiceBank& bank, int numSamples)
{
    std::vector<float> output(static_cast<std::size_t>(numSamples), 0.0f);
    float* outputs[1] = {output.data()};
    bank.render(outputs, 1, numSamples);
    return output;
}

// number of samples until the output is silent for good (the length of the sound)
int soundLength(const std::vector<float>& output)
{
    int length = 0;
    for (std::size_t index = 0; index < output.size(); ++index)
    {
        if (std::abs(output[index]) > 0.0f)
        {
            length = static_cast<int>(index) + 1;
        }
    }
    return length;
}

int countSignChanges(const std::vector<float>& values, int firstIndex, int lastIndex)
{
    int numChanges = 0;
    for (int index = firstIndex + 1; index < lastIndex; ++index)
    {
        const float before = values[static_cast<std::size_t>(index - 1)];
        const float current = values[static_cast<std::size_t>(index)];
        if ((before < 0.0f) != (current < 0.0f))
        {
            ++numChanges;
        }
    }
    return numChanges;
}
} // namespace

class SampleEngineTests : public juce::UnitTest
{
public:
    SampleEngineTests() : juce::UnitTest("SampleEngine", "DrumFeatureDemonstrator") {}

    void runTest() override
    {
        testZeroCrossing();
        testLengthAtDifferentRatios();
        testPitch();
        testReverseIsTimeReversed();
        testRegion();
        testFades();
        testStartOffsetAndGain();
        testStereoMapping();
        testPan();
        testVoiceStealing();
        testNoAllocationWhileRendering();
        testSampleHolder();
    }

private:
    void testZeroCrossing()
    {
        beginTest("zero crossing search");
        const dfd::SampleData sample = makeMonoSample({0.5f, 0.4f, 0.1f, -0.2f, -0.6f, -0.3f, 0.2f, 0.7f}, kRate48k);
        expectEquals(dfd::findNearestZeroCrossing(sample, 2, 0), 2); // crossing between 2 and 3, 0.1 is closer to zero
        expectEquals(dfd::findNearestZeroCrossing(sample, 4, 5), 2); // crossing 2/3 is one step away
        expectEquals(dfd::findNearestZeroCrossing(sample, 5, 5), 6); // crossing 5/6: 0.2 is closer to zero than -0.3
        const dfd::SampleData noCrossing = makeMonoSample({0.5f, 0.6f, 0.7f}, kRate48k);
        expectEquals(dfd::findNearestZeroCrossing(noCrossing, 1, 10), 1);
        const dfd::SampleData empty;
        expectEquals(dfd::findNearestZeroCrossing(empty, 7, 10), 7);
    }

    void testLengthAtDifferentRatios()
    {
        beginTest("length of the sound = region length / ratio");
        constexpr int kLength = 1000;
        const dfd::SampleData sample = makeMonoSample(makeRamp(kLength), kRate48k);
        const double ratios[] = {0.5, 1.0, 2.0, 1.3348, 0.25};
        for (double ratio : ratios)
        {
            dfd::VoiceBank bank;
            bank.addEvent(0, makeStart(sample, ratio, false));
            const std::vector<float> output = renderMono(bank, 6000);
            const int length = soundLength(output);
            // the last sample lies one step (1 / ratio samples) before the end of the region at kLength / ratio
            const double expectedLength = kLength / ratio;
            const double tolerance = 1.0 / ratio + 1.0;
            expect(std::abs(length - expectedLength) <= tolerance, "ratio " + juce::String(ratio) + ": length " + juce::String(length));
            expectEquals(bank.numActiveVoices(), 0);
        }
    }

    void testPitch()
    {
        beginTest("pitch: ratio 2 doubles the frequency");
        constexpr int kLength = 4800;
        constexpr double kFrequency = 1000.0;
        const dfd::SampleData sample = makeMonoSample(makeSine(kLength, kFrequency, kRate48k), kRate48k);
        dfd::VoiceBank bank;
        bank.addEvent(0, makeStart(sample, 2.0, false));
        const std::vector<float> output = renderMono(bank, kLength);
        // 2400 output samples hold 100 periods of the original = 50 ms at 2000 Hz: 100 cycles = 200 sign changes
        const int signChanges = countSignChanges(output, 0, 2400);
        expect(std::abs(signChanges - 200) <= 2, "sign changes " + juce::String(signChanges));
    }

    void testReverseIsTimeReversed()
    {
        beginTest("reverse at ratio 1 is the time-reversed sample");
        const std::vector<float> ramp = makeRamp(200);
        const dfd::SampleData sample = makeMonoSample(ramp, kRate48k);
        dfd::VoiceBank bank;
        bank.addEvent(0, makeStart(sample, 1.0, true));
        const std::vector<float> output = renderMono(bank, 300);
        // the output is 199, 198, ..., 0; the zero at the end is silent anyway
        for (int index = 0; index < 200; ++index)
        {
            expectWithinAbsoluteError(output[static_cast<std::size_t>(index)], ramp[static_cast<std::size_t>(199 - index)], kFloatTolerance);
        }
        expectWithinAbsoluteError(output[250], 0.0f, kFloatTolerance);
    }

    void testRegion()
    {
        beginTest("only the region is played");
        const dfd::SampleData sample = makeMonoSample(makeRamp(1000), kRate48k);
        dfd::VoiceStart voiceStart = makeStart(sample, 1.0, false);
        voiceStart.regionStart = 100;
        voiceStart.regionEnd = 300;
        dfd::VoiceBank bank;
        bank.addEvent(0, voiceStart);
        std::vector<float> output = renderMono(bank, 400);
        expectWithinAbsoluteError(output[0], 100.0f, kFloatTolerance);
        expectWithinAbsoluteError(output[199], 299.0f, kFloatTolerance);
        expectWithinAbsoluteError(output[200], 0.0f, kFloatTolerance);

        voiceStart.reverse = true;
        bank.addEvent(0, voiceStart);
        output = renderMono(bank, 400);
        expectWithinAbsoluteError(output[0], 299.0f, kFloatTolerance);
        expectWithinAbsoluteError(output[199], 100.0f, kFloatTolerance);
        expectWithinAbsoluteError(output[200], 0.0f, kFloatTolerance);

        voiceStart.regionEnd = voiceStart.regionStart; // empty region: silence
        bank.addEvent(0, voiceStart);
        output = renderMono(bank, 100);
        expectEquals(soundLength(output), 0);
    }

    void testFades()
    {
        beginTest("fade in and fade out of the region");
        const dfd::SampleData sample = makeMonoSample(std::vector<float>(1000, 1.0f), kRate48k);
        dfd::VoiceStart voiceStart = makeStart(sample, 1.0, false);
        voiceStart.fadeInSamples = 100.0;
        voiceStart.fadeOutSamples = 200.0;
        dfd::VoiceBank bank;
        bank.addEvent(0, voiceStart);
        std::vector<float> output = renderMono(bank, 1100);
        expectWithinAbsoluteError(output[0], 0.0f, 1.0e-3f);   // starts silent
        expectWithinAbsoluteError(output[50], 0.5f, 1.0e-3f);  // halfway up
        expectWithinAbsoluteError(output[100], 1.0f, 1.0e-3f); // full
        expectWithinAbsoluteError(output[500], 1.0f, 1.0e-3f);
        expectWithinAbsoluteError(output[800], 1.0f, 1.0e-3f); // the fade out starts 200 samples before the end
        expectWithinAbsoluteError(output[900], 0.5f, 1.0e-3f);
        expect(output[999] < 0.01f);

        // reverse: the envelope belongs to the sample (the waveform), so a reversed sound starts inside the fade out
        voiceStart.reverse = true;
        bank.addEvent(0, voiceStart);
        output = renderMono(bank, 1100);
        expect(output[0] < 0.01f);
        expectWithinAbsoluteError(output[100], 0.5f, 0.01f);   // position 899: (1000 - 899) / 200
        expectWithinAbsoluteError(output[500], 1.0f, 1.0e-3f);
        expect(output[999] < 0.01f);
    }

    void testStartOffsetAndGain()
    {
        beginTest("sample-exact start offset and gain");
        const dfd::SampleData sample = makeMonoSample(std::vector<float>(100, 1.0f), kRate48k);
        dfd::VoiceStart voiceStart = makeStart(sample, 1.0, false);
        voiceStart.gain = 0.5f;
        dfd::VoiceBank bank;
        bank.addEvent(37, voiceStart);
        const std::vector<float> output = renderMono(bank, 200);
        expectWithinAbsoluteError(output[36], 0.0f, kFloatTolerance);
        expectWithinAbsoluteError(output[37], 0.5f, kFloatTolerance);
        expectWithinAbsoluteError(output[136], 0.5f, kFloatTolerance);
        expectWithinAbsoluteError(output[137], 0.0f, kFloatTolerance);

        // events in the wrong order are sorted; two voices add up
        dfd::VoiceBank sortedBank;
        sortedBank.addEvent(50, voiceStart);
        sortedBank.addEvent(10, voiceStart);
        const std::vector<float> sorted = renderMono(sortedBank, 200);
        expectWithinAbsoluteError(sorted[9], 0.0f, kFloatTolerance);
        expectWithinAbsoluteError(sorted[10], 0.5f, kFloatTolerance);
        expectWithinAbsoluteError(sorted[50], 1.0f, kFloatTolerance);
    }

    void testStereoMapping()
    {
        beginTest("mono sample on two outputs, stereo sample on two outputs");
        const dfd::SampleData mono = makeMonoSample(std::vector<float>(10, 0.25f), kRate48k);
        std::vector<float> left(20, 0.0f);
        std::vector<float> right(20, 0.0f);
        float* outputs[2] = {left.data(), right.data()};
        dfd::VoiceBank bank;
        bank.addEvent(0, makeStart(mono, 1.0, false));
        bank.render(outputs, 2, 20);
        expectWithinAbsoluteError(left[5], 0.25f, kFloatTolerance);
        expectWithinAbsoluteError(right[5], 0.25f, kFloatTolerance);

        dfd::SampleData stereo;
        stereo.sampleRate = kRate48k;
        stereo.channels.push_back(std::vector<float>(10, 0.1f));
        stereo.channels.push_back(std::vector<float>(10, 0.9f));
        std::fill(left.begin(), left.end(), 0.0f);
        std::fill(right.begin(), right.end(), 0.0f);
        bank.addEvent(0, makeStart(stereo, 1.0, false));
        bank.render(outputs, 2, 20);
        expectWithinAbsoluteError(left[5], 0.1f, kFloatTolerance);
        expectWithinAbsoluteError(right[5], 0.9f, kFloatTolerance);
    }

    void testPan()
    {
        beginTest("pan: balance law, the centre keeps the level");
        const dfd::PanGains centre = dfd::panGains(0.0f);
        expectWithinAbsoluteError(centre.left, 1.0f, kFloatTolerance);
        expectWithinAbsoluteError(centre.right, 1.0f, kFloatTolerance);
        const dfd::PanGains left = dfd::panGains(-1.0f);
        expectWithinAbsoluteError(left.left, 1.0f, kFloatTolerance);
        expectWithinAbsoluteError(left.right, 0.0f, kFloatTolerance);
        const dfd::PanGains halfRight = dfd::panGains(0.5f);
        expectWithinAbsoluteError(halfRight.left, 0.5f, kFloatTolerance);
        expectWithinAbsoluteError(halfRight.right, 1.0f, kFloatTolerance);
        const dfd::PanGains beyond = dfd::panGains(7.0f); // clamped
        expectWithinAbsoluteError(beyond.left, 0.0f, kFloatTolerance);

        const dfd::SampleData mono = makeMonoSample(std::vector<float>(10, 0.5f), kRate48k);
        std::vector<float> leftOutput(20, 0.0f);
        std::vector<float> rightOutput(20, 0.0f);
        float* outputs[2] = {leftOutput.data(), rightOutput.data()};
        dfd::VoiceBank bank;
        dfd::VoiceStart voiceStart = makeStart(mono, 1.0, false);
        voiceStart.pan = 0.5f;
        bank.addEvent(0, voiceStart);
        bank.render(outputs, 2, 20);
        expectWithinAbsoluteError(leftOutput[5], 0.25f, kFloatTolerance);  // 0.5 * 0.5
        expectWithinAbsoluteError(rightOutput[5], 0.5f, kFloatTolerance);

        // a mono output ignores the pan
        std::vector<float> monoOutput(20, 0.0f);
        float* monoOutputs[1] = {monoOutput.data()};
        bank.addEvent(0, voiceStart);
        bank.render(monoOutputs, 1, 20);
        expectWithinAbsoluteError(monoOutput[5], 0.5f, kFloatTolerance);
    }

    void testVoiceStealing()
    {
        beginTest("the oldest voice is stolen");
        const dfd::SampleData sample = makeMonoSample(std::vector<float>(100000, 1.0f), kRate48k);
        dfd::VoiceBank bank;
        for (int index = 0; index < dfd::kNumVoices + 4; ++index)
        {
            bank.addEvent(index, makeStart(sample, 1.0, false));
        }
        renderMono(bank, 100);
        expectEquals(bank.numActiveVoices(), dfd::kNumVoices);
        bank.stopAll();
        expectEquals(bank.numActiveVoices(), 0);
    }

    void testNoAllocationWhileRendering()
    {
        beginTest("no allocation while rendering");
        const dfd::SampleData sample = makeMonoSample(makeRamp(5000), kRate48k);
        dfd::VoiceBank bank;
        std::vector<float> output(512, 0.0f);
        float* outputs[1] = {output.data()};

        g_numAllocations = 0;
        g_countAllocations = true;
        for (int blockIndex = 0; blockIndex < 20; ++blockIndex)
        {
            bank.addEvent(blockIndex, makeStart(sample, 1.1, blockIndex % 2 == 0));
            bank.addEvent(300 - blockIndex, makeStart(sample, 0.7, false));
            bank.render(outputs, 1, 512);
        }
        g_countAllocations = false;
        expectEquals(g_numAllocations.load(), 0);
    }

    void testSampleHolder()
    {
        beginTest("sample holder hands over without freeing in the audio thread");
        dfd::SampleHolder holder;
        expect(holder.getActiveSample() == nullptr);
        expect(holder.updateActiveSample() == false);

        auto first = std::make_shared<dfd::SampleData>(makeMonoSample({1.0f}, kRate48k));
        std::weak_ptr<const dfd::SampleData> weakFirst = first;
        holder.setSample(first);
        first.reset();
        expect(holder.updateActiveSample());
        expect(holder.getActiveSample() != nullptr);
        expect(holder.updateActiveSample() == false);

        auto second = std::make_shared<dfd::SampleData>(makeMonoSample({2.0f}, kRate48k));
        holder.setSample(second);
        second.reset();
        expect(holder.updateActiveSample());
        expectEquals(holder.getActiveSample()->valueAt(0, 0), 2.0f);
        expect(weakFirst.expired() == false); // after the swap the audio thread did not free it

        auto third = std::make_shared<dfd::SampleData>(makeMonoSample({3.0f}, kRate48k));
        holder.setSample(third); // message thread: now the first sample is freed
        expect(weakFirst.expired());
    }
};

static SampleEngineTests s_sampleEngineTests;
