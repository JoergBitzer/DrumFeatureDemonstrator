#!/usr/bin/env bash
# with_virtual_display.sh -- runs a command in a virtual X display (Xvfb) with a small window manager (openbox), so that
# nothing appears on the desktop of the person who works on this machine (Linux).
#
# Usage:  tools/with_virtual_display.sh <command> [arguments...]
#   e.g.  tools/with_virtual_display.sh build/YourPluginName_Tester snapshot gui.png --seconds 1
#
# xvfb-run alone is enough for pluginval, but JUCE's top-level windows (the Tester's snapshot, the Standalone) need a window
# manager in the display, otherwise an X error ends the program ("BadAtom"). openbox is used if it is installed.
set -u
if [ $# -lt 1 ]; then
    echo "Usage: $0 <command> [arguments...]"
    exit 2
fi
if ! command -v xvfb-run > /dev/null; then
    echo "xvfb-run not found (sudo apt install xvfb): not starting a GUI program on the real desktop"
    exit 3
fi
exec xvfb-run -a sh -c 'if command -v openbox > /dev/null; then openbox > /dev/null 2>&1 & sleep 1; fi; exec "$@"' sh "$@"
