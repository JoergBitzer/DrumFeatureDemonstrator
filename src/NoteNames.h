/**
 * @file NoteNames.h
 * @brief names of MIDI notes ("C4" = 60)
 */
#pragma once

#include <string>

namespace dfd
{
constexpr int kNotesPerOctave = 12;
constexpr int kOctaveOfNoteZero = -1; // MIDI note 0 is C-1, note 60 is C4

inline std::string noteName(int midiNote)
{
    static const char* const kNames[kNotesPerOctave] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    if (midiNote < 0)
    {
        midiNote = 0;
    }
    const int octave = midiNote / kNotesPerOctave + kOctaveOfNoteZero;
    const int index = midiNote % kNotesPerOctave;
    return std::string(kNames[index]) + std::to_string(octave);
}
} // namespace dfd
