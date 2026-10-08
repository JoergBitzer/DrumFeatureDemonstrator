#include "SampleVoice.h"

#include <algorithm>
#include <cmath>

namespace dfd
{
void SampleVoice::start(const VoiceStart& voiceStart, unsigned long long order)
{
    m_start = voiceStart;
    m_order = order;
    m_active = false;
    if (voiceStart.sample == nullptr)
    {
        return;
    }
    if (voiceStart.regionEnd <= voiceStart.regionStart)
    {
        return;
    }
    if (voiceStart.playbackRatio <= 0.0)
    {
        return;
    }
    if (voiceStart.reverse)
    {
        m_position = voiceStart.regionEnd - 1;
    }
    else
    {
        m_position = voiceStart.regionStart;
    }
    m_active = true;
}

void SampleVoice::stop()
{
    m_active = false;
}

bool SampleVoice::isActive() const
{
    return m_active;
}

unsigned long long SampleVoice::getOrder() const
{
    return m_order;
}

bool SampleVoice::isPositionInRegion() const
{
    if (m_position < m_start.regionStart)
    {
        return false;
    }
    if (m_position > m_start.regionEnd - 1)
    {
        return false;
    }
    return true;
}

float SampleVoice::interpolatedValue(int channelIndex) const
{
    const int lastIndex = m_start.regionEnd - 1;
    const int lowerIndex = static_cast<int>(std::floor(m_position));
    const float fraction = static_cast<float>(m_position - lowerIndex);
    const int upperIndex = std::min(lowerIndex + 1, lastIndex);
    const float lowerValue = m_start.sample->valueAt(channelIndex, lowerIndex);
    const float upperValue = m_start.sample->valueAt(channelIndex, upperIndex);
    return lowerValue + fraction * (upperValue - lowerValue);
}

float SampleVoice::fadeGain() const
{
    double gain = 1.0;
    if (m_start.fadeInSamples > 0.0)
    {
        gain = std::min(gain, (m_position - m_start.regionStart) / m_start.fadeInSamples);
    }
    if (m_start.fadeOutSamples > 0.0)
    {
        gain = std::min(gain, (m_start.regionEnd - m_position) / m_start.fadeOutSamples);
    }
    return static_cast<float>(std::max(0.0, gain));
}

void SampleVoice::renderAdd(float* const* outputs, int numOutputChannels, int firstSample, int numSamples)
{
    const int numSampleChannels = m_start.sample->numChannels();
    const double step = m_start.playbackRatio;
    for (int sampleIndex = firstSample; sampleIndex < firstSample + numSamples; ++sampleIndex)
    {
        if (isPositionInRegion() == false)
        {
            m_active = false;
            return;
        }
        const float gain = m_start.gain * fadeGain();
        for (int outputIndex = 0; outputIndex < numOutputChannels; ++outputIndex)
        {
            const int sourceChannel = std::min(outputIndex, numSampleChannels - 1);
            outputs[outputIndex][sampleIndex] += gain * interpolatedValue(sourceChannel);
        }
        if (m_start.reverse)
        {
            m_position -= step;
        }
        else
        {
            m_position += step;
        }
    }
}
} // namespace dfd
