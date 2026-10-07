/**
 * @file XdndTarget.h
 * @brief drop target for files dragged from a file manager onto a plug-in window that is embedded in a host window (Linux/X11)
 *
 * Problem: a file manager (GTK, e.g. Thunar) offers a drop only to the top-level window under the mouse (the one with WM_STATE).
 * A host (e.g. Bitwig) creates that top-level window and puts the plug-in window inside it; the host window does not announce
 * "XdndAware", so the drag is refused and never reaches the plug-in.
 *
 * Remedy (X11 drag-and-drop protocol, "XdndProxy"): this class creates its own small invisible window, announces it as the
 * proxy of the host's top-level window, and acts as the drop target itself: it answers the messages of the drag source, takes
 * the dropped list of files (text/uri-list) and hands the first one to a callback. JUCE's own drop code cannot be used for this:
 * the messages carry the host window as target ("window" field), which JUCE does not know (found out with a real GTK drag).
 *
 * Plain Xlib and std::thread, no JUCE types (the X11 headers clash with JUCE's names). The callback is called from the thread of
 * the target, not from the GUI thread: the receiver has to hand the file over to its own thread.
 * Does nothing on other systems than Linux, and nothing if the plug-in window is a top-level window itself (Standalone).
 */
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace dfd
{
class XdndDropTarget
{
public:
    // the dropped item as a text: a "file://" address (percent-encoded) or a path, as the file manager sends it
    using DropCallback = std::function<void(const std::string& droppedText)>;

    XdndDropTarget() = default;
    ~XdndDropTarget();
    XdndDropTarget(const XdndDropTarget&) = delete;
    XdndDropTarget& operator=(const XdndDropTarget&) = delete;

    // pluginWindow: the X11 window id of the plug-in editor window (ComponentPeer::getNativeHandle()).
    // Searches the host's top-level window in the background for up to about 20 seconds (the host may attach the plug-in later).
    void start(unsigned long pluginWindow, DropCallback callback);
    void stop();

    // true as soon as the proxy is in place (for diagnosis)
    bool isActive() const;

private:
    void run(unsigned long pluginWindow);

    std::thread m_thread;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_active{false};
    DropCallback m_callback;
};
} // namespace dfd
