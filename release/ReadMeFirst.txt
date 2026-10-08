# DrumFeatureDemonstrator: ReadMe

## Check, if everything is OK
Since you read this file, you unzipped the downloaded file.

The directory should contain:
1. This ReadMeFirst.txt file (in MarkDown format, you can rename it in ReadMeFirst.md for a better view)
2. ManualDrumFeatureDemonstrator.pdf, the manual
3. LICENSE and LICENSE-AGPL-3.0.txt, the licenses
4. a directory called DrumFeatureDemonstrator.vst3 (the VST3 plugin)
5. the Standalone application (Windows: DrumFeatureDemonstrator.exe, macOS: DrumFeatureDemonstrator.app, Linux: DrumFeatureDemonstrator)
6. (macOS only) DrumFeatureDemonstrator.component (the AU plugin)

## Installation

### Windows
Copy the directory DrumFeatureDemonstrator.vst3 to C:\Program Files\Common Files\VST3
(Cubase: Studio > VST Plug-in Manager > Update, or restart Cubase; then add an Instrument Track
and choose DrumFeatureDemonstrator as the instrument.)

### Mac VST3
Copy the directory DrumFeatureDemonstrator.vst3 to
/Users/yourUSERNAME/Library/Audio/Plug-Ins/VST3

### Mac AU
Copy DrumFeatureDemonstrator.component to
/Users/yourUSERNAME/Library/Audio/Plug-Ins/Components

The macOS version is not signed with an Apple Developer ID yet. If macOS reports that the
plugin is damaged or cannot be opened, remove the quarantine flag in the Terminal, e.g.
xattr -cr ~/Library/Audio/Plug-Ins/VST3/DrumFeatureDemonstrator.vst3

### Linux
Copy the directory DrumFeatureDemonstrator.vst3 to
/home/yourUSERNAME/.vst3/

Done! Start your DAW and let it rescan the plugins.
The Standalone application runs without a DAW: use the on-screen keyboard, or switch "Host sync" off
and press Play (or the space key) for the internal beat.

## First steps
1. Load a sample ("Load sample", or drop a file onto the plugin window).
2. Click some steps in the grid. The button below a step switches its mode: forward, reverse,
   reverse + play to end. In the last mode the sample ends exactly at the end of that step: put it on the
   last step before a beat to lead into the beat with a reversed cymbal.
3. "Host sync" on: the plugin plays when your DAW plays. Off: own tempo and Play button.
4. Cursor keys or a click select a step (yellow frame) while nothing plays; MIDI notes then set its note
   and velocity.

At the first start, the factory presets are copied to your preset folder (plain XML files). Your loaded
samples are copied into the folder "Samples" there; a DAW project contains its sample itself:
- Windows: C:\Users\yourUSERNAME\AppData\Roaming\Jade_Hochschule\DrumFeatureDemonstrator
- macOS:   /Users/yourUSERNAME/Library/Audio/Presets/Jade_Hochschule/DrumFeatureDemonstrator
- Linux:   /home/yourUSERNAME/.config/Jade_Hochschule/DrumFeatureDemonstrator

Have fun and read the manual for instructions and further information.

J. Bitzer aka audio-dsp


# Source code and license

## Download source code
You can download the source code at GitHub:

https://github.com/JoergBitzer/DrumFeatureDemonstrator

## License
- The source code is open source under the MIT License (see LICENSE),
  (c) Joerg Bitzer, Jade Hochschule.
- The plugin binaries contain third-party code: JUCE (used under the AGPLv3), the VST3 SDK
  by Steinberg (MIT License) and, on macOS, the Audio Unit SDK by Apple (Apache License 2.0).
  Therefore the binaries as a whole are distributed under the GNU Affero General Public
  License v3 (see LICENSE-AGPL-3.0.txt). The complete source code is the repository above
  plus JUCE.
- The manual is licensed under CC-BY 4.0.

VST is a registered trademark of Steinberg Media Technologies GmbH.

The plugin comes without any warranty (see the licenses).
