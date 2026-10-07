#include "Sequencer.h"

#include <cmath>

namespace dfd
{
namespace
{
constexpr double kContinuityToleranceInSamples = 1.0;
constexpr int kNotPlayingStep = -1;

int stepAtPosition(const Pattern& pattern, double positionPpq)
{
    const int numSteps = numUsedSteps(pattern);
    const long long stepCount = static_cast<long long>(std::floor(positionPpq * kStepsPerBeat));
    long long stepInPattern = stepCount % numSteps;
    if (stepInPattern < 0)
    {
        stepInPattern += numSteps;
    }
    return static_cast<int>(stepInPattern);
}
} // namespace

void Sequencer::reset()
{
    m_hasExpectedPosition = false;
    m_expectedPositionPpq = 0.0;
}

bool Sequencer::isContinuous(double positionPpq, double samplesPerBeat) const
{
    if (m_hasExpectedPosition == false)
    {
        return false;
    }
    const double differenceInSamples = std::abs(positionPpq - m_expectedPositionPpq) * samplesPerBeat;
    return differenceInSamples <= kContinuityToleranceInSamples;
}

void Sequencer::process(const Pattern& pattern, const SampleRegion& region, double hostSampleRate,
                        const BlockTransport& transport, int numSamples, std::vector<SoundStart>& soundStarts,
                        SequencerResult& result)
{
    soundStarts.clear();
    result.missedPlayToEnd = 0;
    result.currentStep = kNotPlayingStep;
    if (transport.isPlaying == false || transport.hasPosition == false)
    {
        reset();
        return;
    }
    if (transport.bpm <= 0.0 || hostSampleRate <= 0.0 || numSamples <= 0)
    {
        reset();
        return;
    }

    Transport timing;
    timing.bpm = transport.bpm;
    timing.hostSampleRate = hostSampleRate;
    const double beatLength = samplesPerBeat(timing);

    if (isContinuous(transport.positionPpq, beatLength) == false)
    {
        result.missedPlayToEnd = countMissedPlayToEnd(pattern, region, timing, transport.positionPpq);
    }
    scheduleBlock(pattern, region, timing, transport.positionPpq, numSamples, soundStarts);
    result.currentStep = stepAtPosition(pattern, transport.positionPpq);

    m_hasExpectedPosition = true;
    m_expectedPositionPpq = transport.positionPpq + numSamples / beatLength;
}

void InternalClock::setPlaying(bool shouldPlay)
{
    m_playing = shouldPlay;
}

bool InternalClock::isPlaying() const
{
    return m_playing;
}

void InternalClock::setBpm(double bpm)
{
    m_bpm = bpm;
}

double InternalClock::getBpm() const
{
    return m_bpm;
}

void InternalClock::rewind()
{
    m_positionPpq = 0.0;
}

BlockTransport InternalClock::getTransport() const
{
    BlockTransport transport;
    transport.isPlaying = m_playing;
    transport.hasPosition = true;
    transport.positionPpq = m_positionPpq;
    transport.bpm = m_bpm;
    return transport;
}

void InternalClock::advance(int numSamples, double hostSampleRate)
{
    if (m_playing == false)
    {
        return;
    }
    Transport timing;
    timing.bpm = m_bpm;
    timing.hostSampleRate = hostSampleRate;
    m_positionPpq += numSamples / samplesPerBeat(timing);
}
} // namespace dfd
