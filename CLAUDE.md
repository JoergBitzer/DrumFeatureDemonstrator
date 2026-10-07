# CLAUDE.md -- instructions for Claude Code in the DrumFeatureDemonstrator (made from the AdvancedAudioTemplate, AAT2)

## The project
DrumFeatureDemonstrator: a one-sample drum player (instrument: VST3, AU on macOS, Standalone) with a 1/16 step grid
and the new feature "play to end" (the sample is started so that it ends exactly at the end of its step).
Made from the AdvancedAudioTemplate, branch AAT2. **The plan, the decisions and the work packages are in `planning.md`; read it first.**
Until the skeleton work package (W1) is finished, parts of the code are still the template's example code.
- `DrumFeatureDemonstrator.h/.cpp`: the parameters (structs), the algorithm (`DrumFeatureDemonstratorAudio`, fixed-size
  blocks via `SynchronBlockProcessor`) and the GUI (`DrumFeatureDemonstratorGUI`). Most work happens here.
- `PluginProcessor.*`, `PluginEditor.*`: glue code (presets, state, window, preset bar); change rarely.
- `PluginSettings.h`: block size, GUI size, preset-bar and keyboard layout.
- `tools/`: template helpers -- `ParameterSpec.h`, `DayNightLookAndFeel.*`, `PresetHandler.*`,
  `LogFrequencyRange.h`, `run_pluginval.*`, `tester/`.
- `CMakeLists.txt`: version, plugin codes, switches `WITH_PRESETHANDLERGUI`, `WITH_DAYNIGHT` (needs the
  preset handler), `WITH_MIDIKEYBOARD`, source list.
- Docs: `README.md`, `HowToTestYourPlugin.md`, `docs/HowToDoc.md`, manual in `docs/manual/`.

## Build
```console
git submodule update --init                      # JUCE (pinned submodule), once
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DAAT_BUILD_TESTER=ON
cmake --build build --target DrumFeatureDemonstrator_VST3 DrumFeatureDemonstrator_Standalone DrumFeatureDemonstrator_Tester
```
Develop with the Debug build (JUCE assertions are on); releases are Release builds made by CI.
For the review in a DAW, build Release as well (a Debug build is slow there and stops at assertions):
```console
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --target DrumFeatureDemonstrator_VST3 DrumFeatureDemonstrator_Standalone
```
Inside a bigger CMake project (e.g. AudioDev) this folder can also be added with `add_subdirectory`;
then the parent's JUCE is used.

## Workflow rules (from the author, follow them)
- Each new request gets a new git branch with a descriptive name (e.g. `feature/lowpass-filter`,
  `fix/preset-loading`). Commit all changes with clear, concise commit messages.
- Merge into main and push only when the user says so. Before merging, check the result
  (conflicts, conflict markers) before committing or continuing.
- Release tags (`vX.Y.Z`) only on the user's explicit request -- a tag triggers a public release.
- Versioning (`project(... VERSION X.Y.Z)` in CMakeLists.txt), once per logical change that gets
  committed: a new feature raises the second number and sets the third to 0 (1.0.3 -> 1.1.0); a fix or
  other change raises the third (1.1.0 -> 1.1.1). Documentation-only changes need no new version.
- Update the documentation (README, manual, controls list) in the same change as the code.
- Install for review, automatically: after a successful build and pluginval run, copy the
  **Release** build of the plugin into the user's plugin folder (replace the old copy) so the change
  can be checked in a DAW, and say so in the report. Linux: `~/.vst3/`; macOS:
  `~/Library/Audio/Plug-Ins/VST3/` and the AU to `~/Library/Audio/Plug-Ins/Components/`; Windows:
  `C:\Program Files\Common Files\VST3\` (needs admin rights; otherwise tell the user the path of
  the built plugin). Install the build that was tested (run pluginval on the Release build too).
- Report results honestly: failed tests, skipped steps, things not tested (e.g. Windows/macOS).

## Before every commit
1. Build (Debug) without errors and warnings.
2. `tools/run_pluginval.sh build/DrumFeatureDemonstrator_artefacts/Debug/VST3/DrumFeatureDemonstrator.vst3`
   -- all runs SUCCESS, zero JUCE assertions.
3. If the sound should not change (refactoring): render before and after with
   `DrumFeatureDemonstrator_Tester render in.wav out.wav` and compare the files (identical = same sound).
4. After GUI changes: look at `DrumFeatureDemonstrator_Tester snapshot gui.png` (both themes, see below).

## Conventions
- Readable code (code style of the author, binding): no one-liners, no magic numbers, no magic
  constants, no magic strings. Readability is more important than performance (but not too much).
- No ternary operators; no `if`/`else` inside expressions; no `goto`; no `using namespace` in headers.
- Constants with `constexpr`, not `#define`; no `#define` macros (inline functions or templates);
  JUCE's own macros are the exception.
- Headers include as little as possible: forward declare, include only what is used, no includes in
  headers just to pass them on.
- Member variables `m_...`, constants `kName`.
- Parameters: one struct per parameter in `DrumFeatureDemonstrator.h` (ID, name, unit, range, default,
  `numDecimalPlaces`, `logFrequency`, `help`); create it with `jade::makeParameter(spec)`; show
  `jade::helpText(spec)` as tooltip. Don't build `AudioParameterFloat`s by hand.
- Controls: connect them with JUCE attachments (SliderAttachment etc.) or `juce::ParameterAttachment`;
  then automation, undo and the red "changed" Save button work (they use parameter gestures).
- Colours: take them from the LookAndFeel (`findColour`), so the day/night theme applies; fixed
  colours as 8-digit hex `juce::Colour(0xAARRGGBB)` (alpha first), not floats or `juce::Colours::` names.
- GUI text: only Latin-1 characters (no emoji, arrows, warning signs -- Windows fonts lack them); draw
  icons as `juce::Path` (example: `ThemeButton` in `tools/DayNightLookAndFeel.cpp`).
- Keep the algorithm free of GUI code (no `juce_gui_*` includes), so it can be tested headless.
- Factory presets: XML files embedded with `juce_add_binary_data` and `FACTORY_PRESETS`; add
  `presetversion="N"` to the root element and raise it when a preset changes (see README).
- Per-user files (presets, `user.settings`) live in `PresetHandler::getUserPresetsFolder()`; nothing
  there may end in `.xml` except presets.

## Traps
- `PluginEditor.*` and `PluginProcessor.*` use Windows line endings (CRLF): keep them when editing.
- Never change anything in `JUCE/` (submodule).
- Tests must not touch the user's real presets and settings: on Linux/macOS run the Standalone, the
  Tester or pluginval with a temporary home folder (`HOME=$(mktemp -d) ...`; `run_pluginval.sh`
  does this itself). Day theme for a snapshot: write `user.settings` with
  `<VALUE name="dayTheme" val="1"/>` into the temporary preset folder.
- The workflow only builds on tags and "Run workflow" (main branch). To test CI changes on a branch,
  add a temporary `branches:` trigger and remove it again before merging.

## Releases (CI)
`.github/workflows/release.yml`: a tag `vX.Y.Z` that matches the version in CMakeLists.txt builds
Windows, macOS (Universal, VST3 + AU) and Linux, tests with pluginval (and `auval` on macOS), and
creates the GitHub release with one zip per system (plugins, Standalone, `release/ReadMeFirst.txt`,
licenses, `docs/manual/*.pdf`). Before a release: version raised, manual updated
(`\pluginversion`, `controls.tex` via `DrumFeatureDemonstrator_Tester --manual tex`), PDF rebuilt and committed.
