/**
 * @file SampleVoice.h
 * @brief one voice that plays the region of a sample with resampling (forward or reverse)
 */
#pragma once

#include "SampleData.h"

namespace dfd
{
struct VoiceStart
{
    const SampleData* sample = nullptr;
    int regionStart = 0; // first sample of the region
    int regionEnd = 0;   // one behind the last sample of the region
    double playbackRatio = 1.0;
    bool reverse = false;
    float gain = 1.0f;
};

class SampleVoice
{
public:
    void start(const VoiceStart& voiceStart, unsigned long long order);
    void stop();
    bool isActive() const;
    unsigned long long getOrder() const;

    // adds the voice to the outputs from firstSample on (linear interpolation)
    void renderAdd(float* const* outputs, int numOutputChannels, int firstSample, int numSamples);

private:
    float interpolatedValue(int channelIndex) const;
    bool isPositionInRegion() const;

    VoiceStart m_start;
    double m_position = 0.0;
    bool m_active = false;
    unsigned long long m_order = 0;
};
} // namespace dfd
