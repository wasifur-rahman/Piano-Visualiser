#include "window_state.h"
#include <raylib.h>

namespace {
    bool windowedFullscreen = false;
    int savedWindowX = 100;
    int savedWindowY = 100;
    int savedWindowWidth = 1000;
    int savedWindowHeight = 700;
}

namespace window_state {
    bool isWindowedFullscreen() {
        return windowedFullscreen;
    }

    void setWindowMode(bool enabled) {
        if (enabled) {
            savedWindowX = GetWindowPosition().x;
            savedWindowY = GetWindowPosition().y;
            savedWindowWidth = GetScreenWidth();
            savedWindowHeight = GetScreenHeight();

            int monitor = GetCurrentMonitor();
            SetWindowState(FLAG_WINDOW_UNDECORATED);
            SetWindowPosition(0, 0);
            SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        }
        else {
            ClearWindowState(FLAG_WINDOW_UNDECORATED);
            SetWindowSize(savedWindowWidth, savedWindowHeight);
            SetWindowPosition(savedWindowX, savedWindowY);
        }

        windowedFullscreen = enabled;
    }
}
