#include "keyboard.h"
#include "layout.h"
#include "blocks.h"
#include <raylib.h>

namespace keyboard {

    void draw(int screenWidth, int screenHeight) {
        const auto& notes = blocks::getActive();
        float whiteKeyWidth = (float)screenWidth / WHITE_KEYS;
        float pianoTop = (float)screenHeight - PIANO_HEIGHT;

        int whiteKeyIndex = 0;
        for (int pitch = PIANO_LOW; pitch <= PIANO_HIGH; ++pitch) {
            if (layout::isBlackKey(pitch)) continue; // skip black keys for now

            float x = whiteKeyIndex * whiteKeyWidth;

            bool isPressed = false;
            int velocity = 0;
            for (const auto& n : notes) {
                if (n.pitch == pitch && n.held) {
                    isPressed = true;
                    velocity = n.velocity;
                    break;
                }
            }

            Color keyColor = RAYWHITE;
            if (isPressed) {
                keyColor = layout::colorForPitch(pitch, velocity);
                keyColor.a = 255; // fully opaque for pressed keys
            }
            DrawRectangle((int)x, (int)pianoTop, (int)whiteKeyWidth - 1, (int)PIANO_HEIGHT, keyColor);
            DrawRectangleLines((int)x, (int)pianoTop, (int)whiteKeyWidth - 1, (int)PIANO_HEIGHT, BLACK);

            whiteKeyIndex++;
        }

        whiteKeyIndex = 0;
        for (int pitch = PIANO_LOW; pitch <= PIANO_HIGH; ++pitch) {
            if (layout::isBlackKey(pitch)) {
                float whiteX = whiteKeyIndex * whiteKeyWidth;
                float blackKeyWidth = whiteKeyWidth * 0.6f;
                float blackKeyHeight = PIANO_HEIGHT * 0.6f;
                float x = whiteX - (blackKeyWidth / 2.0f);

                bool isPressed = false;
                int velocity = 0;
                for (const auto& n : notes) {
                    if (n.pitch == pitch && n.held) {
                        isPressed = true;
                        velocity = n.velocity;
                        break;
                    }
                }
                Color keyColor = BLACK;
                if (isPressed) {
                    keyColor = layout::colorForPitch(pitch, velocity);
                    keyColor.a = 255; // fully opaque for pressed keys
                }
                DrawRectangle((int)x, (int)pianoTop, (int)blackKeyWidth, (int)blackKeyHeight, keyColor);
            }
            else {
                whiteKeyIndex++;
            }
        }
    }

}
