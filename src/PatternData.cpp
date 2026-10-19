#include "PatternData.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dfd
{
namespace
{
constexpr std::size_t kMagicSize = 4;
constexpr std::uint8_t kMagic[kMagicSize] = {'D', 'F', 'D', 'P'};
constexpr std::uint8_t kFormatVersion = 1;
constexpr std::size_t kHeaderSize = kMagicSize + 4; // magic, version, number of patterns, number of steps, play mask
constexpr std::size_t kBytesPerValue = 4;
constexpr std::size_t kNumValues = static_cast<std::size_t>(kNumPatterns) * kMaxSteps * kNumValuesPerStep;
constexpr std::size_t kBlobSize = kHeaderSize + kNumValues * kBytesPerValue;
constexpr int kBitsPerByte = 8;
constexpr std::uint32_t kByteMask = 0xFF;
constexpr float kHalf = 0.5f;
constexpr float kModeReverseThreshold = kModeReverse + kHalf; // above: reverse to end
constexpr float kModeForwardThreshold = kModeForward + kHalf; // above: reverse

void writeFloat(std::vector<std::uint8_t>& bytes, float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (std::size_t byteIndex = 0; byteIndex < kBytesPerValue; ++byteIndex)
    {
        bytes.push_back(static_cast<std::uint8_t>((bits >> (kBitsPerByte * byteIndex)) & kByteMask));
    }
}

float readFloat(const std::uint8_t* bytes)
{
    std::uint32_t bits = 0;
    for (std::size_t byteIndex = 0; byteIndex < kBytesPerValue; ++byteIndex)
    {
        bits |= static_cast<std::uint32_t>(bytes[byteIndex]) << (kBitsPerByte * byteIndex);
    }
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

StepMode modeFromValue(float value)
{
    if (value > kModeReverseThreshold)
    {
        return StepMode::ReverseToEnd;
    }
    if (value > kModeForwardThreshold)
    {
        return StepMode::Reverse;
    }
    return StepMode::Forward;
}
} // namespace

PatternData::PatternData()
{
    resetToDefaults();
    m_loadCount = 0;
}

std::size_t PatternData::valueSlot(int patternIndex, int stepIndex, StepParam param)
{
    const int clampedPattern = std::clamp(patternIndex, 0, kNumPatterns - 1);
    const int clampedStep = std::clamp(stepIndex, 0, kMaxSteps - 1);
    return static_cast<std::size_t>((clampedPattern * kMaxSteps + clampedStep) * kNumValuesPerStep + static_cast<int>(param));
}

float PatternData::limitedValue(StepParam param, float value)
{
    const StepValueSpec spec = stepValueSpec(param);
    if (std::isnan(value))
    {
        return spec.defaultValue;
    }
    float limited = std::clamp(value, spec.minValue, spec.maxValue);
    if (param == StepParam::Active || param == StepParam::Mode)
    {
        limited = std::round(limited);
    }
    return limited;
}

float PatternData::getValue(int patternIndex, int stepIndex, StepParam param) const
{
    return m_values[valueSlot(patternIndex, stepIndex, param)].load();
}

void PatternData::setValue(int patternIndex, int stepIndex, StepParam param, float value)
{
    m_values[valueSlot(patternIndex, stepIndex, param)].store(limitedValue(param, value));
}

bool PatternData::isPlaying(int patternIndex) const
{
    return m_playing[static_cast<std::size_t>(std::clamp(patternIndex, 0, kNumPatterns - 1))].load();
}

void PatternData::setPlaying(int patternIndex, bool shouldPlay)
{
    m_playing[static_cast<std::size_t>(std::clamp(patternIndex, 0, kNumPatterns - 1))].store(shouldPlay);
}

void PatternData::resetToDefaults()
{
    for (int patternIndex = 0; patternIndex < kNumPatterns; ++patternIndex)
    {
        for (int stepIndex = 0; stepIndex < kMaxSteps; ++stepIndex)
        {
            for (int paramIndex = 0; paramIndex < kNumValuesPerStep; ++paramIndex)
            {
                const StepParam param = static_cast<StepParam>(paramIndex);
                setValue(patternIndex, stepIndex, param, stepValueSpec(param).defaultValue);
            }
        }
        setPlaying(patternIndex, patternIndex == 0);
    }
    ++m_loadCount;
}

void PatternData::fillBank(PatternBank& bank) const
{
    for (int patternIndex = 0; patternIndex < kNumPatterns; ++patternIndex)
    {
        bank.playing[static_cast<std::size_t>(patternIndex)] = isPlaying(patternIndex);
        for (int stepIndex = 0; stepIndex < kMaxSteps; ++stepIndex)
        {
            StepSettings& step = stepAt(bank, patternIndex, stepIndex);
            step.active = getValue(patternIndex, stepIndex, StepParam::Active) > kHalf;
            step.mode = modeFromValue(getValue(patternIndex, stepIndex, StepParam::Mode));
            step.noteNumber = static_cast<double>(getValue(patternIndex, stepIndex, StepParam::Note));
            step.velocity = getValue(patternIndex, stepIndex, StepParam::Velocity);
            step.pan = getValue(patternIndex, stepIndex, StepParam::Pan);
        }
    }
}

std::vector<std::uint8_t> PatternData::toBytes() const
{
    std::vector<std::uint8_t> bytes;
    bytes.reserve(kBlobSize);
    bytes.insert(bytes.end(), kMagic, kMagic + kMagicSize);
    bytes.push_back(kFormatVersion);
    bytes.push_back(static_cast<std::uint8_t>(kNumPatterns));
    bytes.push_back(static_cast<std::uint8_t>(kMaxSteps));
    std::uint8_t playMask = 0;
    for (int patternIndex = 0; patternIndex < kNumPatterns; ++patternIndex)
    {
        if (isPlaying(patternIndex))
        {
            playMask = static_cast<std::uint8_t>(playMask | (1u << patternIndex));
        }
    }
    bytes.push_back(playMask);
    for (const std::atomic<float>& value : m_values)
    {
        writeFloat(bytes, value.load());
    }
    return bytes;
}

bool PatternData::fromBytes(const std::uint8_t* bytes, std::size_t numBytes)
{
    if (bytes == nullptr || numBytes != kBlobSize)
    {
        return false;
    }
    if (std::memcmp(bytes, kMagic, kMagicSize) != 0)
    {
        return false;
    }
    const std::uint8_t* header = bytes + kMagicSize;
    if (header[0] != kFormatVersion || header[1] != kNumPatterns || header[2] != kMaxSteps)
    {
        return false;
    }
    const std::uint8_t playMask = header[3];
    for (int patternIndex = 0; patternIndex < kNumPatterns; ++patternIndex)
    {
        setPlaying(patternIndex, (playMask & (1u << patternIndex)) != 0);
    }
    const std::uint8_t* valueBytes = bytes + kHeaderSize;
    for (int patternIndex = 0; patternIndex < kNumPatterns; ++patternIndex)
    {
        for (int stepIndex = 0; stepIndex < kMaxSteps; ++stepIndex)
        {
            for (int paramIndex = 0; paramIndex < kNumValuesPerStep; ++paramIndex)
            {
                setValue(patternIndex, stepIndex, static_cast<StepParam>(paramIndex), readFloat(valueBytes));
                valueBytes += kBytesPerValue;
            }
        }
    }
    ++m_loadCount;
    return true;
}
} // namespace dfd
