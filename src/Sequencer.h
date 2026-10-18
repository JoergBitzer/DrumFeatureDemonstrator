/**
 * @file Sequencer.h
 * @brief transport handling around the StepScheduler: continuity check, notification count, internal clock
 *
 * Pure C++ (no JUCE types). The host's or the internal beat position goes in, sound starts come out.
 */
#pragma once

#include <vector>

#include "StepScheduler.h"

namespace dfd
{
struct BlockTransport
{
    bool isPlaying = false;
    bool hasPosition = false; // the host delivered a beat position and a tempo
    double positionPpq = 0.0; // beat position (quarter notes) at the start of the block
    double bpm = 0.0;
};

struct SequencerResult
{
    int missedPlayToEnd = 0; // sounds that should be running, but whose start is already gone (after a start or jump)
    bool transportRunning = false; // the transport (host or internal beat) plays
    int currentPattern = -1; // pattern of the chain at the start of the block, -1 if nothing plays
    int currentStep = -1;    // step in that pattern at the start of the block, -1 if nothing plays
};

class Sequencer
{
public:
    void reset();

    // soundStarts is cleared first (reserve its capacity beforehand)
    void process(const PatternBank& bank, const SampleRegion& region, double hostSampleRate,
                 const BlockTransport& transport, int numSamples, std::vector<SoundStart>& soundStarts,
                 SequencerResult& result);
    // one pattern only (as pattern 0 of a bank)
    void process(const Pattern& pattern, const SampleRegion& region, double hostSampleRate,
                 const BlockTransport& transport, int numSamples, std::vector<SoundStart>& soundStarts,
                 SequencerResult& result);

private:
    bool isContinuous(double positionPpq, double samplesPerBeat) const;

    bool m_hasExpectedPosition = false;
    double m_expectedPositionPpq = 0.0;
};

// own beat clock for the internal mode
class InternalClock
{
public:
    void setPlaying(bool shouldPlay);
    bool isPlaying() const;
    void setBpm(double bpm);
    double getBpm() const;
    void rewind();

    BlockTransport getTransport() const;
    void advance(int numSamples, double hostSampleRate);

private:
    bool m_playing = false;
    double m_bpm = 120.0;
    double m_positionPpq = 0.0;
};
} // namespace dfd
