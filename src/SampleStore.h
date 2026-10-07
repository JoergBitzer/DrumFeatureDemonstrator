/**
 * @file SampleStore.h
 * @brief keeps a copy of the loaded sample in the user's data folder and embeds it into the plugin state of a DAW project
 */
#pragma once

#include <juce_core/juce_core.h>

namespace dfd
{
constexpr juce::int64 kMaxEmbeddedSampleBytes = 8 * 1024 * 1024;

// the folder "Samples" inside the folder of the user presets (created if necessary)
juce::File getSamplesFolder(const juce::File& userPresetsFolder);

// Copies the file into the samples folder and returns the copy. A file of the same name with the same content is reused,
// a file of the same name with other content gets a number ("name_2.wav"). If the file is already in the folder it is returned as it is.
juce::File storeSampleCopy(const juce::File& sourceFile, const juce::File& samplesFolder);

// the file content as base64 text (empty if the file is missing or larger than kMaxEmbeddedSampleBytes)
juce::String encodeFileAsBase64(const juce::File& file);

// writes the decoded content to the file, if the file does not exist yet; returns true if the file exists afterwards
bool restoreFileFromBase64(const juce::String& base64Text, const juce::File& file);
} // namespace dfd
