/**
 * @file SampleVoice.h
 * @brief one voice that plays the region of a sample with resampling (forward or reverse)
 */
#pragma once

#include "SampleData.h"

namespace dfd
{
// balance law: the centre keeps the full level of both channels, panning to one side only turns the other channel down
struct PanGains
{
    float left = 1.0f;
    float right = 1.0f;
};
PanGains panGains(float pan);

// the curve of a fade: gain = x ^ exponent, x runs linearly from 0 (silence) to 1 (full level). The shape -1 .. 1 gives the exponent
// 2 ^ (-shape * kShapeExponentRange): 0 = linear, negative = exponential (slow start), positive = anti-exponential (fast start).
constexpr double kShapeExponentRange = 2.0;
double fadeExponent(double shape);

struct VoiceStart
{
    const SampleData* sample = nullptr;
    int regionStart = 0; // first sample of the region
    int regionEnd = 0;   // one behind the last sample of the region
    double fadeInSamples = 0.0;  // in file samples: the gain rises from 0 to 1 over this length after regionStart
    double fadeOutSamples = 0.0; // the gain falls from 1 to 0 over this length before regionEnd (both in the sample's own time)
    double fadeInExponent = 1.0;  // 1 = linear fade, see fadeExponent()
    double fadeOutExponent = 1.0;
    double playbackRatio = 1.0;
    bool reverse = false;
    float gain = 1.0f;
    float pan = 0.0f; // -1 left, 0 centre, 1 right (only for outputs with two channels)
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
    float fadeGain() const;
    bool isPositionInRegion() const;

    VoiceStart m_start;
    double m_position = 0.0;
    bool m_active = false;
    unsigned long long m_order = 0;
};
} // namespace dfd
