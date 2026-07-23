#pragma once

// Top toolbar: slide-down transpose controls + settings gear button.
// Owns its own visibility/animation state; main just calls update() then draw().

namespace toolbar {
    void update(float dt);
    void draw(int screenWidth);

    // Transpose control (was settings::, folded in here since the toolbar
    // is the only thing that owns/displays it).
    void transposeUp();
    void transposeDown();
    int getTranspose();
}