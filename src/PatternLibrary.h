/**
 * @file PatternLibrary.h
 * @brief a list of typical patterns in the categories BD, SD, HH, Perc and Chromatic (written for this plug-in, no external MIDI library)
 *
 * A pattern is a text with one token per step, separated by blanks (8 to 32 tokens, steps behind the last token stay off):
 *   .        the step is off
 *   x X o g  a hit: normal (velocity 100), accent (127), soft (60), ghost note (40)
 *   +n -n    after a hit: the note is n semitones above or below the base note (C4 = the default); for the chromatic patterns
 *   *n       after a hit: repeat the hit n times within the step (2 - 8, a ratchet)
 *   ^ v      after *n: the repeats speed up (^) or slow down (v)
 * Example: "X . . . x*2^ . o+7 ." Pure C++ (no JUCE types).
 */
#pragma once

#include "PatternData.h"

namespace dfd
{
struct LibraryEntry
{
    const char* category;
    const char* name;
    const char* steps;
};

int numLibraryCategories();
const char* libraryCategoryName(int categoryIndex);
int numLibraryEntries();
const LibraryEntry& libraryEntry(int entryIndex);

// the 32 steps of a library pattern (false if the text is not valid; then the steps are all off)
bool makePatternSteps(const LibraryEntry& entry, PatternSteps& steps);
} // namespace dfd
