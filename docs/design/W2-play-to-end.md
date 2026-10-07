# W2: algorithm of play to end (design note)

Status: implemented in `src/StepScheduler.h/.cpp`, tested in `tests/StepSchedulerTests.cpp` (CTest target `DrumFeatureDemonstrator_UnitTests`).
Decisions behind it: `planning.md`, sections 3 and 7.

## Scope
A **pure** scheduler: no JUCE types, no audio, no GUI, no allocation per call (the caller passes an output vector with reserved capacity).
Input: the pattern, the sample region, tempo and sample rate, the position of the block in quarter notes (ppq) and its length.
Output: the sound starts inside this block (step, offset in the block, playback ratio, direction, gain).

## Quantities (host sample domain)
| Name | Formula |
|---|---|
| `ratio` of a step | 2^(pitch / 12) * fileRate / hostRate; pitch = the step's semitones relative to the base note (the base note only matters for MIDI notes) |
| `duration` | regionLength (file samples) / `ratio` |
| `samplesPerBeat` | hostRate * 60 / bpm |
| `samplesPerStep` | `samplesPerBeat` / 4 (1/16 note) |
| `stepStart(k, pass)` | (pass * numSteps + k) / 4 in ppq |
| `anchor` | `stepStart + 1/4` (the end of the step) |
| `start` | forward and reverse step: `stepStart`; reverse + play to end: `anchor - duration / samplesPerBeat` |

A sound belongs to the block if its start, converted to a sample offset (rounded to a whole sample), lies in `[0, numSamples)`.
The rounding is done on the offset, so a sound is found exactly once when consecutive blocks follow each other without a gap.

## Rules
- **Length limit:** reverse + play to end is only valid if `duration <= numSteps * samplesPerStep` (`isPlayToEndAllowed`). An invalid step is not scheduled;
  the processor shows it as invalid (the GUI marks it and shows the notification).
- **Look-ahead:** because `duration` is at most one pattern, the start of a sound lies at most one pass before its step. The scheduler therefore checks the
  passes around the block (one before to one after) for every active step.
- **Start already gone:** a sound whose start lies before the block is never returned. `countMissedPlayToEnd(position)` counts the reverse + play to end sounds
  that would be running at `position` (start before it, end after it). The processor calls it only when the position is not continuous
  (transport start, jump, loop) and sets the notification flag.
- **Tempo changes:** not handled (decided); every call uses the tempo of the block.
- **Pattern length:** 8 to 32 steps (any value); steps beyond `numSteps` are ignored.
- Bad input (bpm, rates or region length not positive) gives no sound starts.

## Open points for the processor (W4/W5, not part of W2)
- The `SynchronBlockProcessor` may lag behind the host's block position by its rebuffering delay: the ppq passed to the scheduler is the position of the block that is filled.
- Detecting a discontinuity (block position differs from the expected one by more than a sample) is done in the processor.

## Tests (known numbers)
Sample counts at 120 bpm, 48 kHz (6000 samples per step); ratio and duration for several pitches and rates; forward and reverse steps start at the step;
play to end: `start + duration = anchor` within one sample for a grid of pitches, tempi, sample rates and region lengths; look-ahead into the pass before the step;
every sound exactly once across consecutive blocks of different sizes; odd pattern lengths; length limit at the boundary; inactive and out-of-range steps;
missed sounds at a transport start and at a jump; gain = velocity.
