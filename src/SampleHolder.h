/**
 * @file SampleHolder.h
 * @brief hands a loaded sample from the message thread to the audio thread without blocking and without freeing memory there
 *
 * Message thread: setSample(). Audio thread: updateActiveSample() once per block, then getActiveSample().
 * The audio thread only swaps pointers (try-lock, no waiting); the old sample is freed later by the message thread.
 */
#pragma once

#include <memory>

#include <juce_core/juce_core.h>

#include "SampleData.h"

namespace dfd
{
class SampleHolder
{
public:
    // message thread
    void setSample(std::shared_ptr<const SampleData> newSample);
    std::shared_ptr<const SampleData> getCurrentSampleForGui() const;

    // audio thread: returns true if the active sample changed (stop the voices then)
    bool updateActiveSample();
    const SampleData* getActiveSample() const;

private:
    mutable juce::SpinLock m_lock;
    std::shared_ptr<const SampleData> m_pending; // after a swap: the old sample, freed by the next setSample()
    std::shared_ptr<const SampleData> m_active;
    bool m_newSampleWaiting = false;
};
} // namespace dfd
