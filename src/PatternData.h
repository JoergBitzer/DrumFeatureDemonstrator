/**
 * @file PatternData.h
 * @brief the four patterns (5 values per step, 32 steps), the "play" switch of each pattern and four pattern settings, as plain data
 *
 * These values are not host parameters: a DAW cannot automate them, and they do not take up parameter slots (that would be
 * 640 parameters). They live in lock-free atomics, so the GUI thread can edit them while the audio thread reads them, and they
 * are saved as one binary blob (toBytes / fromBytes), which the processor stores in the plugin state and in the presets.
 *
 * Blob format (version 2), all numbers little endian:
 *   4 bytes  magic "DFDP"
 *   1 byte   format version (2)
 *   1 byte   number of patterns (4)
 *   1 byte   number of steps per pattern (32)
 *   1 byte   bit mask of the patterns that play (bit 0 = pattern 1)
 *   4 floats the settings in the order of PatternSetting (length, note value, sync to host, declick)
 *   then for every pattern, every step, the values in the order of StepParam (active, mode, note, velocity, pan)
 * Every number after the header is a 32-bit float (IEEE 754).
 * Pure C++ (no JUCE types).
 */
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "StepScheduler.h"
#include "StepValues.h"

namespace dfd
{
class PatternData
{
public:
    PatternData();

    // value of a step in the unit of its StepParam (see StepValues.h: the mode is 0, 1 or 2; active is 0 or 1)
    float getValue(int patternIndex, int stepIndex, StepParam param) const;
    // sets a value; it is limited to its range (active and mode are rounded)
    void setValue(int patternIndex, int stepIndex, StepParam param, float value);

    // the settings (see PatternSetting): integer values, limited to their range
    float getSetting(PatternSetting setting) const;
    void setSetting(PatternSetting setting, float value);

    // which patterns are played
    bool isPlaying(int patternIndex) const;
    void setPlaying(int patternIndex, bool shouldPlay);

    // everything back to the defaults: all steps off, pattern 1 plays, default settings
    void resetToDefaults();

    // audio thread: copies the steps and the play switches into the bank (the length and the base note are not part of it)
    void fillBank(PatternBank& bank) const;

    // the blob (see above) and its restoration; fromBytes changes nothing and returns false if the blob is not valid
    std::vector<std::uint8_t> toBytes() const;
    bool fromBytes(const std::uint8_t* bytes, std::size_t numBytes);

    // counts up with every fromBytes / resetToDefaults, so a GUI can see that the data was replaced (a preset was loaded)
    unsigned getLoadCount() const { return m_loadCount.load(); }

private:
    static std::size_t valueSlot(int patternIndex, int stepIndex, StepParam param);
    static float limitedValue(StepParam param, float value);
    static float limitedSetting(PatternSetting setting, float value);

    std::array<std::atomic<float>, kNumPatterns * kMaxSteps * kNumValuesPerStep> m_values;
    std::array<std::atomic<float>, kNumPatternSettings> m_settings;
    std::array<std::atomic<bool>, kNumPatterns> m_playing;
    std::atomic<unsigned> m_loadCount{0};
};
} // namespace dfd
