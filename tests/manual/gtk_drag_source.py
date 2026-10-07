#!/usr/bin/env python3
"""Manual test: a GTK3 window whose content can be dragged like a file from a file manager (uri list).
   python3 tests/manual/gtk_drag_source.py <file> [x y]     (window position, default 50 900)
Stays open for 40 seconds. Used together with tests/manual/xtest_drag.cpp to reproduce a real GTK drag onto a plug-in window."""
import sys
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, Gdk, GLib
try:
    gi.require_version("GdkX11", "3.0")
    from gi.repository import GdkX11  # for get_xid()
except (ValueError, ImportError):
    GdkX11 = None

path = sys.argv[1]
x = int(sys.argv[2]) if len(sys.argv) > 2 else 50
y = int(sys.argv[3]) if len(sys.argv) > 3 else 900

window = Gtk.Window(title="dnd-source-test")
window.set_default_size(260, 90)
window.move(x, y)
box = Gtk.EventBox()
label = Gtk.Label(label="drag me: " + path.split("/")[-1])
box.add(label)
window.add(box)
box.drag_source_set(Gdk.ModifierType.BUTTON1_MASK, [Gtk.TargetEntry.new("text/uri-list", 0, 0)], Gdk.DragAction.COPY)

def on_data_get(widget, context, data, info, time):
    data.set_uris(["file://" + path])
    print("drag-data-get: uri given", flush=True)

def watch_destination(context):
    # which window does GTK consider the drop target right now?
    try:
        destination = context.get_dest_window()
        xid = destination.get_xid() if destination is not None else None
        print("dest window:", hex(xid) if xid else None, flush=True)
    except Exception as error:  # old GTK without this call
        print("dest window: not available", error, flush=True)
        return False
    return True

def on_drag_begin(widget, context):
    print("drag-begin", flush=True)
    GLib.timeout_add(150, watch_destination, context)

def on_drag_end(widget, context):
    print("drag-end", flush=True)

def on_drag_failed(widget, context, result):
    print("drag-failed", result, flush=True)
    return False

box.connect("drag-data-get", on_data_get)
box.connect("drag-begin", on_drag_begin)
box.connect("drag-end", on_drag_end)
box.connect("drag-failed", on_drag_failed)
window.connect("destroy", Gtk.main_quit)
window.show_all()
GLib.timeout_add_seconds(40, Gtk.main_quit)
Gtk.main()
