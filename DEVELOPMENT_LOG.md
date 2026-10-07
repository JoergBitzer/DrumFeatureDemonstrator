# Development log (Claude Code, autonomous phase)

Started 2026-10-07 on the author's request: "Just go ahead until you think a solution is reached. But keep the branch, commit,
merge way, so that we can go back to each state afterwards. Write all your comments and results in an extra file."

## How to go back to a state
- Every work package (WP) is a branch `feature/...` that is merged into `main` with `--no-ff` (a merge commit marks the state).
- Every merged state has a tag `state-W<n>-<version>` (not `v*`, so no release workflow starts). `git checkout state-W3-0.2.0` shows the state after W3.
- Nothing is pushed to the public remote during this phase; push (branches and tags) when the author says so.
- Builds use at most half of the cores. Debug build + unit tests + pluginval before every commit; the Release build is installed
  to `~/.vst3/` at the marked points (the rule of `CLAUDE.md`).

## States so far
| Tag | What |
|---|---|
| `state-W1-0.0.1` | skeleton from AAT2 as instrument |
| `state-W2-0.1.0` | pure step scheduler with play to end, unit tests |
| `state-W3-0.2.0` | sample engine, MIDI notes with velocity, sample loading (tested by the author in Bitwig: shows as instrument) |
| `state-W5-0.3.0` | sequencer, internal clock, 32 x 4 step parameters, play to end in the plugin (W4 + W5) |
| `state-W6-0.4.0` | GUI: waveform with handles and drop, step grid, tempo section, notifications |
| `state-W7-0.5.0` | sample stored with presets and DAW projects, factory presets, manual |
| `state-W7-0.5.1` | review fix: no reallocation for very long blocks (this is the "solution" state) |

## Log
(newest at the end; each entry: what was done, result, decisions taken without asking, open points)

### W4 + W5: sequencer and play to end (branch `feature/sequencer`, version 0.3.0)
- Done together, because the scheduler of W2 already contained the play-to-end rule: `src/Sequencer.*` (transport handling: continuity check, notification count, current step, `InternalClock`), `src/StepParameters.*` (4 parameters per step, 32 steps = 128 parameters, ids `Step01Active` ... `Step32Velocity`), four more parameters (`PatternLength`, `SyncToHost`, `InternalBpm`, `InternalPlay`). `DrumFeatureDemonstratorAudio` reads the pattern from the parameters each block, takes the transport from the host (`AudioPlayHead`) or the internal clock and adds the sound starts to the voice bank, next to the MIDI notes.
- Results: unit tests (scheduler, sample engine, sequencer) pass, including an end-to-end test (clock -> sequencer -> voice bank -> output buffer) for 5 pitches x 2 file rates: the last sample of a reverse + play-to-end sound lies within one resampling step of the end of its step. Rendered with the Tester (`--set SyncToHost=0 --set InternalPlay=1 --set Step16Active=1 --set Step16Mode=2 ...`, 0.3 s ramp sample, 120 bpm, 48 kHz): the reversed sound runs 1.70000 s .. 1.99996 s, i.e. it ends at 2.0 s = end of step 16; the next pass starts its forward step 1 at 2.00002 s; a +12 semitone reverse step lasts half as long.
- Decisions taken without asking: the internal clock really pauses (position is kept); pressing play after a pause continues in the grid. A start or jump inside a run-up counts a skipped sound (the GUI notification comes in W6). The notification counter and the "invalid step" mask (sample too long for play to end) are published by the audio thread as atomics for the GUI.
- **Environment finding (not a code problem):** on this machine an external monitor (DisplayPort-3) is now mirrored at the same position as the laptop display. In Debug builds JUCE asserts in `juce_Displays.cpp:393` ("all display nodes must have a parent") as soon as a window opens, so `run_pluginval.sh` (which fails on assertions) fails in Debug with the editor tests, also for the Tester snapshot. The Release build of W3 passes the full pluginval; the Debug build of W4 passes pluginval with `--skip-gui-tests` (3 runs, 0 assertions). `tools/run_pluginval.sh` got the variable `PLUGINVAL_EXTRA_ARGS` for this. The GUI tests run again as soon as the display setup is not mirrored (or in CI).
- Not tested: a real host (Bitwig) with the sequencer; Windows/macOS.

### W6: GUI (branch `feature/gui`, version 0.4.0)
- New: `gui/WaveformView.*` (waveform from the sample, two draggable handles connected to the parameters `RegionStart` / `RegionEnd` through `juce::ParameterAttachment` (gestures: undo, automation, red Save button work), drop target for sample files, zero-crossing snap while dragging if the new parameter `Declick` is on), `gui/StepGrid.*` (32 columns in 2 rows of 16: step button, mode button with arrow icons drawn as paths (forward / reverse / reverse + bar = play to end), pitch knob, velocity knob, step number; columns beyond the pattern length are dimmed; the playing step has an orange frame; a mode button is greyed while its step is off and red-outlined if play to end is impossible), and the main GUI: load button, sample name, declick toggle, base note (shown as note name, C4 = 60), pattern length, host-sync toggle, play/pause (only in internal mode), tempo (host tempo as text in sync mode, slider in internal mode), notification line (invalid play to end is shown as long as it lasts, a skipped sound for about 3 s).
- GUI size: 1000 x 620 minimum (`PluginSettings.h`), scales with the template's aspect-ratio logic. Colours come from the LookAndFeel (day and night checked with Tester snapshots), fixed colours as 8-digit hex.
- A GUI timer (30 Hz) reads the atomics of the audio thread (current step, invalid mask, skipped counter, host tempo).
- Result: both themes look right (snapshots looked at). Real examples worth knowing: with a MusicRadar cymbal (about 2 s) at 120 bpm and 16 steps the notification "Play to end impossible" correctly appears, because the sample is as long as the whole pattern; with `RegionEnd=0.3` it is fine.
- Finding: the Debug assertion `juce_Displays.cpp:393` (mirrored monitor) also appears in every Tester snapshot; the pictures are still written. pluginval Debug with `--skip-gui-tests`: 3/3 pass, 0 assertions.
- Not done / ideas: the knobs are not labelled in the GUI (tooltips only; pitch knob left, velocity knob right of each pair); a legend row could be added. Mouse interaction with the waveform handles and the drop were not exercised by an automated test (only drawing is checked); to be tried by hand in the Standalone.

### W7: state, sample storage, factory presets, manual (branch `feature/presets-state`, version 0.5.0)
- Sample storage (decision of the plan, extended): the sample is copied into `<user presets folder>/Samples/` (`src/SampleStore.*`; same name + same content = reused, same name + other content = `name_2.wav`), and the name is a property `SampleFile` of the plugin state. Therefore a **preset** carries the sample name (small files), and a **DAW project** carries the sample itself (base64 of the file, up to 8 MB, as a child `SampleData` of the state written by `getStateInformation` only, never into presets); `setStateInformation` writes the file back if it is missing. A change of `SampleFile` (also by `replaceState`, which redirects the tree: `valueTreeRedirected`) loads the sample asynchronously on the message thread; a preset without a sample keeps the current one (its name is written back into the state); a missing file shows "Missing: name".
- Result (Tester `--save-state` / `--load-state`, new): a state saved after loading a MusicRadar cymbal and a pattern, restored in a **fresh home folder** (empty samples folder), gives a bit-identical render (`cmp`), and the sample file is re-created in the samples folder.
- Factory presets (`res/presets/*.xml`, made by `tools/make_factory_presets.py`, deployed by the template's `DeployFactoryPresets`): "Four on the floor", "Reverse into the beat" (steps 8 and 16 reverse + play to end), "Offbeat riff", "Short loop of 8". They contain no sample. Checked: with a cymbal (region 15 %) the reversed sounds end exactly at 1.0000, 2.0000, 3.0000 and 4.0000 s (end of steps 8 and 16 at 120 bpm).
- Manual: `docs/manual/ManualDrumFeatureDemonstrator.tex` written (introduction, use, play-to-end notes, controls, how it works), `controls.tex` written by hand (the 128 step parameters are described once), screenshot `images/GUI.png` from the Tester, PDF built with latexmk and committed (compiles without errors). `release/ReadMeFirst.txt` adapted to an instrument.
- Tests: new unit tests for the sample store; all unit tests pass; pluginval Debug `--skip-gui-tests` 3/3.
- Decision taken without asking: the embedded sample limit is 8 MB (bigger samples are only referenced by name in the project state: they then rely on the samples folder).

### Review fix (branch `fix/sound-start-capacity`, version 0.5.1)
- Own code review of the audio path found one real-time risk: `scheduleBlock` could push more sound starts than the reserved capacity (32) for a very long block (offline render with a huge block size, 8 steps at a fast tempo), which would allocate in the audio thread. Now `kMaxSoundStartsPerBlock = 256` is reserved and `scheduleBlock` stops there; a unit test checks that a ten-minute block neither exceeds the limit nor reallocates.
- Known minor points, not changed: `m_sampleName` / `m_loadedSampleFileName` (juce::String) are written on the message thread and read by `getStateInformation` which a host may call from another thread (a torn read is theoretically possible; the strings are short and change only on load). The GUI notices a changed sample by comparing pointers on a 30 Hz timer.

## Summary: where the product stands (version 0.5.1)
**What it is now:** an instrument plug-in (VST3, Standalone; AU on macOS by CI) that plays one sample from a 8-32 step 1/16 grid and from MIDI, with per-step on / mode (forward, reverse, reverse + **play to end**) / pitch (+-24 st) / velocity, a waveform with start/end handles and drop, host sync (beat position from the DAW) or internal beat with Play/Pause, base note, declick, presets (with the sample name), DAW projects (with the sample), four factory presets, a manual and release notes.

**How the new feature was verified (all without a DAW):**
1. pure scheduling function with unit tests (known numbers, 5 pitches x 4 tempi x 3 host rates x 2 file rates x 3 lengths: start + duration = end of the step within one sample);
2. end-to-end unit test (clock -> sequencer -> voice bank -> buffer): the last audible sample of the reversed sound is within one resampling step of the end of its step;
3. rendered through the real plug-in code with the Tester (`render`, internal beat): a 0.3 s ramp, and a MusicRadar cymbal with the factory preset "Reverse into the beat": the reversed sound ends at 1.0000 / 2.0000 / 3.0000 / 4.0000 s, i.e. exactly at the end of steps 8 and 16 at 120 bpm.

**What was not tested (be aware):**
- the plug-in in a real host with a moving playhead (Bitwig): the host path (`AudioPlayHead` -> `BlockTransport`) is only covered by the unit tests of the sequencer, not by a host. Please try: Host sync on, play, loop, jump into a run-up (the notice should appear), change the tempo.
- the mouse interaction (handle drag, drop, mode button clicks) was not driven by an automated test; only the drawing was checked with snapshots.
- Windows and macOS (CI builds only start with a version tag; no tag was set).
- the GUI tests of pluginval in Debug (see the monitor finding above); the Release build passes the complete pluginval.

**Ideas for later (W8):** swing, a second sample or per-step sample choice, forward + play to end, play to end for MIDI notes, Hermite interpolation, a legend for the knobs, fade-based declick, a count-in for the internal beat, following tempo changes during a run-up.

**Where to look:** `planning.md` (plan, decisions, progress), this file (what happened, results, findings), `docs/design/W2-play-to-end.md` (the algorithm), `docs/manual/` (user manual).

