#include "XdndProxy.h"

#if defined(__linux__)

#include <X11/Xatom.h>
#include <X11/Xlib.h>

namespace dfd
{
namespace
{
constexpr long kNoLengthLimit = 1;
constexpr int kFormatWindow = 32;

bool hasProperty(Display* display, Window window, Atom property)
{
    Atom actualType = None;
    int actualFormat = 0;
    unsigned long numItems = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    XGetWindowProperty(display, window, property, 0, kNoLengthLimit, False, AnyPropertyType, &actualType, &actualFormat,
                       &numItems, &bytesAfter, &data);
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
    XGetWindowProperty(display, window, property, 0, kNoLengthLimit, False, XA_WINDOW, &actualType, &actualFormat, &numItems,
                       &bytesAfter, &data);
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
    XChangeProperty(display, window, property, XA_WINDOW, kFormatWindow, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&value), 1);
}

// a window may be gone while we work on it (the host closes it): ignore the X error instead of ending the process
int ignoreXErrors(Display*, XErrorEvent*)
{
    return 0;
}

class ScopedIgnoreXErrors
{
public:
    explicit ScopedIgnoreXErrors(Display* display) : m_display(display), m_previous(XSetErrorHandler(ignoreXErrors)) {}
    ~ScopedIgnoreXErrors()
    {
        XSync(m_display, False);
        XSetErrorHandler(m_previous);
    }

private:
    Display* m_display;
    XErrorHandler m_previous;
};

// the window that a file manager treats as the target: the nearest ancestor with WM_STATE; if there is none, the highest
// ancestor below the root window
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
} // namespace

unsigned long installXdndProxy(unsigned long pluginWindow)
{
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        return 0;
    }
    unsigned long installedOn = 0;
    {
        ScopedIgnoreXErrors ignoreErrors(display);
        const Window plugin = static_cast<Window>(pluginWindow);
        const Atom proxyAtom = XInternAtom(display, "XdndProxy", False);
        const Atom wmState = XInternAtom(display, "WM_STATE", False);

        // a top-level window (Standalone) receives drops itself
        if (hasProperty(display, plugin, wmState) == false)
        {
            const Window topLevel = findTopLevelClientWindow(display, plugin, wmState);
            if (topLevel != plugin && topLevel != None)
            {
                const Window existingProxy = readWindowProperty(display, topLevel, proxyAtom);
                if (existingProxy == None || existingProxy == plugin)
                {
                    setWindowProperty(display, plugin, proxyAtom, plugin); // the proxy window names itself
                    setWindowProperty(display, topLevel, proxyAtom, plugin);
                    installedOn = static_cast<unsigned long>(topLevel);
                }
            }
        }
    }
    XCloseDisplay(display);
    return installedOn;
}

void removeXdndProxy(unsigned long topLevelWindow, unsigned long pluginWindow)
{
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        return;
    }
    {
        ScopedIgnoreXErrors ignoreErrors(display);
        const Atom proxyAtom = XInternAtom(display, "XdndProxy", False);
        const Window topLevel = static_cast<Window>(topLevelWindow);
        if (readWindowProperty(display, topLevel, proxyAtom) == static_cast<Window>(pluginWindow))
        {
            XDeleteProperty(display, topLevel, proxyAtom);
        }
    }
    XCloseDisplay(display);
}
} // namespace dfd

#else

namespace dfd
{
unsigned long installXdndProxy(unsigned long)
{
    return 0;
}

void removeXdndProxy(unsigned long, unsigned long)
{
}
} // namespace dfd

#endif
