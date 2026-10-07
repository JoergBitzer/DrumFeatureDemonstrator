// Manual test tool (not built by CMake): acts as the SOURCE of an XDND file drag, like a file manager does, and drops a file
// onto the window under the given screen position. Shows every step, so a refusal can be located.
//   g++ -std=c++17 tests/manual/xdnd_source_tool.cpp -o xdnd_source_tool -lX11
//   ./xdnd_source_tool <top-level window id> <x> <y> <file>      (x, y: screen position of the drop)
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

Window readWindowProperty(Window window, const char* name)
{
    Atom type = None;
    int format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    Window result = None;
    XGetWindowProperty(g_display, window, atom(name), 0, 1, False, XA_WINDOW, &type, &format, &items, &after, &data);
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

long readAtomProperty(Window window, const char* name)
{
    Atom type = None;
    int format = 0;
    unsigned long items = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    long result = -1;
    XGetWindowProperty(g_display, window, atom(name), 0, 1, False, XA_ATOM, &type, &format, &items, &after, &data);
    if (data != nullptr && type == XA_ATOM && items > 0)
    {
        result = static_cast<long>(*reinterpret_cast<Atom*>(data));
    }
    if (data != nullptr)
    {
        XFree(data);
    }
    return result;
}

void sendMessage(Window target, Window source, const char* type, long l1, long l2, long l3, long l4)
{
    XEvent event;
    std::memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.display = g_display;
    event.xclient.window = target;
    event.xclient.message_type = atom(type);
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(source);
    event.xclient.data.l[1] = l1;
    event.xclient.data.l[2] = l2;
    event.xclient.data.l[3] = l3;
    event.xclient.data.l[4] = l4;
    XSendEvent(g_display, target, False, NoEventMask, &event);
    XFlush(g_display);
}
} // namespace

int main(int argc, char* argv[])
{
    if (argc < 5)
    {
        std::cout << "usage: xdnd_source_tool <top-level window id> <x> <y> <file>\n";
        return 2;
    }
    g_display = XOpenDisplay(nullptr);
    if (g_display == nullptr)
    {
        return 2;
    }
    const Window topLevel = std::strtoul(argv[1], nullptr, 0);
    const int x = std::atoi(argv[2]);
    const int y = std::atoi(argv[3]);
    const std::string uri = std::string("file://") + argv[4] + "\r\n";

    // 1. which window gets the messages, like GTK: XdndProxy of the top-level window (if the proxy points to itself), else the window itself
    Window target = topLevel;
    const Window proxy = readWindowProperty(topLevel, "XdndProxy");
    std::cout << "top-level 0x" << std::hex << topLevel << ", XdndProxy: 0x" << proxy << std::dec << "\n";
    if (proxy != None && readWindowProperty(proxy, "XdndProxy") == proxy)
    {
        target = proxy;
    }
    const long version = readAtomProperty(target, "XdndAware");
    std::cout << "target 0x" << std::hex << target << std::dec << ", XdndAware version atom: " << version << "\n";
    if (version < 0)
    {
        std::cout << "REFUSED: the target window is not XdndAware\n";
        return 1;
    }

    // 2. the source window owns the selection
    const Window source = XCreateSimpleWindow(g_display, DefaultRootWindow(g_display), 0, 0, 1, 1, 0, 0, 0);
    XSelectInput(g_display, source, PropertyChangeMask);
    XSetSelectionOwner(g_display, atom("XdndSelection"), source, CurrentTime);
    const Atom uriList = atom("text/uri-list");

    // 3. enter, position
    const long protocolVersion = (version < 5) ? version : 5; // the lower of both sides, like a real source
    sendMessage(target, source, "XdndEnter", protocolVersion << 24, static_cast<long>(uriList), 0, 0);
    sendMessage(target, source, "XdndPosition", 0, (x << 16) | y, CurrentTime, static_cast<long>(atom("XdndActionCopy")));
    std::cout << "sent XdndEnter and XdndPosition at " << x << "," << y << "\n";

    bool accepted = false;
    bool gotStatus = false;
    bool dropSent = false;
    bool finished = false;
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(6) && finished == false)
    {
        while (XPending(g_display) > 0)
        {
            XEvent event;
            XNextEvent(g_display, &event);
            if (event.type == ClientMessage)
            {
                const std::string type = XGetAtomName(g_display, event.xclient.message_type);
                std::cout << "got " << type << "\n";
                if (type == "XdndStatus")
                {
                    gotStatus = true;
                    accepted = (event.xclient.data.l[1] & 1) != 0;
                    std::cout << "  accept flag: " << accepted << "\n";
                    if (accepted && dropSent == false)
                    {
                        sendMessage(target, source, "XdndDrop", 0, CurrentTime, 0, 0);
                        dropSent = true;
                        std::cout << "sent XdndDrop\n";
                    }
                }
                if (type == "XdndFinished")
                {
                    finished = true;
                    std::cout << "  drop finished, accepted flag " << (event.xclient.data.l[1] & 1) << "\n";
                }
            }
            if (event.type == SelectionRequest)
            {
                const XSelectionRequestEvent& request = event.xselectionrequest;
                std::cout << "got SelectionRequest for " << XGetAtomName(g_display, request.target) << "\n";
                XEvent reply;
                std::memset(&reply, 0, sizeof(reply));
                reply.xselection.type = SelectionNotify;
                reply.xselection.requestor = request.requestor;
                reply.xselection.selection = request.selection;
                reply.xselection.target = request.target;
                reply.xselection.time = request.time;
                reply.xselection.property = None;
                if (request.target == uriList || request.target == atom("text/plain") || request.target == XA_STRING)
                {
                    XChangeProperty(g_display, request.requestor, request.property, request.target, 8, PropModeReplace,
                                    reinterpret_cast<const unsigned char*>(uri.c_str()), static_cast<int>(uri.size()));
                    reply.xselection.property = request.property;
                }
                XSendEvent(g_display, request.requestor, False, NoEventMask, &reply);
                XFlush(g_display);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if (gotStatus == false)
    {
        std::cout << "RESULT: the target never answered XdndPosition\n";
        return 1;
    }
    if (accepted == false)
    {
        std::cout << "RESULT: the target answered, but does not accept the drop at this position\n";
        return 1;
    }
    std::cout << (finished ? "RESULT: the drop was completed\n" : "RESULT: no XdndFinished after the drop\n");
    return finished ? 0 : 1;
}
