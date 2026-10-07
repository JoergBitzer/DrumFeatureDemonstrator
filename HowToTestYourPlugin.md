# How to test your plugin

A plugin that works on your computer in your DAW can still crash in another DAW, at another
sample rate, or when the user moves a knob while the audio is running. Testing early saves you
from bug reports later. Start with these three steps; they need no extra knowledge.

## 1. Build and test the Debug version

While you develop, build the Debug version. It is slower, but JUCE checks many things there:
every `jassert(...)` in JUCE and in your code stops the program (in the debugger) or prints
"JUCE Assertion failure" if a condition is wrong. An assertion is always a hint to a real problem --
do not ignore it. Build the Release version for your users; it is faster and has no assertions.

## 2. Test with pluginval before every commit

[pluginval](https://github.com/Tracktion/pluginval) loads your plugin like a DAW and tests it hard:
parameters, automation, different sample rates and block sizes, opening and closing the editor, and
more. It finds bugs that you would otherwise only notice in a DAW, or that your users would find.

Linux/macOS:
```console
tools/run_pluginval.sh path/to/DrumFeatureDemonstrator.vst3
```
Windows (PowerShell):
```console
powershell -ExecutionPolicy Bypass -File tools\run_pluginval.ps1 path\to\DrumFeatureDemonstrator.vst3
```
In AudioDev the plugin is in `build/DrumFeatureDemonstrator/DrumFeatureDemonstrator_artefacts/Debug/VST3/`.

The script downloads pluginval on the first run, tests at the highest strictness level (10) three times
(some bugs only show up now and then; give another number as second argument) and prints SUCCESS or
FAILED per run, with the path to the log of a failed run. On Linux/macOS the script also counts every
JUCE assertion as a failure, and it uses a temporary home folder, so your real presets are not touched.

If a run fails, open the log and search for "FAILED" or "JUCE Assertion": the lines before it show
which test was running. Fix the problem and test again, until all runs pass.

## 3. Test in a DAW (and in JUCE's AudioPluginHost)

Automatic tests do not replace your ears and hands. Load the plugin in your DAW and:
* play audio through it and listen: clicks, dropouts, level jumps?
* move every control while the audio is running, also fast
* automate a parameter, save the project, close and open it again: is everything restored?
* load and save presets, switch between them
* close and open the plugin window, resize it
* use two instances of the plugin at the same time

No DAW at hand? JUCE comes with a simple host: build `JUCE/extras/AudioPluginHost`, then
"Options > Edit the list of available plug-ins" to scan your plugin. The Standalone version of your
plugin (built with the VST3) is also a quick way to test the GUI and the sound.

---

# Advanced testing (AAT2)

The following tools are part of the AAT2 branch only (see README, section AAT2).

## 4. pluginval on GitHub

The release workflow (`.github/workflows/release.yml`) runs `tools/run_pluginval.*` on Windows,
macOS and Linux after every build (Linux under a virtual display, `xvfb`). On macOS the AU is also
checked with `auval`, Apple's Audio Unit validation (the check Logic and GarageBand do before they
load an AU); the AU codes are read from the built plugin, so nothing needs to be set. If a test
fails on one system, no release is created. "Run workflow" on the Actions page runs the same tests without
releasing -- a quick way to test on systems you do not have.

## 5. The Tester: render audio and take GUI snapshots

`DrumFeatureDemonstrator_Tester` is a small console program built from your plugin code. It is not built
by default; switch it on when you configure:
```console
cmake -S . -B build -DAAT_BUILD_TESTER=ON
cmake --build build --target DrumFeatureDemonstrator_Tester
```
It creates the plugin the same way a DAW does (`createPluginFilter()`), so it needs no changes
for your plugin.

**Parameters:** `DrumFeatureDemonstrator_Tester --list` prints all parameter IDs with range and default.

**List of controls for the manual:** `DrumFeatureDemonstrator_Tester --manual md` (Markdown, e.g. for the
README) or `--manual tex` (a LaTeX table) prints all controls with range and default, as the plugin
shows them, and the `help` line of each parameter definition (see tools/ParameterSpec.h). So the
list in your manual comes from the code and stays correct when you change a parameter.

**Render** a wav file through the plugin (the result is a 32-bit float wav):
```console
DrumFeatureDemonstrator_Tester render in.wav out.wav --set ExampleID=1.5 --preset mypreset.xml
```
Uses: listen to a setting without a DAW; check that a code change did not change the sound
(render before and after, then compare the files -- identical files mean identical sound); feed
the result to an analysis (e.g. Python). Note that the output may be delayed by the plugin's
latency (the template's internal block processing delays by 2 ms).

**Snapshot** of the plugin window as a PNG:
```console
DrumFeatureDemonstrator_Tester snapshot gui.png --preset mypreset.xml --audio in.wav --seconds 2
```
With `--audio`, the file is played through the plugin first, so meters and displays show a
signal. Uses: screenshots for the README and the manual; a quick look at the GUI after a change
(also on a machine without a DAW).

Options for both: `--preset <file.xml>` (a preset saved by the preset handler), `--set <id>=<value>`
(repeatable, value in the parameter's own unit), `--blocksize <n>` (default 512).

Like in a DAW, the plugin uses your preset folder and settings. On Linux/macOS you can keep them
untouched with a temporary home folder: `HOME=$(mktemp -d) DrumFeatureDemonstrator_Tester ...`
