#include <cstdint>
#include <vector>

#include <juce_core/juce_core.h>

#include "../src/PatternData.h"

namespace
{
constexpr std::size_t kBytesPerValue = 4;
constexpr std::size_t kNumSettingsInBlob = 4;
constexpr std::size_t kHeaderSize = 8 + kNumSettingsInBlob * kBytesPerValue;
constexpr std::size_t kExpectedBlobSize = kHeaderSize + 4 * 32 * 5 * kBytesPerValue;
constexpr float kTinyTolerance = 1.0e-6f;
constexpr std::size_t kPlayMaskIndex = 7;
constexpr std::size_t kFirstSettingIndex = 8;
constexpr std::size_t kFirstValueIndex = kHeaderSize;
constexpr std::size_t kSyncSettingIndex = kFirstSettingIndex + 2 * kBytesPerValue; // length, note value, sync to host
constexpr std::uint8_t kFloatOneBytes[kBytesPerValue] = {0x00, 0x00, 0x80, 0x3F}; // 1.0f, little endian
constexpr float kOddNote = 61.4f;
constexpr float kOutOfRangeVelocity = 500.0f;
constexpr float kOutOfRangePan = -3.0f;
constexpr float kBetweenModes = 1.6f;
} // namespace

class PatternDataTests : public juce::UnitTest
{
public:
    PatternDataTests() : juce::UnitTest("PatternData", "DrumFeatureDemonstrator") {}

    void runTest() override
    {
        testDefaults();
        testLimits();
        testSettings();
        testFillBank();
        testBlobRoundTrip();
        testBlobLayout();
        testInvalidBlobs();
    }

private:
    void testDefaults()
    {
        beginTest("defaults: all steps off, default values, only pattern 1 plays");
        dfd::PatternData data;
        for (int patternIndex = 0; patternIndex < dfd::kNumPatterns; ++patternIndex)
        {
            expect(data.isPlaying(patternIndex) == (patternIndex == 0));
            for (int stepIndex = 0; stepIndex < dfd::kMaxSteps; ++stepIndex)
            {
                expectEquals(data.getValue(patternIndex, stepIndex, dfd::StepParam::Active), 0.0f);
                expectEquals(data.getValue(patternIndex, stepIndex, dfd::StepParam::Note), dfd::kDefaultStepNote);
                expectEquals(data.getValue(patternIndex, stepIndex, dfd::StepParam::Velocity), dfd::kDefaultVelocity);
                expectEquals(data.getValue(patternIndex, stepIndex, dfd::StepParam::Pan), dfd::kCentrePan);
            }
        }
    }

    void testLimits()
    {
        beginTest("values are limited to their range, active and mode are rounded");
        dfd::PatternData data;
        data.setValue(0, 0, dfd::StepParam::Velocity, kOutOfRangeVelocity);
        expectEquals(data.getValue(0, 0, dfd::StepParam::Velocity), dfd::kMaxVelocity);
        data.setValue(0, 0, dfd::StepParam::Pan, kOutOfRangePan);
        expectEquals(data.getValue(0, 0, dfd::StepParam::Pan), dfd::kMinPan);
        data.setValue(0, 0, dfd::StepParam::Mode, kBetweenModes);
        expectEquals(data.getValue(0, 0, dfd::StepParam::Mode), dfd::kModeReverseToEnd);
        data.setValue(0, 0, dfd::StepParam::Note, kOddNote);
        expectWithinAbsoluteError(data.getValue(0, 0, dfd::StepParam::Note), kOddNote, kTinyTolerance);
        data.setValue(3, 31, dfd::StepParam::Active, 1.0f);
        expectEquals(data.getValue(3, 31, dfd::StepParam::Active), 1.0f);
        expectEquals(data.getValue(3, 30, dfd::StepParam::Active), 0.0f); // the neighbour is untouched
    }

    void testSettings()
    {
        beginTest("settings: defaults, limits, rounding");
        dfd::PatternData data;
        expectEquals(data.getSetting(dfd::PatternSetting::Length), 16.0f);
        expectEquals(data.getSetting(dfd::PatternSetting::NoteValue), 12.0f);
        expectEquals(data.getSetting(dfd::PatternSetting::SyncToHost), 1.0f);
        expectEquals(data.getSetting(dfd::PatternSetting::Declick), 0.0f);
        data.setSetting(dfd::PatternSetting::Length, 100.0f);
        expectEquals(data.getSetting(dfd::PatternSetting::Length), 32.0f);
        data.setSetting(dfd::PatternSetting::Length, 2.0f);
        expectEquals(data.getSetting(dfd::PatternSetting::Length), 8.0f);
        data.setSetting(dfd::PatternSetting::NoteValue, 11.6f);
        expectEquals(data.getSetting(dfd::PatternSetting::NoteValue), 12.0f);
        data.setSetting(dfd::PatternSetting::NoteValue, 99.0f);
        expectEquals(data.getSetting(dfd::PatternSetting::NoteValue), 20.0f);

        beginTest("settings are part of the blob");
        data.setSetting(dfd::PatternSetting::Length, 12.0f);
        data.setSetting(dfd::PatternSetting::NoteValue, 11.0f);
        data.setSetting(dfd::PatternSetting::SyncToHost, 0.0f);
        data.setSetting(dfd::PatternSetting::Declick, 1.0f);
        const std::vector<std::uint8_t> bytes = data.toBytes();
        dfd::PatternData restored;
        expect(restored.fromBytes(bytes.data(), bytes.size()));
        expectEquals(restored.getSetting(dfd::PatternSetting::Length), 12.0f);
        expectEquals(restored.getSetting(dfd::PatternSetting::NoteValue), 11.0f);
        expectEquals(restored.getSetting(dfd::PatternSetting::SyncToHost), 0.0f);
        expectEquals(restored.getSetting(dfd::PatternSetting::Declick), 1.0f);
    }

    void testFillBank()
    {
        beginTest("fillBank copies steps and play switches (mode 0, 1, 2 become forward, reverse, reverse to end)");
        dfd::PatternData data;
        data.setPlaying(0, false);
        data.setPlaying(2, true);
        data.setValue(2, 5, dfd::StepParam::Active, 1.0f);
        data.setValue(2, 5, dfd::StepParam::Mode, dfd::kModeReverse);
        data.setValue(2, 5, dfd::StepParam::Note, 67.0f);
        data.setValue(2, 5, dfd::StepParam::Velocity, 64.0f);
        data.setValue(2, 5, dfd::StepParam::Pan, 0.5f);
        data.setValue(1, 0, dfd::StepParam::Active, 1.0f);
        data.setValue(1, 0, dfd::StepParam::Mode, dfd::kModeReverseToEnd);

        dfd::PatternBank bank;
        data.fillBank(bank);
        expect(bank.playing[0] == false);
        expect(bank.playing[2] == true);
        const dfd::StepSettings& step = dfd::stepAt(bank, 2, 5);
        expect(step.active);
        expect(step.mode == dfd::StepMode::Reverse);
        expectEquals(step.noteNumber, 67.0);
        expectEquals(step.velocity, 64.0f);
        expectEquals(step.pan, 0.5f);
        expect(dfd::stepAt(bank, 1, 0).mode == dfd::StepMode::ReverseToEnd);
        expect(dfd::stepAt(bank, 0, 0).active == false);
    }

    void testBlobRoundTrip()
    {
        beginTest("blob: what toBytes writes, fromBytes restores");
        dfd::PatternData original;
        original.setPlaying(1, true);
        original.setPlaying(3, true);
        original.setValue(1, 7, dfd::StepParam::Active, 1.0f);
        original.setValue(1, 7, dfd::StepParam::Mode, dfd::kModeReverseToEnd);
        original.setValue(1, 7, dfd::StepParam::Note, 72.0f);
        original.setValue(1, 7, dfd::StepParam::Velocity, 33.0f);
        original.setValue(1, 7, dfd::StepParam::Pan, -0.25f);
        const std::vector<std::uint8_t> bytes = original.toBytes();

        dfd::PatternData restored;
        const unsigned loadCountBefore = restored.getLoadCount();
        expect(restored.fromBytes(bytes.data(), bytes.size()));
        expect(restored.getLoadCount() != loadCountBefore);
        expect(restored.toBytes() == bytes);
        expect(restored.isPlaying(1) && restored.isPlaying(3) && restored.isPlaying(0) == original.isPlaying(0));
        expectEquals(restored.getValue(1, 7, dfd::StepParam::Note), 72.0f);
        expectEquals(restored.getValue(1, 7, dfd::StepParam::Pan), -0.25f);
    }

    void testBlobLayout()
    {
        beginTest("blob: fixed layout (magic, version, counts, play mask, little endian floats)");
        dfd::PatternData data;
        data.setValue(0, 0, dfd::StepParam::Active, 1.0f);
        const std::vector<std::uint8_t> bytes = data.toBytes();
        expectEquals(static_cast<int>(bytes.size()), static_cast<int>(kExpectedBlobSize));
        expect(bytes[0] == 'D' && bytes[1] == 'F' && bytes[2] == 'D' && bytes[3] == 'P');
        expectEquals(static_cast<int>(bytes[4]), 2);   // version
        expectEquals(static_cast<int>(bytes[5]), 4);   // patterns
        expectEquals(static_cast<int>(bytes[6]), 32);  // steps
        expectEquals(static_cast<int>(bytes[kPlayMaskIndex]), 1); // only pattern 1 plays
        for (std::size_t byteIndex = 0; byteIndex < kBytesPerValue; ++byteIndex)
        {
            expectEquals(static_cast<int>(bytes[kFirstValueIndex + byteIndex]), static_cast<int>(kFloatOneBytes[byteIndex]));
            expectEquals(static_cast<int>(bytes[kSyncSettingIndex + byteIndex]), static_cast<int>(kFloatOneBytes[byteIndex])); // sync is on by default
        }
    }

    void testInvalidBlobs()
    {
        beginTest("a blob that is wrong in size, magic, version or counts is refused and changes nothing");
        dfd::PatternData data;
        data.setValue(0, 0, dfd::StepParam::Velocity, 42.0f);
        const std::vector<std::uint8_t> good = dfd::PatternData().toBytes();

        expect(data.fromBytes(nullptr, 0) == false);
        expect(data.fromBytes(good.data(), good.size() - 1) == false);

        std::vector<std::uint8_t> badMagic = good;
        badMagic[0] = 'X';
        expect(data.fromBytes(badMagic.data(), badMagic.size()) == false);

        std::vector<std::uint8_t> badVersion = good;
        badVersion[4] = 99;
        expect(data.fromBytes(badVersion.data(), badVersion.size()) == false);

        std::vector<std::uint8_t> badCount = good;
        badCount[6] = 16;
        expect(data.fromBytes(badCount.data(), badCount.size()) == false);

        expectEquals(data.getValue(0, 0, dfd::StepParam::Velocity), 42.0f);

        beginTest("values of a blob are limited to their ranges (a hand-made blob cannot break the player)");
        std::vector<std::uint8_t> wild = good;
        const std::size_t velocityIndex = kFirstValueIndex + 3 * kBytesPerValue; // pattern 1, step 1, velocity
        const std::uint8_t fiveHundred[kBytesPerValue] = {0x00, 0x00, 0xFA, 0x43}; // 500.0f
        for (std::size_t byteIndex = 0; byteIndex < kBytesPerValue; ++byteIndex)
        {
            wild[velocityIndex + byteIndex] = fiveHundred[byteIndex];
        }
        expect(data.fromBytes(wild.data(), wild.size()));
        expectEquals(data.getValue(0, 0, dfd::StepParam::Velocity), dfd::kMaxVelocity);
    }
};

static PatternDataTests s_patternDataTests;
