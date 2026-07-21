#include "panel.h"
#include "../visuals/layout.h"
#include "../window_state.h"

#include <raylib.h>
#include <raygui.h>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {
    bool settingsOpen = false;
    float lowColorR = 220.0f;
    float lowColorG = 20.0f;
    float lowColorB = 60.0f;
    float highColorR = 255.0f;
    float highColorG = 0.0f;
    float highColorB = 255.0f;

    Texture2D backgroundTexture = { 0 };
    panel::BackgroundSettings backgroundSettings;
    bool backgroundTextureLoaded = false;

    std::string pickImagePathFromExplorer() {
        const std::string tempPath = "C:/Users/wasif/Desktop/PianoVisualiser/build/selected_image_path.txt";
        const std::string command =
            "powershell -NoProfile -Command \"Add-Type -AssemblyName System.Windows.Forms; "
            "$dialog = New-Object System.Windows.Forms.OpenFileDialog; "
            "$dialog.Title = 'Select an image'; "
            "$dialog.Filter = 'Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.gif;*.webp)|*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.gif;*.webp|All Files (*.*)|*.*'; "
            "if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $dialog.FileName }\" > \"" + tempPath + "\"";

        std::system(command.c_str());

        std::ifstream input(tempPath);
        std::string selectedPath;
        if (input.is_open()) {
            std::getline(input, selectedPath);
        }

        std::remove(tempPath.c_str());
        return selectedPath;
    }

    void loadBackgroundTextureIfNeeded() {
        if (backgroundTextureLoaded) return;
        if (!backgroundSettings.imagePath.empty()) {
            if (FileExists(backgroundSettings.imagePath.c_str())) {
                Image img = LoadImage(backgroundSettings.imagePath.c_str());
                if (img.data != nullptr) {
                    if (backgroundTexture.id != 0) {
                        UnloadTexture(backgroundTexture);
                    }
                    backgroundTexture = LoadTextureFromImage(img);
                    UnloadImage(img);
                    backgroundTextureLoaded = true;
                }
            }
        }
    }
}

namespace panel {

    const BackgroundSettings& getBackgroundSettings() {
        return backgroundSettings;
    }

    void setBackgroundImage(const char* imagePath) {
        backgroundSettings.imagePath = imagePath ? imagePath : "";
        backgroundTextureLoaded = false;
        if (backgroundTexture.id != 0) {
            UnloadTexture(backgroundTexture);
            backgroundTexture = { 0 };
        }
        loadBackgroundTextureIfNeeded();
    }

    void unloadBackgroundImage() {
        if (backgroundTexture.id != 0) {
            UnloadTexture(backgroundTexture);
            backgroundTexture = { 0 };
        }
        backgroundTextureLoaded = false;
        backgroundSettings.imagePath.clear();
        backgroundSettings.offsetX = 0.0f;
        backgroundSettings.offsetY = 0.0f;
        backgroundSettings.scale = 1.0f;
        backgroundSettings.keepAspectRatio = true;
    }

    void toggle() {
        settingsOpen = !settingsOpen;
    }

    bool isOpen() {
        return settingsOpen;
    }

    void update() {
        if (!settingsOpen) return;

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            float settingsWidth = GetScreenWidth() * 0.5f;
            float settingsHeight = GetScreenHeight() * 0.6f;
            float settingsX = (GetScreenWidth() - settingsWidth) / 2.0f;
            float settingsY = (GetScreenHeight() - settingsHeight) / 2.0f;
            Rectangle closeButton = { settingsX + settingsWidth - 40, settingsY + 10, 30, 30 };

            Vector2 mousePos = GetMousePosition();
            if (CheckCollisionPointRec(mousePos, closeButton)) {
                settingsOpen = false;
            }
        }

        layout::ColorRange range = {
            { (unsigned char)lowColorR, (unsigned char)lowColorG, (unsigned char)lowColorB, 255 },
            { (unsigned char)highColorR, (unsigned char)highColorG, (unsigned char)highColorB, 255 }
        };
        layout::setColorRange(range);
    }

    void drawBackground(int screenWidth, int screenHeight) {
        loadBackgroundTextureIfNeeded();
        if (backgroundTexture.id == 0 || backgroundSettings.imagePath.empty()) {
            return;
        }

        if (backgroundSettings.keepAspectRatio) {
            float aspect = (float)backgroundTexture.width / (float)backgroundTexture.height;
            float targetHeight = (float)screenHeight * backgroundSettings.scale;
            float targetWidth = targetHeight * aspect;
            float drawX = (float)screenWidth / 2.0f - targetWidth / 2.0f + backgroundSettings.offsetX;
            float drawY = (float)screenHeight / 2.0f - targetHeight / 2.0f + backgroundSettings.offsetY;
            DrawTexturePro(backgroundTexture,
                { 0, 0, (float)backgroundTexture.width, (float)backgroundTexture.height },
                { drawX, drawY, targetWidth, targetHeight },
                { 0, 0 }, 0.0f, RAYWHITE);
        }
        else {
            float targetWidth = (float)screenWidth * backgroundSettings.scale;
            float targetHeight = (float)screenHeight * backgroundSettings.scale;
            float drawX = (float)screenWidth / 2.0f - targetWidth / 2.0f + backgroundSettings.offsetX;
            float drawY = (float)screenHeight / 2.0f - targetHeight / 2.0f + backgroundSettings.offsetY;
            DrawTexturePro(backgroundTexture,
                { 0, 0, (float)backgroundTexture.width, (float)backgroundTexture.height },
                { drawX, drawY, targetWidth, targetHeight },
                { 0, 0 }, 0.0f, RAYWHITE);
        }
    }

    void draw(int screenWidth, int screenHeight) {
        if (!settingsOpen) return;

        float settingsWidth = screenWidth * 0.5f;
        float settingsHeight = screenHeight * 0.6f;
        float settingsX = (screenWidth - settingsWidth) / 2.0f;
        float settingsY = (screenHeight - settingsHeight) / 2.0f;

        DrawRectangle((int)settingsX, (int)settingsY, (int)settingsWidth, (int)settingsHeight, Color{ 25, 25, 30, 250 });
        DrawRectangleLines((int)settingsX, (int)settingsY, (int)settingsWidth, (int)settingsHeight, RAYWHITE);

        DrawText("Settings", (int)settingsX + 20, (int)settingsY + 20, 30, RAYWHITE);

        Rectangle closeButton = { settingsX + settingsWidth - 40, settingsY + 10, 30, 30 };
        DrawRectangleRec(closeButton, RED);
        DrawText("X", (int)closeButton.x + 8, (int)closeButton.y + 5, 20, RAYWHITE);

        GuiGroupBox({ settingsX + 30, settingsY + 70, settingsWidth - 60, settingsHeight - 110 }, "Color range");

        DrawText("Low note color", (int)settingsX + 50, (int)settingsY + 110, 18, RAYWHITE);
        GuiSlider({ settingsX + 50, settingsY + 135, 180, 20 }, "R", TextFormat("%d", (int)lowColorR), &lowColorR, 0.0f, 255.0f);
        GuiSlider({ settingsX + 50, settingsY + 165, 180, 20 }, "G", TextFormat("%d", (int)lowColorG), &lowColorG, 0.0f, 255.0f);
        GuiSlider({ settingsX + 50, settingsY + 195, 180, 20 }, "B", TextFormat("%d", (int)lowColorB), &lowColorB, 0.0f, 255.0f);

        DrawText("High note color", (int)settingsX + 50, (int)settingsY + 240, 18, RAYWHITE);
        GuiSlider({ settingsX + 50, settingsY + 265, 180, 20 }, "R", TextFormat("%d", (int)highColorR), &highColorR, 0.0f, 255.0f);
        GuiSlider({ settingsX + 50, settingsY + 295, 180, 20 }, "G", TextFormat("%d", (int)highColorG), &highColorG, 0.0f, 255.0f);
        GuiSlider({ settingsX + 50, settingsY + 325, 180, 20 }, "B", TextFormat("%d", (int)highColorB), &highColorB, 0.0f, 255.0f);

        DrawRectangleRec({ settingsX + 280, settingsY + 120, 120, 80 }, Color{ (unsigned char)lowColorR, (unsigned char)lowColorG, (unsigned char)lowColorB, 255 });
        DrawRectangleRec({ settingsX + 280, settingsY + 240, 120, 80 }, Color{ (unsigned char)highColorR, (unsigned char)highColorG, (unsigned char)highColorB, 255 });

        GuiGroupBox({ settingsX + 30, settingsY + 360, settingsWidth - 60, settingsHeight - 410 }, "Background");

        if (GuiButton({ settingsX + 50, settingsY + 390, 180, 30 }, "Choose image")) {
            const bool wasFullscreen = window_state::isWindowedFullscreen();
            if (wasFullscreen) {
                window_state::setWindowMode(false);
            }

            std::string selectedPath = pickImagePathFromExplorer();
            if (!selectedPath.empty()) {
                setBackgroundImage(selectedPath.c_str());
            }

            if (wasFullscreen) {
                window_state::setWindowMode(true);
            }
        }

        if (!backgroundSettings.imagePath.empty()) {
            DrawText(backgroundSettings.imagePath.c_str(), (int)settingsX + 50, (int)settingsY + 430, 14, RAYWHITE);
        }

        bool keepAspectValue = backgroundSettings.keepAspectRatio;
        GuiCheckBox({ settingsX + 50, settingsY + 455, 20, 20 }, "Keep aspect ratio", &keepAspectValue);
        backgroundSettings.keepAspectRatio = keepAspectValue;

        float offsetXValue = backgroundSettings.offsetX;
        float offsetYValue = backgroundSettings.offsetY;
        float scaleValue = backgroundSettings.scale;
        GuiSlider({ settingsX + 50, settingsY + 520, 180, 20 }, "Offset X", TextFormat("%.0f", backgroundSettings.offsetX), &offsetXValue, -1000.0f, 1000.0f);
        GuiSlider({ settingsX + 50, settingsY + 550, 180, 20 }, "Offset Y", TextFormat("%.0f", backgroundSettings.offsetY), &offsetYValue, -1000.0f, 1000.0f);
        GuiSlider({ settingsX + 50, settingsY + 580, 180, 20 }, "Scale", TextFormat("%.2f", backgroundSettings.scale), &scaleValue, 0.1f, 3.0f);
        backgroundSettings.offsetX = offsetXValue;
        backgroundSettings.offsetY = offsetYValue;
        backgroundSettings.scale = scaleValue;
    }

}
