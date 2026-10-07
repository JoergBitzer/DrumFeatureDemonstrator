/**
 * @file VoiceBank.h
 * @brief a pool of sample voices with sample-exact triggers inside a block
 *
 * Real-time safe: all memory is reserved in the constructor; render() does not allocate.
 */
#pragma once

#include <array>
#include <vector>

#include "SampleVoice.h"

namespace dfd
{
constexpr int kNumVoices = 16;
constexpr int kMaxEventsPerBlock = 256;

struct TriggerEvent
{
    int sampleOffset = 0;
    VoiceStart voiceStart;
};

class VoiceBank
{
public:
    VoiceBank();

    // events beyond kMaxEventsPerBlock per block are dropped
    void addEvent(int sampleOffset, const VoiceStart& voiceStart);

    // ADDS the sound of all voices to the outputs (clear them first) and removes the events
    void render(float* const* outputs, int numOutputChannels, int numSamples);

    void stopAll();
    int numActiveVoices() const;

private:
    void sortEventsByOffset();
    void renderVoices(float* const* outputs, int numOutputChannels, int firstSample, int numSamples);
    void startVoice(const VoiceStart& voiceStart);
    SampleVoice& chooseVoice();

    std::array<SampleVoice, kNumVoices> m_voices;
    std::vector<TriggerEvent> m_events;
    unsigned long long m_nextOrder = 0;
};
} // namespace dfd
