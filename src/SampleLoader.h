/**
 * @file SampleLoader.h
 * @brief reads an audio file (wav, aiff, flac, ogg, ...) into a SampleData (message thread only)
 */
#pragma once

#include <memory>

#include <juce_core/juce_core.h>

#include "SampleData.h"

namespace dfd
{
// returns nullptr if the file cannot be read or is empty
std::shared_ptr<const SampleData> loadSampleFile(const juce::File& file);
} // namespace dfd
