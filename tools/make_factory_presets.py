#!/usr/bin/env python3
"""Writes the factory presets (res/presets/*.xml) of the DrumFeatureDemonstrator.

Run it from the project folder after changing a pattern:  python3 tools/make_factory_presets.py
Raise PRESET_VERSION when a preset changes (installed unmodified factory presets with a lower version are replaced).
The presets contain no sample (the samples are not part of the repository): they keep the sample you have loaded.
"""
import os

PRESET_VERSION = 4 # 2: step note and velocity 1 - 127; 3: fade in / fade out; 4: four patterns, pan
BASE_NOTE = 60
NUM_STEPS = 32
NUM_PATTERNS = 4
FORWARD, REVERSE, REVERSE_TO_END = 0, 1, 2

SINGLE_DEFAULTS = {
    "BaseNote": 60.0, "Declick": 0.0, "FadeIn": 0.0, "FadeOut": 0.0, "InternalBpm": 120.0, "InternalPlay": 0.0, "PatternLength": 16.0,
    "RegionEnd": 1.0, "RegionStart": 0.0, "SyncToHost": 1.0,
}

# name: (pattern length, [pattern 1, pattern 2, ...], patterns that play)
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
    "Four patterns": (8, [{1: (FORWARD, 0, 1.0, 0), 5: (FORWARD, 0, 0.8, 0)},
                          {1: (FORWARD, 0, 1.0, 0), 3: (FORWARD, 5, 0.6, -0.6), 5: (FORWARD, 0, 0.8, 0), 7: (FORWARD, 5, 0.6, 0.6)},
                          {1: (FORWARD, 0, 1.0, 0), 5: (FORWARD, 7, 0.8, 0), 8: (REVERSE_TO_END, 0, 0.8, 0)},
                          {1: (FORWARD, 12, 0.9, 0), 3: (FORWARD, 7, 0.7, 0.3), 5: (FORWARD, 0, 0.9, 0), 6: (FORWARD, -5, 0.7, -0.3),
                           8: (REVERSE_TO_END, 3, 0.8, 0)}], [1, 2, 3, 4]),
}


def step_prefix(pattern_index):
    if pattern_index == 0:
        return "Step"
    return "P%dStep" % (pattern_index + 1)


def preset_xml(name, length, patterns, playing):
    values = dict(SINGLE_DEFAULTS)
    values["PatternLength"] = float(length)
    lines = []
    for key in sorted(values):
        lines.append('  <PARAM id="%s" value="%.1f"/>' % (key, values[key]))
    for pattern_number in range(1, NUM_PATTERNS + 1):
        lines.append('  <PARAM id="PlayPattern%d" value="%.1f"/>' % (pattern_number, 1.0 if pattern_number in playing else 0.0))
    lines.append('  <PARAM id="EditPattern" value="1.0"/>')
    for pattern_index in range(NUM_PATTERNS):
        steps = patterns[pattern_index] if pattern_index < len(patterns) else {}
        for step in range(1, NUM_STEPS + 1):
            mode, pitch, velocity, pan = steps.get(step, (FORWARD, 0, 1.0, 0))
            velocity = max(1, round(velocity * 127))
            active = 1.0 if step in steps else 0.0
            prefix = "%s%02d" % (step_prefix(pattern_index), step)
            lines.append('  <PARAM id="%sActive" value="%.1f"/>' % (prefix, active))
            lines.append('  <PARAM id="%sMode" value="%.1f"/>' % (prefix, mode))
            lines.append('  <PARAM id="%sNote" value="%.1f"/>' % (prefix, BASE_NOTE + pitch))
            lines.append('  <PARAM id="%sVelocity" value="%.1f"/>' % (prefix, velocity))
            lines.append('  <PARAM id="%sPan" value="%.2f"/>' % (prefix, pan))
    header = '<DrumFeatureDemonstratorVTS presetname="%s" category="Unknown" bank="Factory" presetversion="%d">' % (name, PRESET_VERSION)
    return '<?xml version="1.0" encoding="UTF-8"?>\n' + header + "\n" + "\n".join(lines) + "\n</DrumFeatureDemonstratorVTS>\n"


def main():
    folder = os.path.join("res", "presets")
    os.makedirs(folder, exist_ok=True)
    for name, (length, patterns, playing) in PRESETS.items():
        file_name = name.replace(" ", "_") + ".xml"
        with open(os.path.join(folder, file_name), "w") as handle:
            handle.write(preset_xml(name, length, patterns, playing))
        print("wrote", file_name)


if __name__ == "__main__":
    main()
