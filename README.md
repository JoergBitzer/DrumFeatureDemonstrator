# DrumFeatureDemonstrator
A one-sample drum player (instrument plugin: VST3, AU on macOS, Standalone) with a 1/16 step grid and one new feature:
**play to end**. The plugin knows how long the sample sounds at the chosen pitch and reads the beat position from the DAW,
so it can start a (reversed) sample so that it ends exactly at the end of its step, e.g. a reverse cymbal that leads
into the next full beat.

Status: work packages W1-W7 of [planning.md](planning.md) are done: sample loading (button or drop), waveform with start/end handles, 8-32 step grid with mode/pitch/velocity per step, **play to end**, host sync or internal beat, presets with sample, manual. Progress notes: [DEVELOPMENT_LOG.md](DEVELOPMENT_LOG.md).

Made from the [AdvancedAudioTemplate](https://github.com/JoergBitzer/AdvancedAudioTemplate), branch AAT2.

## Build
```console
git submodule update --init                      # JUCE (pinned submodule), once
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DAAT_BUILD_TESTER=ON
cmake --build build --target DrumFeatureDemonstrator_VST3 DrumFeatureDemonstrator_Standalone DrumFeatureDemonstrator_Tester
```
Testing: see [HowToTestYourPlugin.md](HowToTestYourPlugin.md) (Debug build, `tools/run_pluginval.sh`, Tester).
Drum samples for tests: `tools/fetch_samples.sh` (MusicRadar SampleRadar, free and royalty-free). Example: `DrumFeatureDemonstrator_Tester render - out.wav --sample cymbal.wav --note 60,100,0.5 --seconds 4`.
Documentation rules: [docs/HowToDoc.md](docs/HowToDoc.md).

## Versioning
The version is set in `CMakeLists.txt` (`project(... VERSION X.Y.Z)`): a new feature raises the second number and sets the
third to zero, a fix or other change raises the third.

## License
`LICENSE`: MIT License for the source code. `LICENSE-AGPL-3.0.txt`: JUCE is used under the AGPLv3, so plugin binaries
are distributed under the AGPLv3 (see the note at the end of `LICENSE`).
