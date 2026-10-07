/**
 * @file SampleData.h
 * @brief the loaded sample (immutable after loading) and the zero-crossing search for the declick button
 */
#pragma once

#include <vector>

namespace dfd
{
struct SampleData
{
    std::vector<std::vector<float>> channels;
    double sampleRate = 0.0;

    int numChannels() const;
    int numSamples() const;
    float valueAt(int channelIndex, int sampleIndex) const;
    float monoValueAt(int sampleIndex) const; // mean of all channels
};

// Nearest zero crossing of the mono mix to the position, searched up to searchRadius samples in both directions.
// Returns the sample before or after the crossing, whichever is closer to zero. Without a crossing in range
// (or for an empty sample) the position is returned unchanged.
int findNearestZeroCrossing(const SampleData& sample, int position, int searchRadius);
} // namespace dfd
