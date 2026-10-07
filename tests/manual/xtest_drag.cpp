// Manual test tool (not built by CMake): moves the mouse pointer with XTest, presses button 1, drags to another position and releases.
//   g++ -std=c++17 tests/manual/xtest_drag.cpp -o xtest_drag -lX11 -lXtst
//   ./xtest_drag <x1> <y1> <x2> <y2>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

#include <chrono>
#include <cstdlib>
#include <thread>

int main(int argc, char* argv[])
{
    if (argc < 5)
    {
        return 2;
    }
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        return 2;
    }
    const int x1 = std::atoi(argv[1]);
    const int y1 = std::atoi(argv[2]);
    const int x2 = std::atoi(argv[3]);
    const int y2 = std::atoi(argv[4]);
    constexpr int kSteps = 40;
    auto pause = [](int milliseconds) { std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds)); };

    XTestFakeMotionEvent(display, -1, x1, y1, 0);
    XFlush(display);
    pause(200);
    XTestFakeButtonEvent(display, 1, True, 0);
    XFlush(display);
    pause(200);
    for (int step = 1; step <= kSteps; ++step)
    {
        XTestFakeMotionEvent(display, -1, x1 + (x2 - x1) * step / kSteps, y1 + (y2 - y1) * step / kSteps, 0);
        XFlush(display);
        pause(25);
    }
    pause(600);
    XTestFakeMotionEvent(display, -1, x2 + 1, y2, 0);
    XFlush(display);
    pause(300);
    XTestFakeButtonEvent(display, 1, False, 0);
    XFlush(display);
    pause(300);
    XCloseDisplay(display);
    return 0;
}
