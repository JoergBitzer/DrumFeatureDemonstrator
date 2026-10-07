// Manual test tool (not built by CMake): runs dfd::XdndDropTarget on a plug-in window id for some seconds and prints what is dropped.
//   g++ -std=c++17 -I. tests/manual/xdnd_target_tool.cpp src/platform/XdndTarget.cpp -o xdnd_target_tool -lX11 -pthread
//   ./xdnd_target_tool <plug-in window id> [seconds]      (see "xwininfo -root -tree")
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "src/platform/XdndTarget.h"

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        return 2;
    }
    const unsigned long window = std::strtoul(argv[1], nullptr, 0);
    const int seconds = (argc > 2) ? std::atoi(argv[2]) : 20;
    dfd::XdndDropTarget target;
    target.start(window, [](const std::string& text) { std::cout << "DROPPED: " << text << std::endl; });
    for (int second = 0; second < seconds; ++second)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (second == 2)
        {
            std::cout << "target active: " << target.isActive() << std::endl;
        }
    }
    target.stop();
    std::cout << "stopped" << std::endl;
    return 0;
}
