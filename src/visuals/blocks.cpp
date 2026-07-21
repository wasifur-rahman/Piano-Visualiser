#include "blocks.h"
#include "layout.h"
#include <raylib.h>
#include <algorithm>

constexpr float RISE_SPEED = 220.0f; // pixels/sec block travel speed

namespace {
    std::vector<ActiveNote> activeNotes;
}

namespace blocks {

    void onNoteOn(int pitch, int velocity) {
        activeNotes.push_back({ pitch, velocity, (float)(GetScreenHeight() - PIANO_HEIGHT), true, 0.0f });
    }

    void onNoteOff(int pitch) {
        for (auto& n : activeNotes) {
            if (n.pitch == pitch && n.held) {
                n.held = false; // freeze this block's length, let it keep rising
            }
        }
    }

    void update(float dt) {
        for (auto& n : activeNotes) {
            if (n.held) n.length += RISE_SPEED * dt;
            n.startY -= RISE_SPEED * dt;
        }
        // Remove blocks that have risen off the top of the screen
        activeNotes.erase(
            std::remove_if(activeNotes.begin(), activeNotes.end(),
                [](const ActiveNote& n) { return n.startY + n.length < 0; }),
            activeNotes.end());
    }

    void draw(int screenWidth) {
        for (auto& n : activeNotes) {
            KeyRect kr = layout::getKeyRect(n.pitch, screenWidth);
            Color c = layout::colorForPitch(n.pitch, n.velocity);
            DrawRectangle((int)kr.x, (int)n.startY, (int)kr.width - 2, (int)n.length, c);
        }
    }

    const std::vector<ActiveNote>& getActive() {
        return activeNotes;
    }

}
