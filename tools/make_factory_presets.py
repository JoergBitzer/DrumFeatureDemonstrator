#!/usr/bin/env python3
"""Writes the factory presets (res/presets/*.xml) of the DrumFeatureDemonstrator.

Run it from the project folder after changing a pattern:  python3 tools/make_factory_presets.py
Raise PRESET_VERSION when a preset changes (installed unmodified factory presets with a lower version are replaced).
The presets contain no sample (the samples are not part of the repository): they keep the sample you have loaded.
"""
import base64
import os
import struct

PRESET_VERSION = 7 # 2: step note and velocity 1 - 127; 3: fade in / fade out; 4: four patterns, pan; 5: note value of a step; 6: patterns as one blob (PatternData); 7: blob version 2 with the pattern settings
BASE_NOTE = 60
NUM_STEPS = 32
NUM_PATTERNS = 4
FORWARD, REVERSE, REVERSE_TO_END = 0, 1, 2

SINGLE_DEFAULTS = {
    "BaseNote": 60.0, "FadeIn": 0.0, "FadeOut": 0.0, "InternalBpm": 120.0, "RegionEnd": 1.0, "RegionStart": 0.0,
}
SYNC_TO_HOST = 1.0
DECLICK = 0.0

# name: (pattern length, [pattern 1, pattern 2, ...], patterns that play[, note value index])
# note value index: base * 3 + flavour; base 0 = 1/1, 1 = 1/2, 2 = 1/4, 3 = 1/8, 4 = 1/16 (the default, 12), 5 = 1/32, 6 = 1/64; flavour 0 normal, 1 dotted, 2 triplet
# a pattern: {step number (1-based): (mode, semitones relative to the base note, velocity 0 - 1 (written as 1 - 127), pan -1 .. 1)}
PRESETS = {
    "Four on the floor": (16, [{1: (FORWARD, 0, 1.0, 0), 5: (FORWARD, 0, 0.9, 0), 9: (FORWARD, 0, 1.0, 0), 13: (FORWARD, 0, 0.9, 0)}], [1]),
    "Reverse into the beat": (16, [{1: (FORWARD, 0, 1.0, 0), 9: (FORWARD, 0, 1.0, 0),
                                    8: (REVERSE_TO_END, 0, 0.8, 0), 16: (REVERSE_TO_END, 0, 0.8, 0)}], [1]),
    "Offbeat riff": (16, [{1: (FORWARD, 0, 1.0, 0), 4: (FORWARD, 7, 0.6, 0), 7: (REVERSE, -5, 0.7, 0), 11: (FORWARD, 12, 0.5, 0),
                           14: (REVERSE_TO_END, 3, 0.8, 0)}], [1]),
    "Short loop of 8": (8, [{1: (FORWARD, 0, 1.0, 0), 4: (FORWARD, 5, 0.7, 0), 8: (REVERSE_TO_END, 0, 0.8, 0)}], [1]),
    "Two patterns": (16, [{1: (FORWARD, 0, 1.0, 0), 5: (FORWARD, 0, 0.8, 0), 9: (FORWARD, 0, 1.0, 0), 13: (FORWARD, 0, 0.8, 0)},
                          {1: (FORWARD, 0, 1.0, 0), 4: (FORWARD, 7, 0.7, -0.5), 7: (FORWARD, 7, 0.7, 0.5), 9: (FORWARD, 0, 1.0, 0),
                           12: (REVERSE, -5, 0.8, 0), 16: (REVERSE_TO_END, 0, 0.9, 0)}], [1, 2]),
    "Triplets": (12, [{1: (FORWARD, 0, 1.0, 0), 4: (FORWARD, 0, 0.8, 0), 7: (FORWARD, 0, 1.0, 0), 10: (FORWARD, 0, 0.8, 0),
                      3: (FORWARD, 7, 0.5, 0.4), 6: (FORWARD, 7, 0.5, -0.4), 12: (REVERSE_TO_END, 0, 0.8, 0)}], [1], 11),  # 1/8 triplets
    "Four patterns": (8, [{1: (FORWARD, 0, 1.0, 0), 5: (FORWARD, 0, 0.8, 0)},
                          {1: (FORWARD, 0, 1.0, 0), 3: (FORWARD, 5, 0.6, -0.6), 5: (FORWARD, 0, 0.8, 0), 7: (FORWARD, 5, 0.6, 0.6)},
                          {1: (FORWARD, 0, 1.0, 0), 5: (FORWARD, 7, 0.8, 0), 8: (REVERSE_TO_END, 0, 0.8, 0)},
                          {1: (FORWARD, 12, 0.9, 0), 3: (FORWARD, 7, 0.7, 0.3), 5: (FORWARD, 0, 0.9, 0), 6: (FORWARD, -5, 0.7, -0.3),
                           8: (REVERSE_TO_END, 3, 0.8, 0)}], [1, 2, 3, 4]),
}


def pattern_blob(patterns, playing, length, note_value):
    """The PatternData blob (format version 2, see src/PatternData.h): header, play mask, the settings (length, note value,
    sync to host, declick), then per pattern and step the floats active, mode, note, velocity, pan (little endian)."""
    play_mask = 0
    for pattern_number in playing:
        play_mask |= 1 << (pattern_number - 1)
    blob = b"DFDP" + bytes([2, NUM_PATTERNS, NUM_STEPS, play_mask])
    blob += struct.pack("<4f", float(length), float(note_value), SYNC_TO_HOST, DECLICK)
    for pattern_index in range(NUM_PATTERNS):
        steps = patterns[pattern_index] if pattern_index < len(patterns) else {}
        for step in range(1, NUM_STEPS + 1):
            mode, pitch, velocity, pan = steps.get(step, (FORWARD, 0, 1.0, 0))
            velocity = max(1, round(velocity * 127))
            active = 1.0 if step in steps else 0.0
            blob += struct.pack("<5f", active, float(mode), float(BASE_NOTE + pitch), float(velocity), float(pan))
    return blob


def preset_xml(name, length, patterns, playing, note_value=12):
    values = dict(SINGLE_DEFAULTS)
    lines = []
    for key in sorted(values):
        lines.append('  <PARAM id="%s" value="%.1f"/>' % (key, values[key]))
    blob_text = base64.b64encode(pattern_blob(patterns, playing, length, note_value)).decode("ascii")
    header = ('<DrumFeatureDemonstratorVTS presetname="%s" category="Unknown" bank="Factory" presetversion="%d" PatternData="%s">'
              % (name, PRESET_VERSION, blob_text))
    return '<?xml version="1.0" encoding="UTF-8"?>\n' + header + "\n" + "\n".join(lines) + "\n</DrumFeatureDemonstratorVTS>\n"


def main():
    folder = os.path.join("res", "presets")
    os.makedirs(folder, exist_ok=True)
    for name, entry in PRESETS.items():
        length, patterns, playing = entry[0], entry[1], entry[2]
        note_value = entry[3] if len(entry) > 3 else 12
        file_name = name.replace(" ", "_") + ".xml"
        with open(os.path.join(folder, file_name), "w") as handle:
            handle.write(preset_xml(name, length, patterns, playing, note_value))
        print("wrote", file_name)


if __name__ == "__main__":
    main()
