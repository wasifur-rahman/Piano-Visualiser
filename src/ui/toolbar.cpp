#include "toolbar.h"
#include "panel.h"
#include "../settings.h"
#include <raylib.h>
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

    void drawSettingsIcon(const Rectangle& button) {
        Vector2 center = { button.x + button.width / 2.0f, button.y + button.height / 2.0f };
        const float outerRadius = 9.0f;
        const float innerRadius = 4.0f;

        DrawCircleLines((int)center.x, (int)center.y, outerRadius, RAYWHITE);
        DrawCircle((int)center.x, (int)center.y, innerRadius, RAYWHITE);

        for (int i = 0; i < 8; i++) {
            float angle = i * (2.0f * PI / 8.0f);
            float toothOuterX = center.x + cosf(angle) * (outerRadius + 2.0f);
            float toothOuterY = center.y + sinf(angle) * (outerRadius + 2.0f);
            float toothInnerX = center.x + cosf(angle) * (outerRadius - 2.0f);
            float toothInnerY = center.y + sinf(angle) * (outerRadius - 2.0f);
            DrawLineEx({ toothInnerX, toothInnerY }, { toothOuterX, toothOuterY }, 2.0f, RAYWHITE);
        }

        DrawCircle((int)center.x, (int)center.y, 2.0f, Color{ 30, 30, 35, 255 });
    }
}

namespace toolbar {

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
                settings::transposeDown();
            }
            if (CheckCollisionPointRec(mousePos, plusButton)) {
                settings::transposeUp();
            }
            if (CheckCollisionPointRec(mousePos, settingsButton)) {
                panel::toggle();
            }
        }
    }

    void draw(int screenWidth) {
        unsigned char toolbarAlpha = (unsigned char)(240 * toolbarSlide);
        DrawRectangle(0, (int)toolbarY, screenWidth, (int)toolbarHeight, { 30, 30, 35, toolbarAlpha });

        DrawRectangleRec(minusButton, GRAY);
        DrawText("-", (int)minusButton.x + 8, (int)minusButton.y + 5, 20, RAYWHITE);

        std::string transposeText = std::to_string(settings::getTranspose());
        int textWidth = MeasureText(transposeText.c_str(), 20);
        float textX = minusButton.x + minusButton.width + ((plusButton.x - (minusButton.x + minusButton.width) - textWidth) / 2.0f);
        DrawText(transposeText.c_str(), (int)textX, (int)minusButton.y + 5, 20, RAYWHITE);

        DrawRectangleRec(plusButton, GRAY);
        DrawText("+", (int)plusButton.x + 8, (int)plusButton.y + 5, 20, RAYWHITE);

        DrawRectangleRec(settingsButton, GRAY);
        drawSettingsIcon(settingsButton);
    }

}
