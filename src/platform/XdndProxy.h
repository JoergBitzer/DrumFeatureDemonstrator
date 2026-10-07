/**
 * @file XdndProxy.h
 * @brief lets a plug-in window that is embedded in a host window receive drag and drop of files (Linux/X11)
 *
 * Problem: a file manager (GTK, e.g. Thunar) offers a drop only to the top-level window under the mouse (the one with
 * WM_STATE). A host (e.g. Bitwig) creates that top-level window and puts the plug-in window inside it; the host window
 * does not announce "XdndAware", so the drag is refused and never reaches the plug-in (the symbol of the dragged file
 * does not change). The X11 drag-and-drop protocol has a remedy: the property XdndProxy on the top-level window names a
 * window that receives the drag messages instead. This sets XdndProxy of the host window to the plug-in window (and the
 * plug-in window points to itself, as the protocol requires). JUCE then handles the drop like in any window.
 *
 * Plain Xlib, no JUCE types (the X11 headers clash with JUCE's names).
 */
#pragma once

namespace dfd
{
// pluginWindow: the X11 window id of the plug-in editor window (ComponentPeer::getNativeHandle()).
// Returns the id of the host's top-level window that now points to the plug-in window, or 0 if nothing was changed (the plug-in
// window is a top-level window itself, as in the Standalone, no top-level window was found, or another proxy exists).
// Does nothing (returns 0) on other systems than Linux.
unsigned long installXdndProxy(unsigned long pluginWindow);

// takes the proxy away again (before the plug-in window is destroyed), only if it still points to pluginWindow
void removeXdndProxy(unsigned long topLevelWindow, unsigned long pluginWindow);
} // namespace dfd
