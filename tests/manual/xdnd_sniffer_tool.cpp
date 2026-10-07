// Manual test tool (not built by CMake): becomes the XdndProxy of a top-level window for a few seconds and prints every XDND message
// a drag source sends to it, answers like a real drop target (XdndStatus accept, takes the dropped file list, XdndFinished) and prints
// the dropped files. It is the prototype of src/platform/XdndTarget.* (the plug-in's own drop target).
//   g++ -std=c++17 tests/manual/xdnd_sniffer_tool.cpp -o xdnd_sniffer_tool -lX11
//   ./xdnd_sniffer_tool <top-level window id> [seconds]        (the old XdndProxy of the window is restored at the end)
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

namespace
{
Display* g_display = nullptr;

Atom atom(const char* name)
{
    return XInternAtom(g_display, name, False);
}

std::string atomName(Atom a)
{
    if (a == None)
    {
        return "None";
    }
    char* name = XGetAtomName(g_display, a);
    std::string text = (name != nullptr) ? name : "?";
    if (name != nullptr)
    {
        XFree(name);
    }
    return text;
}

Window readProxy(Window window)
{
    Atom type = None;
    int format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    Window result = None;
    XGetWindowProperty(g_display, window, atom("XdndProxy"), 0, 1, False, XA_WINDOW, &type, &format, &items, &after, &data);
    if (data != nullptr && type == XA_WINDOW && items > 0)
    {
        result = *reinterpret_cast<Window*>(data);
    }
    if (data != nullptr)
    {
        XFree(data);
    }
    return result;
}

void setWindow(Window window, const char* name, Window value)
{
    XChangeProperty(g_display, window, atom(name), XA_WINDOW, 32, PropModeReplace, reinterpret_cast<unsigned char*>(&value), 1);
}
} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        return 2;
    }
    g_display = XOpenDisplay(nullptr);
    if (g_display == nullptr)
    {
        return 2;
    }
    const Window topLevel = std::strtoul(argv[1], nullptr, 0);
    const int seconds = (argc > 2) ? std::atoi(argv[2]) : 20;
    const Window old = readProxy(topLevel);

    const Window sniffer = XCreateSimpleWindow(g_display, DefaultRootWindow(g_display), 0, 0, 1, 1, 0, 0, 0);
    const long version = 3; // like JUCE
    XChangeProperty(g_display, sniffer, atom("XdndAware"), XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&version), 1);
    setWindow(sniffer, "XdndProxy", sniffer);
    setWindow(topLevel, "XdndProxy", sniffer);
    XSync(g_display, False);
    std::cout << "sniffer 0x" << std::hex << sniffer << " is now the XdndProxy of 0x" << topLevel << " (old: 0x" << old << ")" << std::dec << "\n";

    Window dragSource = None;
    Window targetWindow = None; // the window field of the messages: the real target; replies must name it, not the proxy
    Atom chosenType = None;
    const Atom uriList = atom("text/uri-list");
    const Atom selectionName = atom("XdndSelection");
    const Atom dropProperty = atom("DFD_DROP");
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(seconds))
    {
        while (XPending(g_display) > 0)
        {
            XEvent event;
            XNextEvent(g_display, &event);
            if (event.type == SelectionNotify)
            {
                std::cout << "SelectionNotify, property " << atomName(event.xselection.property) << "\n";
                if (event.xselection.property != None)
                {
                    Atom type = None;
                    int format = 0;
                    unsigned long items = 0;
                    unsigned long after = 0;
                    unsigned char* data = nullptr;
                    XGetWindowProperty(g_display, sniffer, dropProperty, 0, 65536, True, AnyPropertyType, &type, &format, &items, &after, &data);
                    if (data != nullptr)
                    {
                        std::cout << "DROPPED DATA (" << atomName(type) << "): " << std::string(reinterpret_cast<char*>(data), items) << "\n";
                        XFree(data);
                    }
                }
                XEvent finished;
                std::memset(&finished, 0, sizeof(finished));
                finished.xclient.type = ClientMessage;
                finished.xclient.display = g_display;
                finished.xclient.window = dragSource;
                finished.xclient.message_type = atom("XdndFinished");
                finished.xclient.format = 32;
                finished.xclient.data.l[0] = static_cast<long>(targetWindow);
                finished.xclient.data.l[1] = (event.xselection.property != None) ? 1 : 0;
                finished.xclient.data.l[2] = static_cast<long>(atom("XdndActionCopy"));
                XSendEvent(g_display, dragSource, False, NoEventMask, &finished);
                XFlush(g_display);
                continue;
            }
            if (event.type != ClientMessage)
            {
                continue;
            }
            const std::string type = atomName(event.xclient.message_type);
            std::cout << "message " << type << " window 0x" << std::hex << event.xclient.window << " format " << std::dec << event.xclient.format;
            for (int i = 0; i < 5; ++i)
            {
                std::cout << " l" << i << "=0x" << std::hex << event.xclient.data.l[i];
            }
            std::cout << std::dec << "\n";
            if (type == "XdndEnter")
            {
                dragSource = static_cast<Window>(event.xclient.data.l[0]);
                targetWindow = event.xclient.window;
                chosenType = None;
                for (int i = 2; i < 5; ++i)
                {
                    if (static_cast<Atom>(event.xclient.data.l[i]) == uriList)
                    {
                        chosenType = uriList;
                    }
                }
                std::cout << "   version " << (event.xclient.data.l[1] >> 24) << ", more than 3 types: " << (event.xclient.data.l[1] & 1);
                for (int i = 2; i < 5; ++i)
                {
                    std::cout << ", type " << atomName(static_cast<Atom>(event.xclient.data.l[i]));
                }
                std::cout << "\n";
            }
            if (type == "XdndDrop")
            {
                std::cout << "   drop: asking the source for text/uri-list\n";
                XConvertSelection(g_display, selectionName, chosenType != None ? chosenType : uriList, dropProperty, sniffer, static_cast<Time>(event.xclient.data.l[2]));
                XFlush(g_display);
            }
            if (type == "XdndPosition")
            {
                std::cout << "   position " << (event.xclient.data.l[2] >> 16) << "," << (event.xclient.data.l[2] & 0xffff) << ", action "
                          << atomName(static_cast<Atom>(event.xclient.data.l[4])) << "\n";
                XEvent reply;
                std::memset(&reply, 0, sizeof(reply));
                reply.xclient.type = ClientMessage;
                reply.xclient.display = g_display;
                reply.xclient.window = static_cast<Window>(event.xclient.data.l[0]);
                reply.xclient.message_type = atom("XdndStatus");
                reply.xclient.format = 32;
                reply.xclient.data.l[0] = static_cast<long>(event.xclient.window);
                reply.xclient.data.l[1] = 1;
                reply.xclient.data.l[4] = static_cast<long>(atom("XdndActionCopy"));
                XSendEvent(g_display, reply.xclient.window, False, NoEventMask, &reply);
                XFlush(g_display);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (old != None)
    {
        setWindow(topLevel, "XdndProxy", old);
    }
    else
    {
        XDeleteProperty(g_display, topLevel, atom("XdndProxy"));
    }
    XDestroyWindow(g_display, sniffer);
    XSync(g_display, False);
    std::cout << "restored\n";
    return 0;
}
