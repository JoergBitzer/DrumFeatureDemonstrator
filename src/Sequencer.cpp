#include "Sequencer.h"

#include <cmath>

namespace dfd
{
namespace
{
constexpr double kContinuityToleranceInSamples = 1.0;
constexpr int kNotPlaying = -1;

// where in the chain of the playing patterns the position lies
void locateInChain(const PatternBank& bank, double positionPpq, int& patternIndex, int& stepIndex)
{
    const long long chainSteps = static_cast<long long>(numPlayingPatterns(bank)) * numUsedSteps(bank);
    const long long stepCount = static_cast<long long>(std::floor(positionPpq / bank.stepLengthInBeats));
    long long stepInChain = stepCount % chainSteps;
    if (stepInChain < 0)
    {
        stepInChain += chainSteps;
    }
    const int usedSteps = numUsedSteps(bank);
    patternIndex = playingPatternAt(bank, static_cast<int>(stepInChain / usedSteps));
    stepIndex = static_cast<int>(stepInChain % usedSteps);
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
    process(makeBank(pattern), region, hostSampleRate, transport, numSamples, soundStarts, result);
}

void Sequencer::process(const PatternBank& bank, const SampleRegion& region, double hostSampleRate,
                        const BlockTransport& transport, int numSamples, std::vector<SoundStart>& soundStarts,
                        SequencerResult& result)
{
    soundStarts.clear();
    result.missedPlayToEnd = 0;
    result.transportRunning = false;
    result.currentPattern = kNotPlaying;
    result.currentStep = kNotPlaying;
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
    result.transportRunning = true;

    Transport timing;
    timing.bpm = transport.bpm;
    timing.hostSampleRate = hostSampleRate;
    const double beatLength = samplesPerBeat(timing);

    if (isContinuous(transport.positionPpq, beatLength) == false)
    {
        result.missedPlayToEnd = countMissedPlayToEnd(bank, region, timing, transport.positionPpq);
    }
    scheduleBlock(bank, region, timing, transport.positionPpq, numSamples, soundStarts);
    if (numPlayingPatterns(bank) > 0)
    {
        locateInChain(bank, transport.positionPpq, result.currentPattern, result.currentStep);
    }

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
