#pragma once

// Shared piano geometry + color helpers used by both the keyboard and the
// falling note blocks. Kept separate so neither of those two files has to
// include the other just to get colorForPitch/getKeyRect.

#include <raylib.h>

constexpr int PIANO_LOW = 21;   // A0
constexpr int PIANO_HIGH = 108; // C8
constexpr float PIANO_HEIGHT = 150.0f; // height of the piano keyboard area at the bottom of the screen
constexpr int WHITE_KEYS = 52; // number of white keys on a standard piano

struct KeyRect {
    float x;
    float width;
};

namespace layout {
    struct ColorRange {
        Color low;
        Color high;
    };

    void setColorRange(const ColorRange& range);
    const ColorRange& getColorRange();

    Color colorForPitch(int pitch, int velocity);

    bool isBlackKey(int pitch);

    KeyRect getKeyRect(int pitch, int screenWidth);
}
