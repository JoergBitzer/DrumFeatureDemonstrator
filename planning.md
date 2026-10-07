# DrumFeatureDemonstrator - planning

Status: draft v1 (2026-10-07), from the author's first ideas. Nothing is decided until marked **DECIDED**. No code has been written yet.
Basis: the AdvancedAudioTemplate, branch `AAT2` (`~/AudioDev/AdvancedAudioTemplate`). This time the plugin is an **instrument**.
The working name `DrumFeatureDemonstrator` is the folder name; the plugin name is open (section 8).

## 1. Idea and purpose
A simple one-sample drum player with a 1/16 step grid. It is a *demonstrator* for one new feature: **"play to end"**.

**The new thing.** The plugin knows exactly how long the sample sounds at the chosen pitch:
`duration = number of samples of the region * resampling factor` (resampling factor = 1 / playback ratio, including the
sample-rate ratio file/host). From the beat position the DAW reports (`AudioPlayHead`) it can therefore compute the **start
time so that the sample ends exactly on a beat boundary**. Main use: reversed sounds (a reverse cymbal) that run into the next
full beat as a typical intro, without nudging the sample by hand, at any pitch, at any tempo.

## 2. Features (author's list, ordered)
1. **One sample**, loaded by the user; the GUI shows the waveform with two handles (start, end of the region that is played).
2. **Step grid of 1/16 notes** with one button per step. Below each step button:
   - a second button **reverse** (play the region backwards),
   - a third button **play to end**, drawn as two half buttons left and right of the main button (the meaning of left/right is
     open, question 1).
3. **Per step a small knob, +-24 semitones**, the pitch of that step.
4. **Play engine:** plain resampling (pitch and duration change together, no time stretching). **Base note** is adjustable, default C4 (question 6).
5. **Tempo:** a button switches between *synchron mode* (follows the DAW; the BPM is displayed) and *internal mode* (own BPM).
   The internal beat can be played by the plugin itself, so a **play/pause button** is needed. It is only active in internal mode.
6. **Always synchronized** in synchron mode: the plugin reads beat position, tempo and play state from the playhead.

## 3. The core algorithm: play to end
Quantities, all in the host's sample domain:
- `regionLength` = (end handle - start handle) in file samples; `ratio` = 2^((note - baseNote) / 12) * fileRate / hostRate (playback speed).
- `duration` = `regionLength / ratio` host samples (exact; the time of the sample at the chosen pitch).
- `samplesPerStep` = hostRate * 60 / bpm / 4 (1/16 = a quarter of a beat).
- `anchor` = the beat boundary at which the sound has to end, in ppq (quarter notes) from the playhead.
- `startTime` = `anchor` converted to host samples - `duration`.

Consequences to design for (details in a design note in W2):
- **Look-ahead.** The start lies *before* the step, by up to the whole duration of the sample. The scheduler therefore does not ask
  "which step starts in this block" but "which sound must **start** in this block". Because the pattern repeats, a step of the
  *next* pass can start in the current pass. With a duration longer than the pattern, several passes are involved (limit it and say so in the GUI).
- **Jumping in.** If transport starts, loops or jumps when a sound should already be running, it starts **with an offset into the
  sample** (the position the sound would have reached), so the end still lands on the beat.
- **Tempo changes** during the run-up: `duration` is a time, the anchor is a beat. The start is computed from the tempo at the time
  of scheduling; recompute every block while the sound is not started, and state honestly that a change *during* the sound shifts the end
  (alternative: change the playback ratio live to keep the end; decide later, question 9).
- **Block-exact starts:** the sample offset inside the block is rounded to a sample (optionally fractional start phase of the resampler, question 8).
- **The template's `SynchronBlockProcessor`** works with fixed block sizes; for sample-exact starts the engine runs in the processor block
  or with a very small synchron block (`desiredSize < 1` = direct processing). To be decided in W1/W2.
- **Playhead access:** `getPlayHead()->getPosition()` gives `ppqPosition`, `bpm`, `isPlaying`, `timeSignature`, loop points, each optional.
  A host that delivers none of them -> fall back to internal mode (shown in the GUI).
- **Internal mode:** own clock from BPM and sample count; play/pause; same scheduler as synchron mode (only the source of the beat position differs).
- Test with known numbers: start and end sample of every sound are computed by a pure function (no audio, no JUCE GUI) and unit-tested
  (several pitches, reverse, tempo, sample rates, jump-in, tempo change).

## 4. Architecture (on the template)
- `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE` in CMake; stereo out; the template's presets, day/night theme, state, pluginval, tester and manual are kept.
- `StepScheduler` (algorithm, no GUI): grid + beat position -> list of sound starts (position in block, offset into sample, ratio, reverse). Pure and testable.
- `SampleVoice` / voice pool: resampling playback (forward and reverse, region, pitch ratio). Several voices at once are necessary
  (a reverse sound overlaps the preceding steps); count and stealing rule open (question 7).
- `SampleHolder`: loads the file (JUCE `AudioFormatManager`), holds the data, thread-safe swap with the audio thread (no allocation or file access in `processBlock`).
- GUI: `WaveformView` with two handles, `StepGrid` (16 steps x [step button, reverse, play-to-end halves, pitch knob]), tempo section (sync button, BPM display, play/pause), base-note control.
- Parameters per template rule: one struct per parameter (`ParameterSpec`, `jade::makeParameter`), controls with JUCE attachments so that automation, undo and the red Save button work.
  16 steps x 4 parameters (on, reverse, play to end, pitch) plus region start/end, base note, sync mode, internal BPM, play state.
- State and presets: the pattern and the region are in the preset; whether the sample itself is stored in the state or only its path is open (question 5).

## 5. Development rules (binding)
- The project `CLAUDE.md` is the one of AAT2 (conventions, workflow rules, traps) with the project description adapted in W1.
- Step by step. For each new feature: design note, implementation, function test, then code review and documentation.
- Documentation (README, manual, controls list) in the same change as the code. Versioning as in the global rule; documentation-only changes need no new version.
- Release tags only on the user's explicit request. Report results honestly (failed tests, skipped steps, not tested on Windows/macOS).

## 6. Work packages (proposal, each with a definition of done)
| # | Work package | Done when |
|---|---|---|
| W1 | **Skeleton** from AAT2: new folder/repository, rename, `IS_SYNTH`, CLAUDE.md adapted, build and pluginval pass, empty instrument in a DAW | Builds Debug and Release; pluginval SUCCESS; loads as an instrument. |
| W2 | **Design note and algorithm of play to end**: the pure scheduling function with unit tests (section 3), questions 1-3, 9 decided | All test cases (pitch, reverse, tempo, rates, jump-in) give the expected start/end samples. |
| W3 | **Sample engine**: loading, region, resampling playback (forward, reverse), voice pool, MIDI note trigger with base note | Tester `render` reproduces known pitch and length; no allocation in the audio thread. |
| W4 | **Step sequencer**: grid, per-step on/reverse/pitch, internal clock and play/pause, synchron mode from the playhead (without play to end) | Pattern plays in time in internal mode and in a DAW (Bitwig on Linux). |
| W5 | **Play to end** in the sequencer (the new thing) | In a DAW the end of a reversed cymbal falls exactly on the beat (checked by rendering and measuring the sample position of the end) at several pitches and tempi. |
| W6 | **GUI**: waveform with handles, step grid, knobs, tempo section, day/night, snapshot check | Snapshots in both themes reviewed; handles and knobs automatable. |
| W7 | **Presets, state, manual**, factory pattern(s) with a sample | Preset round trip identical; manual built. |
| W8 | **Extras by decision** (e.g. swing, more samples, pattern length, MIDI-triggered play to end) | Per feature. |

## 7. Decisions
| Topic | Status |
|---|---|
| Basis: AdvancedAudioTemplate, branch AAT2 | **DECIDED** (2026-10-07) |
| Plugin type: instrument (VST3, AU, Standalone), one sample, 1/16 grid | **DECIDED** (author's first ideas) |
| Engine: plain resampling (pitch and time change together); pitch per step +-24 semitones; base note adjustable | **DECIDED** (author's first ideas) |
| Synchron mode (host playhead) and internal mode with play/pause | **DECIDED** (author's first ideas) |

## 8. Open questions (my proposal first; please answer or correct)
1. **What exactly does "play to end" anchor to?** The sound ends (a) at the **start of the clicked step** (the downbeat of the step; typical for a reverse cymbal that leads *into* the step), or (b) at the **end of the clicked step** (= start of the next step)? Your text says "end of the clicked beat". Proposal: the two half buttons left/right are the two choices: left half = ends at the start of this step, right half = ends at the end of this step. Is that what you meant, or are the halves something else (e.g. left = "play to end", right = "play from start")?
   Answer: forget left and right. It meant, you end in this step. If you want an intro to the next full beat, you woould click the last step and switch on play to end
2. **Grid length:** 16 steps = one bar of 4/4 (1/16 notes) or longer/selectable (8, 16, 32, 64)? Time signatures other than 4/4? Proposal: fixed 16 steps for the demonstrator; the pattern position is derived from the DAW's ppq position modulo 4 beats.
The pattern can be at max 32 steps and could be shorten to 8steps
3. **Step off and play to end:** does a step button need to be *on* for play to end (play-to-end as a modifier of the step) or does play to end alone trigger a sound? Proposal: it is a modifier; reverse and play to end combine (reverse + play to end is the main case).
Answer see comments, solved differently
4. **MIDI:** should notes also play the sample (pitch from the note, relative to the base note), independent of the grid? Proposal: yes, simple polyphonic trigger, no play to end for MIDI notes (the DAW would have to send them early).
Answer yes, but with velocity
5. **Sample storage:** state stores only the file path (small, but breaks when the file moves), or the audio data itself (robust, large state)? Proposal: path plus a copy in the user's data folder; decide in W7. Loading by drag and drop and a load button.
Answer see comments.
6. **Note naming:** "C-4" is MIDI note 60 (C4, Middle C) or 72 (Yamaha/FL C5 convention)? Proposal: MIDI 60 = C4 as the default base note.
C-4 = 60, yes
7. **Voices:** 16 simultaneous voices with oldest-voice stealing, resampling interpolation: Hermite cubic (there is `~/AudioDev/hermite-cubic-curve.h`) or linear? Proposal: Hermite for quality, linear as a first step in W3. Declick (short fade at region ends)?
DeClicking by short fade or zero crossing should be a button but only if the user changes start or end time.
8. **Fractional start:** with pitch changes the exact sample position of the start is often fractional. Proposal: start the interpolator at the fractional phase, so the end is exact to a fraction of a sample, not only to 1 sample.
Simple is enough, since the timinmg is irrelevant for 1ms which is 48 samples at 48kHz. 
9.  **Tempo change during a sound:** accept that the end drifts (simple), or follow with a live change of the playback ratio (exact, but the pitch moves while the tempo ramps)? Proposal: simple first, document it.
Answer, Tempo changes rarely, ignore
10. **Latency / transport jumps:** with look-ahead the start can lie in the past after a jump: start with an offset into the sample (section 3) or skip the sound? Proposal: start with offset; for loops in the DAW the pattern keeps its meaning.
skip the sound, it should be clear, why this happened. Perhaps a small notification
11. **Plugin name, plugin code and manufacturer code** (CMake `PLUGIN_CODE`), license text (template default: MIT + AGPL note).
template Default: MIT + AGPL note. Proposal: MIT + AGPL note, plugin name "DrumFeatureDemonstrator", code "DFD1", manufacturer "Jade_Hochschule".
12. **Scope:** is one pattern enough or are patterns/presets of patterns wanted (the template's preset handler stores the whole state, so a pattern = a preset)? Proposal: the preset handler is the pattern memory.
Agree and one pattern is enough for the demo. The user can save a preset with a different sample and pattern.

## 9. Progress
- 2026-10-07: this planning document created from the author's first ideas.


## Comments:
- Change the GUI idea. One button below the grid is enough. Click once its changes its state to reverse, click again to change click to end, another click turns it off again. This does not include the case the user wants to play the sample in forward mode but start, so that it ends on th ebeat. Solvable by the user. It could reverse the sample beforehand. Not our concern. this is a demo
- I do not agree with your asseesmant that the static internal blocksize will not work. The program knows exactly when the next beat will follow (assuming no change in tempo and no loop jump in a fraction of a beat) and therefore can compute whick block and which sub-block to fill at the correct startsample.
- jumping in: No, if the starttime is already gone, no audio is played
- I forgot velocity as another parameter per step. It should only vary the gain of the sample
- Add USE_MIDIKEYBOARD from the template, to play the sample at different ptches and velocities,
- Add Load/save sample button. A sample should be dropable onto the waveform view, and the sample should be stored in the preset (or at least a copy in the user's data folder).