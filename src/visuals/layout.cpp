#include "layout.h"
#include <algorithm>

namespace {
    layout::ColorRange currentRange = {
        {220, 20, 60, 255},
        {255, 0, 255, 255}
    };
}

namespace layout {

    void setColorRange(const ColorRange& range) {
        currentRange = range;
    }

    const ColorRange& getColorRange() {
        return currentRange;
    }

    Color colorForPitch(int pitch, int velocity) {
        float t = float(pitch - PIANO_LOW) / float(PIANO_HIGH - PIANO_LOW);
        t = std::clamp(t, 0.0f, 1.0f);

        Color low = currentRange.low;
        Color high = currentRange.high;

        unsigned char r = (unsigned char)(low.r + (high.r - low.r) * t);
        unsigned char g = (unsigned char)(low.g + (high.g - low.g) * t);
        unsigned char b = (unsigned char)(low.b + (high.b - low.b) * t);
        unsigned char alpha = (unsigned char)std::min(255, 100 + velocity * 2);
        return {r, g, b, alpha};
    }

    bool isBlackKey(int pitch) {
        int noteInOctave = pitch % 12;
        return noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 ||
            noteInOctave == 8 || noteInOctave == 10;
    }

    KeyRect getKeyRect(int pitch, int screenWidth) {
        float whiteKeyWidth = (float)screenWidth / WHITE_KEYS;

        // Count how many white keys come before this pitch
        int whiteKeyIndex = 0;
        for (int p = PIANO_LOW; p < pitch; ++p) {
            if (!isBlackKey(p)) whiteKeyIndex++;
        }

        if (isBlackKey(pitch)) {
            float blackKeyWidth = whiteKeyWidth * 0.6f;
            float x = (whiteKeyIndex * whiteKeyWidth) - (blackKeyWidth / 2.0f);
            return { x, blackKeyWidth };
        }
        else {
            return { whiteKeyIndex * whiteKeyWidth, whiteKeyWidth };
        }
    }

}
