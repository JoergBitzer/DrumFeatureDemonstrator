#include "VoiceBank.h"

#include <algorithm>

namespace dfd
{
VoiceBank::VoiceBank()
{
    m_events.reserve(kMaxEventsPerBlock);
}

void VoiceBank::addEvent(int sampleOffset, const VoiceStart& voiceStart)
{
    if (m_events.size() >= kMaxEventsPerBlock)
    {
        return;
    }
    TriggerEvent event;
    event.sampleOffset = sampleOffset;
    event.voiceStart = voiceStart;
    m_events.push_back(event);
}

void VoiceBank::sortEventsByOffset()
{
    // insertion sort: stable and without allocation
    for (std::size_t index = 1; index < m_events.size(); ++index)
    {
        std::size_t position = index;
        while (position > 0 && m_events[position - 1].sampleOffset > m_events[position].sampleOffset)
        {
            std::swap(m_events[position - 1], m_events[position]);
            --position;
        }
    }
}

SampleVoice& VoiceBank::chooseVoice()
{
    for (SampleVoice& voice : m_voices)
    {
        if (voice.isActive() == false)
        {
            return voice;
        }
    }
    SampleVoice* oldestVoice = &m_voices[0];
    for (SampleVoice& voice : m_voices)
    {
        if (voice.getOrder() < oldestVoice->getOrder())
        {
            oldestVoice = &voice;
        }
    }
    return *oldestVoice;
}

void VoiceBank::startVoice(const VoiceStart& voiceStart)
{
    SampleVoice& voice = chooseVoice();
    voice.start(voiceStart, m_nextOrder);
    ++m_nextOrder;
}

void VoiceBank::renderVoices(float* const* outputs, int numOutputChannels, int firstSample, int numSamples)
{
    if (numSamples <= 0)
    {
        return;
    }
    for (SampleVoice& voice : m_voices)
    {
        if (voice.isActive())
        {
            voice.renderAdd(outputs, numOutputChannels, firstSample, numSamples);
        }
    }
}

void VoiceBank::render(float* const* outputs, int numOutputChannels, int numSamples)
{
    sortEventsByOffset();
    int renderedUntil = 0;
    for (const TriggerEvent& event : m_events)
    {
        const int offset = std::clamp(event.sampleOffset, 0, numSamples);
        renderVoices(outputs, numOutputChannels, renderedUntil, offset - renderedUntil);
        renderedUntil = offset;
        startVoice(event.voiceStart);
    }
    renderVoices(outputs, numOutputChannels, renderedUntil, numSamples - renderedUntil);
    m_events.clear();
}

void VoiceBank::stopAll()
{
    for (SampleVoice& voice : m_voices)
    {
        voice.stop();
    }
    m_events.clear();
}

int VoiceBank::numActiveVoices() const
{
    int numActive = 0;
    for (const SampleVoice& voice : m_voices)
    {
        if (voice.isActive())
        {
            ++numActive;
        }
    }
    return numActive;
}
} // namespace dfd
