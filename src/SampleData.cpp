#include "SampleData.h"

#include <cmath>

namespace dfd
{
namespace
{
// a crossing between sample index-1 and index (or an exact zero at index)
bool isCrossingAt(const SampleData& sample, int index)
{
    if (index < 1)
    {
        return false;
    }
    if (index >= sample.numSamples())
    {
        return false;
    }
    const float before = sample.monoValueAt(index - 1);
    const float current = sample.monoValueAt(index);
    return before * current <= 0.0f;
}

int closerToZero(const SampleData& sample, int crossingIndex)
{
    const float before = std::abs(sample.monoValueAt(crossingIndex - 1));
    const float current = std::abs(sample.monoValueAt(crossingIndex));
    if (before < current)
    {
        return crossingIndex - 1;
    }
    return crossingIndex;
}
} // namespace

int SampleData::numChannels() const
{
    return static_cast<int>(channels.size());
}

int SampleData::numSamples() const
{
    if (channels.empty())
    {
        return 0;
    }
    return static_cast<int>(channels[0].size());
}

float SampleData::valueAt(int channelIndex, int sampleIndex) const
{
    const std::vector<float>& channel = channels[static_cast<std::size_t>(channelIndex)];
    return channel[static_cast<std::size_t>(sampleIndex)];
}

float SampleData::monoValueAt(int sampleIndex) const
{
    float sum = 0.0f;
    for (int channelIndex = 0; channelIndex < numChannels(); ++channelIndex)
    {
        sum += valueAt(channelIndex, sampleIndex);
    }
    return sum / static_cast<float>(numChannels());
}

int findNearestZeroCrossing(const SampleData& sample, int position, int searchRadius)
{
    if (sample.numSamples() == 0)
    {
        return position;
    }
    for (int distance = 0; distance <= searchRadius; ++distance)
    {
        const int below = position - distance;
        if (isCrossingAt(sample, below))
        {
            return closerToZero(sample, below);
        }
        const int above = position + distance;
        if (isCrossingAt(sample, above))
        {
            return closerToZero(sample, above);
        }
    }
    return position;
}
} // namespace dfd
