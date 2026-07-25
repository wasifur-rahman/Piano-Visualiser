#pragma once
#include <raylib.h>

namespace particles {
    // Spawns a burst of particles at (x, y) using color c, scaled by velocity and size.
    void spawn(float x, float y, Color c, int velocity, float size);

    void setEnabled(bool enabled);
    bool isEnabled();

    void update(float dt, int screenWidth, int screenHeight, int pianoHeight);
    void draw();
}
