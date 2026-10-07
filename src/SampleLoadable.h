/**
 * @file SampleLoadable.h
 * @brief the small interface of a processor that can load a sample file (used by the Tester without the plugin headers)
 */
#pragma once

#include <juce_core/juce_core.h>

namespace dfd
{
class SampleLoadable
{
public:
    virtual ~SampleLoadable() = default;
    virtual bool loadSample(const juce::File& file) = 0;
    virtual juce::String getSampleName() const = 0;
};
} // namespace dfd
