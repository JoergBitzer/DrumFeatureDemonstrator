# DrumFeatureDemonstrator - planning

Status: draft v2 (2026-10-07), revised with the author's answers and comments to draft v1 (kept in the git history). Decisions are marked **DECIDED** (section 7); the few remaining questions are in section 8. No code has been written yet.
Basis: the AdvancedAudioTemplate, branch `AAT2` (`~/AudioDev/AdvancedAudioTemplate`). This time the plugin is an **instrument**.
Plugin name: `DrumFeatureDemonstrator` (also the folder name).

## 1. Idea and purpose
A simple one-sample drum player with a step grid. It is a *demonstrator* for one new feature: **"play to end"**.

**The new thing.** The plugin knows exactly how long the sample sounds at the chosen pitch:
`duration = number of samples of the region * resampling factor` (resampling factor = 1 / playback ratio, including the
sample-rate ratio file/host). From the beat position the DAW reports (`AudioPlayHead`) it can therefore compute the **start
time so that the sample ends exactly at the end of the step** in which "play to end" is set. Main use: a reversed sound
(a reverse cymbal) that runs into the next full beat as a typical intro: set play to end on the last step before the beat, at any pitch, at any tempo.
This is a demo: the combination "forward sample, but ending on the beat" is not offered; the user reverses the sample beforehand (not our concern).

## 2. Features
1. **One sample**, loaded by the user with a load button or by **drag and drop onto the waveform view**; a save button saves the sample.
   The GUI shows the waveform with two handles (start and end of the region that is played). The sample is part of the preset (section 4).
2. **Step grid of 1/16 notes**, **8 to 32 steps** (pattern length adjustable, default 16). Per step:
   - a **step button** (on/off),
   - **one mode button below the step**: click once = *reverse*, click again = *reverse + play to end*, click again = back to *forward* (the normal state). The mode is only meaningful for an active step,
   - a small **pitch knob**, +-24 semitones,
   - a small **velocity knob**; velocity only changes the gain of the sample.
3. **Play engine:** plain resampling (pitch and duration change together, no time stretching). **Base note** is adjustable, default C4 (= MIDI note 60).
4. **MIDI keyboard** (the template switch `WITH_MIDIKEYBOARD`): the sample is also played from MIDI notes (on the keyboard of the GUI and from the DAW), pitch relative to the base note, **with velocity** (gain). No play to end for MIDI notes.
5. **Tempo:** a button switches between *synchron mode* (follows the DAW; the BPM is displayed) and *internal mode* (own BPM).
   The internal beat can be played by the plugin itself, so a **play/pause button** is needed (active in internal mode).
6. **Declick button:** if on, a change of the start or end handle by the user snaps to a zero crossing (or applies a short fade; decided in W3). The button has no effect on a handle that is not moved.
7. A small **notification** in the GUI if a sound was skipped because its start time had already passed (section 3).

## 3. The core algorithm: play to end
Quantities, all in the host's sample domain:
- `regionLength` = (end handle - start handle) in file samples; `ratio` = 2^((note - baseNote) / 12) * fileRate / hostRate (playback speed).
- `duration` = `regionLength / ratio` host samples (the time of the sample at the chosen pitch).
- `samplesPerStep` = hostRate * 60 / bpm / 4 (1/16 = a quarter of a beat).
- `anchor` = the **end of the clicked step** (= start of the next step), in ppq, converted to host samples.
- `startTime` = `anchor` - `duration`. For a step without play to end, `startTime` is the start of the step.

Design points (details in the design note of W2):
- **Look-ahead.** The start lies *before* the step, by up to the whole duration of the sample. The scheduler therefore asks
  "which sound must **start** in this block (or fixed-size block)?" instead of "which step starts in this block?". Because the pattern repeats, a step of the
  *next* pass can start in the current pass; with a duration longer than the pattern, several passes are involved (limit it and say so in the GUI).
- **Fixed-size blocks work (author's remark).** The plugin knows exactly when the next beat follows (tempo constant, no loop jump inside a fraction of a beat),
  so it can compute the block of the `SynchronBlockProcessor` and the sample offset inside that block at which the sound starts. The offset is simply a start sample inside the block.
  To verify in W2: the playhead position belongs to the host block, the fixed-size blocks may lag behind by the rebuffering delay; the scheduler has to use the position of the block it is filling.
- **Jumping in (decided):** if the computed start time is already gone (transport starts or jumps, loop restarts, the run-up lies before the start of the transport), **no audio is played** for this sound
  and the GUI shows a small notification so that the user understands why (flag set in the audio thread, shown by a GUI timer).
- **Tempo changes (decided):** ignored; they are rare. `duration` and the start are computed from the tempo at the time of scheduling; a change during the run-up shifts the end (documented in the manual).
- **Timing accuracy (decided):** simple is enough; the start is rounded to a whole sample (1 ms = 48 samples at 48 kHz is irrelevant), no fractional start phase.
- **Playhead access:** `getPlayHead()->getPosition()` gives `ppqPosition`, `bpm`, `isPlaying`, `timeSignature`, loop points, each optional.
  A host that delivers none of them -> fall back to internal mode (shown in the GUI).
- **Internal mode:** own clock from BPM and sample count; play/pause; same scheduler as synchron mode (only the source of the beat position differs).
- **Pattern position:** (ppq position * 4) modulo the pattern length in steps; 4/4 is assumed, other time signatures only change the position of the pattern against the bar.
- Test with known numbers: start and end sample of every sound are computed by a pure function (no audio, no GUI) and unit-tested
  (several pitches, reverse, tempi, sample rates, pattern lengths, start already gone).

## 4. Architecture (on the template)
- `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`, `WITH_MIDIKEYBOARD`, VST3 category `Instrument` (and `Sampler`/`Drum`), AU type `kAudioUnitType_MusicDevice`; stereo output; the template's presets, day/night theme, state, pluginval, tester and manual are kept.
  Plugin code `DFD1`, manufacturer code `IHAJ` and company name `Jade_Hochschule` of the template.
- `StepScheduler` (algorithm, no GUI): pattern + beat position -> list of sound starts (position in block, ratio, reverse, gain). Pure and testable.
- `SampleVoice` / voice pool: resampling playback (forward and reverse, region, pitch ratio, gain). Several voices at once (a reverse sound overlaps the steps before it); 16 voices with oldest-voice stealing.
  Interpolation: linear in W3 as a first step, Hermite cubic later (`~/AudioDev/hermite-cubic-curve.h` exists).
- `SampleHolder`: loads the file (JUCE `AudioFormatManager`), holds the data, thread-safe swap with the audio thread (no allocation or file access in `processBlock`).
- GUI: `WaveformView` (two handles, drop target), `StepGrid` (8-32 steps x [step button, mode button, pitch knob, velocity knob]), tempo section (sync button, BPM display, play/pause), base-note control, pattern length, declick button, notification, MIDI keyboard.
- Parameters per template rule: one struct per parameter (`ParameterSpec`, `jade::makeParameter`), controls with JUCE attachments so that automation, undo and the red Save button work.
  32 steps x 4 parameters (on, mode with 3 values, pitch, velocity) plus region start/end, base note, pattern length, sync mode, internal BPM, play state, declick.
- **Sample storage (decided as far as the author said):** the sample belongs to the preset. The user's data folder gets a copy of the loaded sample (a `Samples` subfolder next to the presets; sample files there are `.wav`, nothing else ends in `.xml`);
  the preset stores the name of this copy and the plugin state does the same. Embedding the audio data in the preset XML is the alternative if the copy turns out to be fragile (a preset sent to another computer lacks the file); decide in W7.
- Factory preset: pattern with a sample; the sample is embedded as binary data (`juce_add_binary_data`) and copied to the data folder on first use.

## 5. Development rules (binding)
- The project `CLAUDE.md` is the one of AAT2 (conventions, workflow rules, traps, now with the code style rules) with the project description adapted in W1.
- Step by step. For each new feature: design note, implementation, function test, then code review and documentation.
- Documentation (README, manual, controls list) in the same change as the code. Versioning as in the global rule; documentation-only changes need no new version.
- Release tags only on the user's explicit request. Report results honestly (failed tests, skipped steps, not tested on Windows/macOS).
- This repository: `git init` on `main` (2026-10-07). Per the AAT2 workflow rule, each new request gets its own branch once the skeleton exists.

## 6. Work packages (proposal, each with a definition of done)
| # | Work package | Done when |
|---|---|---|
| W1 | **Skeleton** from AAT2: rename, `IS_SYNTH`, `WITH_MIDIKEYBOARD`, CLAUDE.md adapted, build and pluginval pass, empty instrument in a DAW | Builds Debug and Release; pluginval SUCCESS; loads as an instrument. |
| W2 | **Design note and algorithm of play to end**: the pure scheduling function with unit tests (section 3), block/latency question verified | All test cases (pitch, reverse, tempi, rates, pattern lengths, start gone) give the expected start/end samples. |
| W3 | **Sample engine**: loading (button, drop), region, declick, resampling playback (forward, reverse), voice pool, MIDI notes with velocity and base note | Tester `render` reproduces known pitch and length; no allocation in the audio thread. |
| W4 | **Step sequencer**: grid (8-32 steps), per-step on/mode/pitch/velocity, internal clock and play/pause, synchron mode from the playhead (without play to end) | Pattern plays in time in internal mode and in a DAW (Bitwig on Linux). |
| W5 | **Play to end** in the sequencer (the new thing), skip rule and notification | In a DAW the end of a reversed cymbal falls exactly on the end of its step (checked by rendering and measuring the sample position of the end) at several pitches and tempi; a start in the past is skipped and notified. |
| W6 | **GUI**: waveform with handles, step grid, knobs, tempo section, day/night, snapshot check | Snapshots in both themes reviewed; handles and knobs automatable. |
| W7 | **Presets, sample storage, state, manual**, factory pattern with a sample | Preset round trip identical (including the sample); manual built. |
| W8 | **Extras by decision** (e.g. swing, more samples, forward + play to end, MIDI-triggered play to end) | Per feature. |

## 7. Decisions
| Topic | Status |
|---|---|
| Basis: AdvancedAudioTemplate, branch AAT2 | **DECIDED** (2026-10-07) |
| Plugin type: instrument (VST3, AU, Standalone), one sample, 1/16 grid | **DECIDED** |
| Engine: plain resampling (pitch and time change together); pitch per step +-24 semitones; base note adjustable; C-4 = MIDI 60 | **DECIDED** |
| Synchron mode (host playhead) and internal mode with play/pause | **DECIDED** |
| Play to end: the sound ends at the **end of the step** where it is set; to lead into a beat, set it on the last step before the beat | **DECIDED** (2026-10-07) |
| GUI: one mode button below each step (reverse -> reverse + play to end -> forward); no left/right half buttons; no "forward + play to end" | **DECIDED** (2026-10-07) |
| Pattern length 8 to 32 steps | **DECIDED** (2026-10-07) |
| Velocity per step (gain only); MIDI notes play the sample with velocity; `WITH_MIDIKEYBOARD` on | **DECIDED** (2026-10-07) |
| Fixed-size block processing is kept (the offset inside the block is computed) | **DECIDED** (2026-10-07, author) |
| Start already gone (jump, loop, transport start): no audio, with a small notification | **DECIDED** (2026-10-07) |
| Tempo change during run-up or sound: ignored | **DECIDED** (2026-10-07) |
| Timing: whole-sample start is enough | **DECIDED** (2026-10-07) |
| Declick: a button, active only when the user changes start or end | **DECIDED** (2026-10-07) |
| Sample: load/save button, drop onto the waveform, stored with the preset (at least a copy in the user's data folder) | **DECIDED** (2026-10-07) |
| License MIT + AGPL note (template default); plugin name `DrumFeatureDemonstrator`, code `DFD1`, manufacturer `Jade_Hochschule` (code `IHAJ` of the template) | **DECIDED** (2026-10-07) |
| One pattern per preset; the preset handler is the pattern memory | **DECIDED** (2026-10-07) |

## 8. Open questions (remaining; my proposal first)
1. **Mode button without an active step:** the mode only matters for an active step. Proposal: the mode button of an inactive step is disabled (greyed), and its stored mode is kept. Or should a click on the mode button of an inactive step also switch the step on?
Answer: they should be inactive
2. **Pattern length steps:** any value from 8 to 32 (odd lengths give polyrhythms against the bar) or only 8, 16, 24, 32? Proposal: any value from 8 to 32, default 16.
OK
3. **Declick method:** zero-crossing snap of the handle (does not change the sound) or a short fade (2 ms) at the region ends (always click-free, shortens the attack slightly)? Proposal: zero-crossing snap, as the button text says "snap to zero crossing"; a fade can be added in W8.
OK
4. **Sample embedded or copy:** the copy in the user's data folder is planned; embed the audio in the preset (robust, larger files) later if needed (W7)? Proposal: copy first, decide in W7.
OK
5. **Sample length limit:** very long samples make the run-up longer than the pattern. Proposal: limit play to end to a duration of at most one pattern length and show "too long" in the notification instead of playing; longer samples still play normally.
Shorten the sample by changing the end handle so that the sample fits into the pattern length. The user can still load a long sample, but the plugin will not allow "play to end" if it exceeds the pattern length.

## 9. Progress
- 2026-10-07: this planning document created from the author's first ideas (v1).
- 2026-10-07: git repository created, v1 with the author's answers committed; v2 written (answers and comments worked in).
