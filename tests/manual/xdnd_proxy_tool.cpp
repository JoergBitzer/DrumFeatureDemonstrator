// Manual test tool (not built by CMake): sets the XdndProxy of the top-level window above a plug-in window.
//   g++ -I. tests/manual/xdnd_proxy_tool.cpp src/platform/XdndProxy.cpp -o xdnd_proxy_tool -lX11
//   ./xdnd_proxy_tool 0x6800009      (the id of the plug-in window, see "xwininfo -root -tree")
#include <cstdlib>
#include <iostream>

#include "src/platform/XdndProxy.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "usage: xdnd_proxy_tool <window id>\n";
        return 2;
    }
    const unsigned long window = std::strtoul(argv[1], nullptr, 0);
    const unsigned long topLevel = dfd::installXdndProxy(window);
    if (topLevel == 0)
    {
        std::cout << "nothing changed\n";
        return 1;
    }
    std::cout << "XdndProxy of window 0x" << std::hex << topLevel << " now points to 0x" << window << "\n";
    return 0;
}
