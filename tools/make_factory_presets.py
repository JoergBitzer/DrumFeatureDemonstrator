#!/usr/bin/env python3
"""Writes the factory presets (res/presets/*.xml) of the DrumFeatureDemonstrator.

Run it from the project folder after changing a pattern:  python3 tools/make_factory_presets.py
Raise PRESET_VERSION when a preset changes (installed unmodified factory presets with a lower version are replaced).
The presets contain no sample (the samples are not part of the repository): they keep the sample you have loaded.
"""
import os

PRESET_VERSION = 1
NUM_STEPS = 32
FORWARD, REVERSE, REVERSE_TO_END = 0, 1, 2

SINGLE_DEFAULTS = {
    "BaseNote": 60.0, "Declick": 0.0, "InternalBpm": 120.0, "InternalPlay": 0.0, "PatternLength": 16.0,
    "RegionEnd": 1.0, "RegionStart": 0.0, "SyncToHost": 1.0,
}

# name: (pattern length, {step number (1-based): (mode, pitch in semitones, velocity)})
PRESETS = {
    "Four on the floor": (16, {1: (FORWARD, 0, 1.0), 5: (FORWARD, 0, 0.9), 9: (FORWARD, 0, 1.0), 13: (FORWARD, 0, 0.9)}),
    "Reverse into the beat": (16, {1: (FORWARD, 0, 1.0), 9: (FORWARD, 0, 1.0),
                                    8: (REVERSE_TO_END, 0, 0.8), 16: (REVERSE_TO_END, 0, 0.8)}),
    "Offbeat riff": (16, {1: (FORWARD, 0, 1.0), 4: (FORWARD, 7, 0.6), 7: (REVERSE, -5, 0.7), 11: (FORWARD, 12, 0.5),
                          14: (REVERSE_TO_END, 3, 0.8)}),
    "Short loop of 8": (8, {1: (FORWARD, 0, 1.0), 4: (FORWARD, 5, 0.7), 8: (REVERSE_TO_END, 0, 0.8)}),
}


def preset_xml(name, length, steps):
    values = dict(SINGLE_DEFAULTS)
    values["PatternLength"] = float(length)
    lines = []
    for key in sorted(values):
        lines.append('  <PARAM id="%s" value="%.1f"/>' % (key, values[key]))
    for step in range(1, NUM_STEPS + 1):
        mode, pitch, velocity = steps.get(step, (FORWARD, 0, 1.0))
        active = 1.0 if step in steps else 0.0
        prefix = "Step%02d" % step
        lines.append('  <PARAM id="%sActive" value="%.1f"/>' % (prefix, active))
        lines.append('  <PARAM id="%sMode" value="%.1f"/>' % (prefix, mode))
        lines.append('  <PARAM id="%sPitch" value="%.1f"/>' % (prefix, pitch))
        lines.append('  <PARAM id="%sVelocity" value="%.2f"/>' % (prefix, velocity))
    header = '<DrumFeatureDemonstratorVTS presetname="%s" category="Unknown" bank="Factory" presetversion="%d">' % (name, PRESET_VERSION)
    return '<?xml version="1.0" encoding="UTF-8"?>\n' + header + "\n" + "\n".join(lines) + "\n</DrumFeatureDemonstratorVTS>\n"


def main():
    folder = os.path.join("res", "presets")
    os.makedirs(folder, exist_ok=True)
    for name, (length, steps) in PRESETS.items():
        file_name = name.replace(" ", "_") + ".xml"
        with open(os.path.join(folder, file_name), "w") as handle:
            handle.write(preset_xml(name, length, steps))
        print("wrote", file_name)


if __name__ == "__main__":
    main()
