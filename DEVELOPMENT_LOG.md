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

