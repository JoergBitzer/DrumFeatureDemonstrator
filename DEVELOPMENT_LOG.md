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

