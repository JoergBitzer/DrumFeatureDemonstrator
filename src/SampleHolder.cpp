#include "SampleHolder.h"

namespace dfd
{
void SampleHolder::setSample(std::shared_ptr<const SampleData> newSample)
{
    std::shared_ptr<const SampleData> oldSample;
    {
        const juce::SpinLock::ScopedLockType lock(m_lock);
        oldSample = std::move(m_pending);
        m_pending = std::move(newSample);
        m_newSampleWaiting = true;
    }
    // oldSample is freed here, on the message thread, outside the lock
}

std::shared_ptr<const SampleData> SampleHolder::getCurrentSampleForGui() const
{
    const juce::SpinLock::ScopedLockType lock(m_lock);
    if (m_newSampleWaiting)
    {
        return m_pending;
    }
    return m_active;
}

bool SampleHolder::updateActiveSample()
{
    const juce::SpinLock::ScopedTryLockType lock(m_lock);
    if (lock.isLocked() == false)
    {
        return false;
    }
    if (m_newSampleWaiting == false)
    {
        return false;
    }
    m_active.swap(m_pending); // the old active sample now waits in m_pending
    m_newSampleWaiting = false;
    return true;
}

const SampleData* SampleHolder::getActiveSample() const
{
    return m_active.get();
}
} // namespace dfd
