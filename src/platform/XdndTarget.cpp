#include "XdndTarget.h"

#if defined(__linux__)

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <sys/select.h>

#include <chrono>
#include <cstring>
#include <vector>

namespace dfd
{
namespace
{
constexpr int kXdndVersion = 3;           // like JUCE: all common file managers speak at least this
constexpr int kFormatWindow = 32;
constexpr int kFormatAtom = 32;
constexpr long kMaxPropertyLongs = 65536;
constexpr int kSearchAttempts = 20;       // looking for the host's top-level window, one attempt per second
constexpr int kPollMilliseconds = 100;
constexpr int kSelectionTimeoutMilliseconds = 3000;
constexpr long kAcceptFlag = 1;
constexpr long kMoreThanThreeTypesFlag = 1;
constexpr int kVersionShift = 24;
constexpr int kFirstTypeIndex = 2;
constexpr int kNumTypesInMessage = 3;

int ignoreXErrors(Display*, XErrorEvent*)
{
    return 0;
}

// a failed request (a window that is already gone) must not end the process: if nobody installed an error handler, install one
void makeErrorsNonFatal()
{
    const XErrorHandler current = XSetErrorHandler(nullptr);          // installs Xlib's default and returns the one in use
    const XErrorHandler defaultHandler = XSetErrorHandler(current);    // puts the one in use back and returns the default
    if (current == defaultHandler)
    {
        XSetErrorHandler(ignoreXErrors);
    }
}

bool hasProperty(Display* display, Window window, Atom property)
{
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long numItems = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    XGetWindowProperty(display, window, property, 0, 1, False, AnyPropertyType, &actualType, &actualFormat, &numItems, &bytesAfter, &data);
    if (data != nullptr)
    {
        XFree(data);
    }
    return actualType != None;
}

Window readWindowProperty(Display* display, Window window, Atom property)
{
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long numItems = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    Window result = None;
    XGetWindowProperty(display, window, property, 0, 1, False, XA_WINDOW, &actualType, &actualFormat, &numItems, &bytesAfter, &data);
    if (data != nullptr && actualType == XA_WINDOW && numItems > 0)
    {
        result = *reinterpret_cast<Window*>(data);
    }
    if (data != nullptr)
    {
        XFree(data);
    }
    return result;
}

void setWindowProperty(Display* display, Window window, Atom property, Window value)
{
    XChangeProperty(display, window, property, XA_WINDOW, kFormatWindow, PropModeReplace, reinterpret_cast<unsigned char*>(&value), 1);
}

// the window that a file manager treats as the target: the nearest ancestor with WM_STATE; if there is none, the highest ancestor below the root
Window findTopLevelClientWindow(Display* display, Window start, Atom wmState)
{
    Window current = start;
    Window highestBelowRoot = start;
    for (;;)
    {
        Window root = None;
        Window parent = None;
        Window* children = nullptr;
        unsigned int numChildren = 0;
        if (XQueryTree(display, current, &root, &parent, &children, &numChildren) == 0)
        {
            break;
        }
        if (children != nullptr)
        {
            XFree(children);
        }
        if (current != start && hasProperty(display, current, wmState))
        {
            return current;
        }
        if (parent == None || parent == root)
        {
            break;
        }
        highestBelowRoot = parent;
        current = parent;
    }
    return highestBelowRoot;
}

std::vector<Atom> readAtomList(Display* display, Window window, Atom property)
{
    std::vector<Atom> atoms;
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long numItems = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    XGetWindowProperty(display, window, property, 0, kMaxPropertyLongs, False, XA_ATOM, &actualType, &actualFormat, &numItems, &bytesAfter, &data);
    if (data != nullptr && actualType == XA_ATOM && actualFormat == kFormatAtom)
    {
        const Atom* list = reinterpret_cast<const Atom*>(data);
        for (unsigned long index = 0; index < numItems; ++index)
        {
            atoms.push_back(list[index]);
        }
    }
    if (data != nullptr)
    {
        XFree(data);
    }
    return atoms;
}

// the first "file://" line (or, if there is none, the first line) of a text/uri-list
std::string firstEntryOfUriList(const std::string& text)
{
    std::string firstLine;
    std::size_t position = 0;
    while (position < text.size())
    {
        std::size_t end = text.find_first_of("\r\n", position);
        if (end == std::string::npos)
        {
            end = text.size();
        }
        const std::string line = text.substr(position, end - position);
        position = end + 1;
        if (line.empty() || line[0] == '#')
        {
            continue;
        }
        if (line.rfind("file://", 0) == 0)
        {
            return line;
        }
        if (firstLine.empty())
        {
            firstLine = line;
        }
    }
    return firstLine;
}

struct Atoms
{
    Atom proxy, aware, typeList, enter, position, status, leave, drop, finished, selection, actionCopy, uriList, dropProperty, wmState;
};

Atoms makeAtoms(Display* display)
{
    Atoms atoms;
    atoms.proxy = XInternAtom(display, "XdndProxy", False);
    atoms.aware = XInternAtom(display, "XdndAware", False);
    atoms.typeList = XInternAtom(display, "XdndTypeList", False);
    atoms.enter = XInternAtom(display, "XdndEnter", False);
    atoms.position = XInternAtom(display, "XdndPosition", False);
    atoms.status = XInternAtom(display, "XdndStatus", False);
    atoms.leave = XInternAtom(display, "XdndLeave", False);
    atoms.drop = XInternAtom(display, "XdndDrop", False);
    atoms.finished = XInternAtom(display, "XdndFinished", False);
    atoms.selection = XInternAtom(display, "XdndSelection", False);
    atoms.actionCopy = XInternAtom(display, "XdndActionCopy", False);
    atoms.uriList = XInternAtom(display, "text/uri-list", False);
    atoms.dropProperty = XInternAtom(display, "DFD_XDND_DROP", False);
    atoms.wmState = XInternAtom(display, "WM_STATE", False);
    return atoms;
}

void sendReply(Display* display, Window source, const Atoms& atoms, Atom messageType, Window targetWindow, long flags, long action)
{
    XEvent event;
    std::memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.display = display;
    event.xclient.window = source;
    event.xclient.message_type = messageType;
    event.xclient.format = 32;
    // the answer names the window the message was addressed to (the host's top-level window), not our proxy window
    event.xclient.data.l[0] = static_cast<long>(targetWindow);
    event.xclient.data.l[1] = flags;
    if (messageType == atoms.status)
    {
        event.xclient.data.l[4] = action;
    }
    else
    {
        event.xclient.data.l[2] = action;
    }
    XSendEvent(display, source, False, NoEventMask, &event);
    XFlush(display);
}

// state of one drag
struct DragState
{
    Window source = None;
    Window targetWindow = None;
    bool offersFiles = false;
    bool waitingForData = false;
    std::chrono::steady_clock::time_point dataRequestedAt;
};

void handleEnter(Display* display, const XClientMessageEvent& message, const Atoms& atoms, DragState& drag)
{
    drag = DragState();
    drag.source = static_cast<Window>(message.data.l[0]);
    drag.targetWindow = message.window;
    if ((message.data.l[1] & kMoreThanThreeTypesFlag) != 0)
    {
        for (Atom type : readAtomList(display, drag.source, atoms.typeList))
        {
            if (type == atoms.uriList)
            {
                drag.offersFiles = true;
            }
        }
    }
    else
    {
        for (int index = 0; index < kNumTypesInMessage; ++index)
        {
            if (static_cast<Atom>(message.data.l[kFirstTypeIndex + index]) == atoms.uriList)
            {
                drag.offersFiles = true;
            }
        }
    }
}
} // namespace

XdndDropTarget::~XdndDropTarget()
{
    stop();
}

void XdndDropTarget::start(unsigned long pluginWindow, DropCallback callback)
{
    stop();
    m_callback = std::move(callback);
    m_stopRequested = false;
    m_thread = std::thread([this, pluginWindow]() { run(pluginWindow); });
}

void XdndDropTarget::stop()
{
    m_stopRequested = true;
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    m_active = false;
}

bool XdndDropTarget::isActive() const
{
    return m_active.load();
}

void XdndDropTarget::run(unsigned long pluginWindowId)
{
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        return;
    }
    makeErrorsNonFatal();
    const Atoms atoms = makeAtoms(display);
    const Window pluginWindow = static_cast<Window>(pluginWindowId);

    // a top-level window (Standalone) receives drops itself
    if (hasProperty(display, pluginWindow, atoms.wmState))
    {
        XCloseDisplay(display);
        return;
    }

    // the host attaches the plug-in window to its top-level window; wait for it
    Window topLevel = None;
    for (int attempt = 0; attempt < kSearchAttempts && m_stopRequested == false && topLevel == None; ++attempt)
    {
        const Window candidate = findTopLevelClientWindow(display, pluginWindow, atoms.wmState);
        if (candidate != pluginWindow && candidate != None && hasProperty(display, candidate, atoms.wmState))
        {
            topLevel = candidate;
        }
        else
        {
            for (int waited = 0; waited < 1000 && m_stopRequested == false; waited += kPollMilliseconds)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(kPollMilliseconds));
            }
        }
    }
    const Window existingProxy = (topLevel != None) ? readWindowProperty(display, topLevel, atoms.proxy) : None;
    if (topLevel == None || existingProxy != None)
    {
        XCloseDisplay(display);
        return;
    }

    // our own invisible window is the proxy and the drop target
    const Window proxyWindow = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0, 0, 0);
    const long version = kXdndVersion;
    XChangeProperty(display, proxyWindow, atoms.aware, XA_ATOM, kFormatAtom, PropModeReplace, reinterpret_cast<const unsigned char*>(&version), 1);
    setWindowProperty(display, proxyWindow, atoms.proxy, proxyWindow);
    setWindowProperty(display, topLevel, atoms.proxy, proxyWindow);
    XSync(display, False);
    m_active = true;

    DragState drag;
    const int connection = ConnectionNumber(display);
    while (m_stopRequested == false)
    {
        if (XPending(display) == 0)
        {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(connection, &readSet);
            timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = kPollMilliseconds * 1000;
            select(connection + 1, &readSet, nullptr, nullptr, &timeout);
        }
        if (drag.waitingForData
            && std::chrono::steady_clock::now() - drag.dataRequestedAt > std::chrono::milliseconds(kSelectionTimeoutMilliseconds))
        {
            sendReply(display, drag.source, atoms, atoms.finished, drag.targetWindow, 0, 0);
            drag = DragState();
        }
        while (XPending(display) > 0)
        {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == ClientMessage)
            {
                const XClientMessageEvent& message = event.xclient;
                if (message.message_type == atoms.enter)
                {
                    handleEnter(display, message, atoms, drag);
                }
                else if (message.message_type == atoms.position)
                {
                    long flags = 0;
                    if (drag.offersFiles)
                    {
                        flags = kAcceptFlag;
                    }
                    sendReply(display, drag.source, atoms, atoms.status, drag.targetWindow, flags, static_cast<long>(atoms.actionCopy));
                }
                else if (message.message_type == atoms.leave)
                {
                    drag = DragState();
                }
                else if (message.message_type == atoms.drop)
                {
                    if (drag.offersFiles)
                    {
                        XConvertSelection(display, atoms.selection, atoms.uriList, atoms.dropProperty, proxyWindow, static_cast<Time>(message.data.l[2]));
                        XFlush(display);
                        drag.waitingForData = true;
                        drag.dataRequestedAt = std::chrono::steady_clock::now();
                    }
                    else
                    {
                        sendReply(display, drag.source, atoms, atoms.finished, drag.targetWindow, 0, 0);
                        drag = DragState();
                    }
                }
            }
            else if (event.type == SelectionNotify && drag.waitingForData)
            {
                bool gotFile = false;
                if (event.xselection.property != None)
                {
                    Atom type = None;
                    int format = 0;
                    unsigned long numItems = 0;
                    unsigned long bytesAfter = 0;
                    unsigned char* data = nullptr;
                    XGetWindowProperty(display, proxyWindow, atoms.dropProperty, 0, kMaxPropertyLongs, True, AnyPropertyType, &type, &format, &numItems, &bytesAfter, &data);
                    if (data != nullptr)
                    {
                        const std::string text(reinterpret_cast<const char*>(data), numItems);
                        XFree(data);
                        const std::string entry = firstEntryOfUriList(text);
                        if (entry.empty() == false && m_callback)
                        {
                            gotFile = true;
                            m_callback(entry);
                        }
                    }
                }
                long accepted = 0;
                if (gotFile)
                {
                    accepted = kAcceptFlag;
                }
                sendReply(display, drag.source, atoms, atoms.finished, drag.targetWindow, accepted, static_cast<long>(atoms.actionCopy));
                drag = DragState();
            }
        }
    }

    // take the proxy away again, if it is still ours (the host window may be gone already)
    if (readWindowProperty(display, topLevel, atoms.proxy) == proxyWindow)
    {
        XDeleteProperty(display, topLevel, atoms.proxy);
    }
    XDestroyWindow(display, proxyWindow);
    XSync(display, False);
    XCloseDisplay(display);
    m_active = false;
}
} // namespace dfd

#else

namespace dfd
{
XdndDropTarget::~XdndDropTarget() {}
void XdndDropTarget::start(unsigned long, DropCallback) {}
void XdndDropTarget::stop() {}
bool XdndDropTarget::isActive() const { return false; }
void XdndDropTarget::run(unsigned long) {}
} // namespace dfd

#endif
