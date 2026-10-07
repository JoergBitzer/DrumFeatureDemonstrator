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
