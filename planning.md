# DrumFeatureDemonstrator - planning

Status: draft v3 (2026-10-07), the five remaining questions of v2 are answered and worked in (earlier versions are in the git history). Decisions are marked **DECIDED** (section 7); no open questions remain (section 8). No code has been written yet.
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
2. **Step grid of 1/16 notes**, **8 to 32 steps** (pattern length adjustable in single steps, default 16; odd lengths give polyrhythms against the bar). Per step:
   - a **step button** (on/off),
   - **one mode button below the step**: click once = *reverse*, click again = *reverse + play to end*, click again = back to *forward* (the normal state). The mode button of an **inactive step is disabled** (its stored mode is kept). *Reverse + play to end* is only selectable while the sample fits into the pattern (see section 3),
   - a small **pitch knob**, +-24 semitones,
   - a small **velocity knob**; velocity only changes the gain of the sample.
3. **Play engine:** plain resampling (pitch and duration change together, no time stretching). **Base note** is adjustable, default C4 (= MIDI note 60).
4. **MIDI keyboard** (the template switch `WITH_MIDIKEYBOARD`): the sample is also played from MIDI notes (on the keyboard of the GUI and from the DAW), pitch relative to the base note, **with velocity** (gain). No play to end for MIDI notes.
5. **Tempo:** a button switches between *synchron mode* (follows the DAW; the BPM is displayed) and *internal mode* (own BPM).
   The internal beat can be played by the plugin itself, so a **play/pause button** is needed (active in internal mode).
6. **Declick button:** if on, a change of the start or end handle by the user snaps to the nearest zero crossing (does not change the sound; a fade is a possible extra in W8). The button has no effect on a handle that is not moved.
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
  *next* pass can start in the current pass; with the length limit below a sound starts at most one pass earlier.
- **Fixed-size blocks work (author's remark).** The plugin knows exactly when the next beat follows (tempo constant, no loop jump inside a fraction of a beat),
  so it can compute the block of the `SynchronBlockProcessor` and the sample offset inside that block at which the sound starts. The offset is simply a start sample inside the block.
  To verify in W2: the playhead position belongs to the host block, the fixed-size blocks may lag behind by the rebuffering delay; the scheduler has to use the position of the block it is filling.
- **Length limit (decided):** play to end is only allowed if `duration` is at most the pattern length (`patternSteps * samplesPerStep`). The user can load a long sample, but
  has to shorten it with the end handle (or choose a higher pitch or a longer pattern) before *reverse + play to end* can be selected. The check is made whenever the region, a pitch, the base note, the tempo or the pattern length changes;
  a step that becomes invalid is marked in the GUI and does not play (notification), it never plays at a wrong time. A long sample still plays normally in forward and reverse mode.
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
- **Sample storage (decided):** the sample belongs to the preset. The user's data folder gets a copy of the loaded sample (a `Samples` subfolder next to the presets; sample files there are `.wav`, nothing else ends in `.xml`);
  the preset stores the name of this copy and the plugin state does the same. Embedding the audio data in the preset XML stays an option for W7 if the copy turns out to be fragile (a preset sent to another computer lacks the file).
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
| W3 | **Sample engine**: loading (button, drop), region, declick (zero-crossing snap), resampling playback (forward, reverse), voice pool, MIDI notes with velocity and base note | Tester `render` reproduces known pitch and length; no allocation in the audio thread. |
| W4 | **Step sequencer**: grid (8-32 steps), per-step on/mode/pitch/velocity, internal clock and play/pause, synchron mode from the playhead (without play to end) | Pattern plays in time in internal mode and in a DAW (Bitwig on Linux). |
| W5 | **Play to end** in the sequencer (the new thing), skip rule and notification | In a DAW the end of a reversed cymbal falls exactly on the end of its step (checked by rendering and measuring the sample position of the end) at several pitches and tempi; a start in the past is skipped and notified; play to end is refused for a sample longer than the pattern. |
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
| Fixed-size block processing is kept (the offset inside the block is computed) | **DECIDED** (2026-10-07, author). **Changed in W3 (please confirm):** `g_desired_blocksize_ms = 0`, i.e. the `SynchronBlockProcessor` runs in its direct mode (host blocks, no rebuffering): an instrument gains nothing from fixed blocks, rebuffering would add latency to MIDI and the sequencer, and the offset inside the host block is exact anyway. The setting is one constant in `PluginSettings.h`. |
| Start already gone (jump, loop, transport start): no audio, with a small notification | **DECIDED** (2026-10-07) |
| Tempo change during run-up or sound: ignored | **DECIDED** (2026-10-07) |
| Timing: whole-sample start is enough | **DECIDED** (2026-10-07) |
| Declick: a button, active only when the user changes start or end | **DECIDED** (2026-10-07) |
| Sample: load/save button, drop onto the waveform, stored with the preset (at least a copy in the user's data folder) | **DECIDED** (2026-10-07) |
| **Drum samples for tests and the factory preset: the MusicRadar SampleRadar packs** (free, royalty-free); local collection `~/Music/samples/musicradar_sub/`, automatic download for CI by `tools/fetch_samples.sh` (pack "Junk Drums", 101 MB, `https://cdn.mos.musicradar.com/audio/samples/musicradar-junk-drums-samples.zip`); not stored in the repository | **DECIDED** (2026-10-07, author) |
| Step pitch is relative to the base note (the base note only matters for MIDI notes and for the mapping note -> speed) | **DECIDED** (W3, 2026-10-07) |
| License MIT + AGPL note (template default); plugin name `DrumFeatureDemonstrator`, code `DFD1`, manufacturer `Jade_Hochschule` (code `IHAJ` of the template) | **DECIDED** (2026-10-07) |
| One pattern per preset; the preset handler is the pattern memory | **DECIDED** (2026-10-07) |
| The mode button of an inactive step is disabled (stored mode kept) | **DECIDED** (2026-10-07) |
| Pattern length: any value from 8 to 32, default 16 | **DECIDED** (2026-10-07) |
| Declick: zero-crossing snap of the handle; a fade is a possible extra later | **DECIDED** (2026-10-07) |
| Sample storage: copy in the user's data folder first; embedding in the preset decided in W7 if needed | **DECIDED** (2026-10-07) |
| Length limit: a long sample can be loaded, but *play to end* is not allowed if the duration exceeds the pattern length; the user shortens it with the end handle | **DECIDED** (2026-10-07) |

## 8. Open questions
None. (The five questions of draft v2 are answered and moved to section 7.)

## 9. Progress
- 2026-10-07: this planning document created from the author's first ideas (v1).
- 2026-10-07: git repository created, v1 with the author's answers committed; v2 written (answers and comments worked in).
- 2026-10-07: answers to the five open questions committed; v3 written (no open questions left).
- 2026-10-07: W1 (skeleton) done: template copied, renamed, instrument settings, JUCE 9.0.3 submodule; Debug and Release build, pluginval SUCCESS.
- 2026-10-07: W2 done (branch `feature/scheduler`, version 0.1.0): `src/StepScheduler.*`, `tests/StepSchedulerTests.cpp`, `docs/design/W2-play-to-end.md`; unit tests pass (also checked by a mutation of the anchor).
- 2026-10-07: W3 done (branch `feature/sample-engine`, version 0.2.0): `src/SampleData`, `SampleVoice`, `VoiceBank` (16 voices, linear interpolation, sample-exact triggers), `SampleHolder` (lock-free handover), `SampleLoader`, MIDI note-on with velocity and base note, parameters BaseNote / RegionStart / RegionEnd, load button and file drop on the GUI (waveform view comes in W6). Tester: `render - out.wav --sample file --note n,vel,seconds`. Checked with a 1 kHz sine and a MusicRadar cymbal: pitch within 0.1 %, length within 3 samples, velocity linear. Not yet: the sample is not stored in the state/preset (W7), the zero-crossing snap exists (`findNearestZeroCrossing`) but is wired to the handles in W6, Hermite interpolation later.
- 2026-10-07: W4 + W5 done in one branch `feature/sequencer` (version 0.3.0): sequencer, internal clock, 32 x 4 step parameters, host playhead; play to end verified by an end-to-end unit test and a Tester render (see DEVELOPMENT_LOG.md).
