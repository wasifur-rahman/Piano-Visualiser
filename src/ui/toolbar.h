#pragma once

// Top toolbar: slide-down transpose controls + settings gear button.
// Owns its own visibility/animation state; main just calls update() then draw().

namespace toolbar {
    void update(float dt);
    void draw(int screenWidth);
}
