#include "toolbar.h"
#include "panel.h"
#include "../midi.h"
#include <raylib.h>
#include <raygui.h>
#include <string>
#include <cmath>

namespace {
    bool toolbarVisible = false;
    float toolbarSlide = 0.0f; // 0.0 = hidden, 1.0 = fully visible
    constexpr float toolbarHeight = 60.0f;
    constexpr float toolbarAnimationSpeed = 5.0f;

    // Recomputed each update() call, read back in draw()
    float toolbarY = -toolbarHeight;
    Rectangle minusButton{};
    Rectangle plusButton{};
    Rectangle settingsButton{};

    // Transpose state (moved in from settings.cpp)
    int transposeSemitones = 0;
    constexpr int TRANSPOSE_MIN = -6;
    constexpr int TRANSPOSE_MAX = 6;
}

namespace toolbar {

    void transposeUp() {
        if (transposeSemitones < TRANSPOSE_MAX) transposeSemitones++;
    }

    void transposeDown() {
        if (transposeSemitones > TRANSPOSE_MIN) transposeSemitones--;
    }

    int getTranspose() {
        return transposeSemitones;
    }

    void update(float dt) {
        if (IsKeyPressed(KEY_TAB)) {
            toolbarVisible = !toolbarVisible;
        }

        float targetToolbarSlide = toolbarVisible ? 1.0f : 0.0f;
        toolbarSlide += (targetToolbarSlide - toolbarSlide) * dt * toolbarAnimationSpeed;
        toolbarY = (toolbarSlide * toolbarHeight) - toolbarHeight;

        int screenWidth = GetScreenWidth();
        float clusterWidth = 100.0f; // width of the transpose button cluster
        minusButton = { (float)screenWidth - clusterWidth - 40, toolbarY + 15, 30, 30 };
        plusButton = { (float)screenWidth - 40, toolbarY + 15, 30, 30 };
        settingsButton = { 20, toolbarY + 15, 30, 30 };

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mousePos = GetMousePosition();
            if (CheckCollisionPointRec(mousePos, minusButton)) {
                transposeDown();
            }
            if (CheckCollisionPointRec(mousePos, plusButton)) {
                transposeUp();
            }
            if (CheckCollisionPointRec(mousePos, settingsButton)) {
                panel::toggle();
            }
        }
    }
    

        void draw(int screenWidth) {
            unsigned char toolbarAlpha = (unsigned char)(240 * toolbarSlide);
            DrawRectangle(0, (int)toolbarY, screenWidth, (int)toolbarHeight, { 30, 30, 35, toolbarAlpha });

            constexpr int buttonFontSize = 20;

            DrawRectangleRec(minusButton, GRAY);
            {
                int minusWidth = MeasureText("-", buttonFontSize);
                float minusX = minusButton.x + (minusButton.width - minusWidth) / 2.0f;
                float minusY = minusButton.y + (minusButton.height - buttonFontSize) / 2.0f;
                DrawText("-", (int)minusX, (int)minusY, buttonFontSize, RAYWHITE);
            }

            std::string transposeText = std::to_string(getTranspose());
            int textWidth = MeasureText(transposeText.c_str(), buttonFontSize);
            float textX = minusButton.x + minusButton.width + ((plusButton.x - (minusButton.x + minusButton.width) - textWidth) / 2.0f);
            float textY = minusButton.y + (minusButton.height - buttonFontSize) / 2.0f;
            DrawText(transposeText.c_str(), (int)textX, (int)textY, buttonFontSize, RAYWHITE);

            DrawRectangleRec(plusButton, GRAY);
            {
                int plusWidth = MeasureText("+", buttonFontSize);
                float plusX = plusButton.x + (plusButton.width - plusWidth) / 2.0f;
                float plusY = plusButton.y + (plusButton.height - buttonFontSize) / 2.0f;
                DrawText("+", (int)plusX, (int)plusY, buttonFontSize, RAYWHITE);
            }

            DrawRectangleRec(settingsButton, GRAY);
            {
                constexpr int iconScale = 1;
                constexpr int iconSizePx = 16.0f * iconScale; // raygui icons are 16px square at scale 1
                float iconX = settingsButton.x + (settingsButton.width - iconSizePx) / 2.0f;
                float iconY = settingsButton.y + (settingsButton.height - iconSizePx) / 2.0f;
                GuiDrawIcon(ICON_GEAR, (int)iconX, (int)iconY, iconScale, RAYWHITE);
            }

            const char* midiStatusText = midiInInitialized ? "Connected" : "Disconnected";
            Color midiStatusColor = midiInInitialized ? Color{ 80, 220, 100, 255 } : Color{ 240, 90, 90, 255 };
            const int midiStatusX = (int)(settingsButton.x + settingsButton.width + 20.0f);
            DrawText(midiStatusText, midiStatusX, (int)minusButton.y + 5, 18, midiStatusColor);
        }

}