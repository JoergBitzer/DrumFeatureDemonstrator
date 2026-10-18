/**
 * @file NoteValues.h
 * @brief the note values a step of the pattern can have: 1/1 to 1/64, each normal, dotted (x 1.5) and as a triplet (x 2/3)
 */
#pragma once

#include <string>

namespace dfd
{
constexpr int kNumBaseNoteValues = 7;      // 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64
constexpr int kNumNoteValueFlavours = 3;   // normal, dotted, triplet
constexpr int kNumNoteValues = kNumBaseNoteValues * kNumNoteValueFlavours;
constexpr int kDefaultNoteValueIndex = 12; // 1/16
constexpr double kBeatsInWholeNote = 4.0;  // a beat is a quarter note
constexpr double kDottedFactor = 1.5;
constexpr double kTripletFactor = 2.0 / 3.0;

// index = base * 3 + flavour: 0 "1/1", 1 "1/1 dotted", 2 "1/1 triplet", 3 "1/2", ... 20 "1/64 triplet"
inline int clampedNoteValueIndex(int index)
{
    if (index < 0)
    {
        return 0;
    }
    if (index >= kNumNoteValues)
    {
        return kNumNoteValues - 1;
    }
    return index;
}

// the length of one step in beats (quarter notes)
inline double noteValueLengthInBeats(int index)
{
    const int clamped = clampedNoteValueIndex(index);
    const int baseIndex = clamped / kNumNoteValueFlavours;
    const int flavour = clamped % kNumNoteValueFlavours;
    double length = kBeatsInWholeNote;
    for (int halving = 0; halving < baseIndex; ++halving)
    {
        length *= 0.5;
    }
    if (flavour == 1)
    {
        length *= kDottedFactor;
    }
    if (flavour == 2)
    {
        length *= kTripletFactor;
    }
    return length;
}

inline std::string noteValueName(int index)
{
    const int clamped = clampedNoteValueIndex(index);
    const int baseIndex = clamped / kNumNoteValueFlavours;
    const int flavour = clamped % kNumNoteValueFlavours;
    int denominator = 1;
    for (int doubling = 0; doubling < baseIndex; ++doubling)
    {
        denominator *= 2;
    }
    std::string name = "1/" + std::to_string(denominator);
    if (flavour == 1)
    {
        name += " dotted";
    }
    if (flavour == 2)
    {
        name += " triplet";
    }
    return name;
}
} // namespace dfd
