#include "panel.h"
#include "../visuals/layout.h"
#include "../window_state.h"

#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "raymath.h"

namespace {
    struct SettingsLayout {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float viewportX = 0.0f;
        float viewportY = 0.0f;
        float viewportWidth = 0.0f;
        float viewportHeight = 0.0f;
    };

    SettingsLayout getSettingsLayout(int screenWidth, int screenHeight) {
        const float settingsWidth = screenWidth * 0.52f;
        const float settingsHeight = screenHeight * 0.8f;
        const float settingsX = (screenWidth - settingsWidth) / 2.0f;
        const float settingsY = (screenHeight - settingsHeight) / 2.0f;

        return {
            settingsX,
            settingsY,
            settingsWidth,
            settingsHeight,
            settingsX + 16.0f,
            settingsY + 64.0f,
            settingsWidth - 32.0f,
            settingsHeight - 96.0f
        };
    }

    Rectangle getCloseButton(const SettingsLayout& layout) {
        return { layout.x + layout.width - 42.0f, layout.y + 10.0f, 32.0f, 32.0f };
    }

    bool settingsOpen = false;
    float settingsScroll = 0.0f;
    bool draggingScrollThumb = false;
    Color lowColor = { 220, 20, 60, 255 };
    Color highColor = { 255, 0, 255, 255 };

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
        backgroundSettings.scaleX = 1.0f;
        backgroundSettings.scaleY = 1.0f;
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

        const SettingsLayout layout = getSettingsLayout(GetScreenWidth(), GetScreenHeight());
        const Rectangle closeButton = getCloseButton(layout);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mousePos = GetMousePosition();
            if (CheckCollisionPointRec(mousePos, closeButton)) {
                settingsOpen = false;
            }
        }

        layout::ColorRange range = { lowColor, highColor };
        layout::setColorRange(range);
    }

    void drawBackground(int screenWidth, int screenHeight) {
        loadBackgroundTextureIfNeeded();
        if (backgroundTexture.id == 0 || backgroundSettings.imagePath.empty()) {
            return;
        }

        if (backgroundSettings.keepAspectRatio) {
            float fitScale = std::min(
                (float)screenWidth  / (float)backgroundTexture.width,
            (float)screenHeight / (float)backgroundTexture.height
            );
            float targetWidth  = backgroundTexture.width  * fitScale * backgroundSettings.scaleX;
            float targetHeight = backgroundTexture.height * fitScale * backgroundSettings.scaleY;
            float drawX = (float)screenWidth / 2.0f - targetWidth / 2.0f + backgroundSettings.offsetX;
            float drawY = (float)screenHeight / 2.0f - targetHeight / 2.0f + backgroundSettings.offsetY;
            DrawTexturePro(backgroundTexture,
                { 0, 0, (float)backgroundTexture.width, (float)backgroundTexture.height },
                { drawX, drawY, targetWidth, targetHeight },
                { 0, 0 }, 0.0f, RAYWHITE);
        }
        else {
            float targetWidth = (float)screenWidth * backgroundSettings.scaleX;
            float targetHeight = (float)screenHeight * backgroundSettings.scaleY;
            float drawX = (float)screenWidth / 2.0f - targetWidth / 2.0f + backgroundSettings.offsetX;
            float drawY = (float)screenHeight / 2.0f - targetHeight / 2.0f + backgroundSettings.offsetY;
            DrawTexturePro(backgroundTexture,
                { 0, 0, (float)backgroundTexture.width, (float)backgroundTexture.height },
                { drawX, drawY, targetWidth, targetHeight },
                { 0, 0 }, 0.0f, RAYWHITE);
        }
    }

    namespace {
    bool draggingPositionDot = false;
}

void drawPositionPad(float padX, float padY, float padW, float padH, int screenWidth, int screenHeight) {
    DrawRectangleLines((int)padX, (int)padY, (int)padW, (int)padH, RAYWHITE);
    DrawText("Image position", (int)padX, (int)padY - 20, 16, RAYWHITE);

    float scaleX = padW / (float)screenWidth;
    float scaleY = padH / (float)screenHeight;

    float dotX = padX + padW / 2.0f + backgroundSettings.offsetX * scaleX;
    float dotY = padY + padH / 2.0f + backgroundSettings.offsetY * scaleY;

    Vector2 mouse = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        CheckCollisionPointCircle(mouse, { dotX, dotY }, 10.0f)) {
        draggingPositionDot = true;
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        draggingPositionDot = false;
    }

    if (draggingPositionDot) {
        float clampedX = Clamp(mouse.x, padX, padX + padW);
        float clampedY = Clamp(mouse.y, padY, padY + padH);
        backgroundSettings.offsetX = (clampedX - (padX + padW / 2.0f)) / scaleX;
        backgroundSettings.offsetY = (clampedY - (padY + padH / 2.0f)) / scaleY;
        dotX = clampedX;
        dotY = clampedY;
    }

    DrawCircleLines((int)dotX, (int)dotY, 8, RAYWHITE);
    DrawCircle((int)dotX, (int)dotY, 6, draggingPositionDot ? YELLOW : SKYBLUE);
}


    void draw(int screenWidth, int screenHeight) {
        if (!settingsOpen) return;

        const SettingsLayout layout = getSettingsLayout(screenWidth, screenHeight);
        DrawRectangle((int)layout.x, (int)layout.y, (int)layout.width, (int)layout.height, Color{ 25, 25, 30, 250 });
        DrawRectangleLines((int)layout.x, (int)layout.y, (int)layout.width, (int)layout.height, RAYWHITE);

        DrawText("Settings", (int)layout.x + 20, (int)layout.y + 20, 30, RAYWHITE);

        const Rectangle closeButton = getCloseButton(layout);
        DrawRectangleRec(closeButton, RED);
        DrawText("X", (int)closeButton.x + 8, (int)closeButton.y + 5, 20, RAYWHITE);

        const float viewportX = layout.viewportX;
        const float viewportY = layout.viewportY;
        const float viewportWidth = layout.viewportWidth;
        const float viewportHeight = layout.viewportHeight;
        const float contentWidth = viewportWidth - 20.0f;

        // --- Shared spacing constants -------------------------------------------------
        // Everything below is laid out with a running cursor: each element's position
        // is "previous element's bottom edge + a gap", never a hardcoded absolute pixel.
        // That's what makes it impossible for two controls to silently overlap again.
        const float titleOffset   = 34.0f; // space reserved for the GuiGroupBox title bar
        const float sectionGap    = 28.0f; // gap between the Color / Background group boxes
        const float sectionBottomPad = 20.0f; // breathing room before a group box's bottom edge

        const float labelHeight = 20.0f;
        const float labelGap    = 10.0f; // gap between a label and the control(s) under it

        const float rowGap       = 14.0f; // gap between stacked rows (sliders, etc.)
        const float colorBlockGap = 26.0f; // gap between the "Low" and "High" color blocks

        const float colorPickerSize = 140.0f; // width/height of the color picker square
        const float colorPickerHuePadding = 10.0f; // space between the color picker square and the hue slider
        const float colorPickerHueWidth = 16.0f; // width of the hue slider
        const float colorPickerTotalWidth = colorPickerSize + colorPickerHuePadding + colorPickerHueWidth;
        const float colorBlockHeight = colorPickerSize;

        const float leftColumnX  = viewportX + 30.0f;
        const float rightColumnX = leftColumnX + colorPickerTotalWidth + 40.0f;

        // Background section pieces
        const float buttonHeight       = 32.0f;
        const float gapAfterButton     = 14.0f;
        const float pathTextHeight     = 18.0f;
        const float gapAfterPathText   = 14.0f;
        const float checkboxHeight     = 20.0f;
        const float gapAfterCheckbox   = 20.0f;
        const float padLabelReserve    = labelHeight + 12.0f; // space for drawPositionPad's own label
        const float gapAfterPad        = 22.0f;
        const float scaleSliderHeight  = 20.0f;

        // The position pad's height depends on the aspect ratio of screenWidth/screenHeight,
        // so it MUST be computed before we size the group box around it - otherwise the
        // Scale X/Y sliders below it get placed at a fixed offset and end up overlapping it
        // whenever padH is taller than that offset assumed.
        const float padW = std::min(180.0f, contentWidth - 60.0f);
        const float padH = padW * ((float)screenHeight / (float)screenWidth);

        const float colorSectionHeight =
            titleOffset +
            (labelHeight + labelGap + colorBlockHeight) + // Low color block
            colorBlockGap +
            (labelHeight + labelGap + colorBlockHeight) + // High color block
            sectionBottomPad;

        const float backgroundSectionHeight =
            titleOffset +
            buttonHeight + gapAfterButton +
            pathTextHeight + gapAfterPathText +
            checkboxHeight + gapAfterCheckbox +
            padLabelReserve +
            padH +
            gapAfterPad +
            scaleSliderHeight + rowGap +
            scaleSliderHeight +
            sectionBottomPad;

        const float contentHeight = 20.0f + colorSectionHeight + sectionGap + backgroundSectionHeight + 20.0f;
        const float maxScroll = std::max(0.0f, contentHeight - viewportHeight);

        settingsScroll = Clamp(settingsScroll, 0.0f, maxScroll);

        float wheelMove = GetMouseWheelMove();
        if (wheelMove != 0.0f) {
            settingsScroll = Clamp(settingsScroll - wheelMove * 40.0f, 0.0f, maxScroll);
        }

        const float scrollBarX = viewportX + viewportWidth - 14.0f;
        const float scrollBarY = viewportY + 8.0f;
        const float scrollBarHeight = viewportHeight - 16.0f;
        const float thumbHeight = std::max(28.0f, scrollBarHeight * (viewportHeight / std::max(contentHeight, 1.0f)));
        const float thumbY = scrollBarY + (scrollBarHeight - thumbHeight) * (maxScroll > 0.0f ? settingsScroll / maxScroll : 0.0f);
        Rectangle scrollThumb = { scrollBarX, thumbY, 8.0f, thumbHeight };

        Vector2 mouse = GetMousePosition();
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, scrollThumb)) {
            draggingScrollThumb = true;
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            draggingScrollThumb = false;
        }
        if (draggingScrollThumb && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && maxScroll > 0.0f) {
            float dragRatio = (mouse.y - scrollBarY) / (scrollBarHeight - thumbHeight);
            settingsScroll = Clamp(dragRatio * maxScroll, 0.0f, maxScroll);
        }

        DrawRectangleRec({ viewportX, viewportY, viewportWidth, viewportHeight }, Color{ 20, 20, 25, 240 });
        DrawRectangleLinesEx({ viewportX, viewportY, viewportWidth, viewportHeight }, 1.0f, LIGHTGRAY);
        DrawRectangleRec({ scrollBarX, scrollBarY, 8.0f, scrollBarHeight }, Color{ 60, 60, 70, 220 });
        DrawRectangleRec(scrollThumb, Color{ 120, 120, 180, 220 });

        BeginScissorMode((int)viewportX, (int)viewportY, (int)viewportWidth, (int)viewportHeight);

        float y = 20.0f - settingsScroll;

        // ---------------------------------------------------------------- Color section
        GuiGroupBox({ viewportX + 10.0f, viewportY + y, contentWidth, colorSectionHeight }, "Color range");

        float cy = viewportY + y + titleOffset;

        DrawText("Low note color", (int)leftColumnX, (int)cy, 18, RAYWHITE);
        cy += labelHeight + labelGap;

        const float lowBlockTop = cy;
        GuiColorPicker({ leftColumnX, cy, colorPickerSize, colorPickerSize }, nullptr, &lowColor);
        cy += colorPickerSize;



        DrawRectangleRec({ rightColumnX, lowBlockTop, 80.0f, colorBlockHeight }, lowColor);
        DrawRectangleLinesEx({rightColumnX, lowBlockTop, 80.0f, colorBlockHeight}, 1.0f, LIGHTGRAY);
        
        cy += colorBlockGap;

        DrawText("High note color", (int)leftColumnX, (int)cy, 18, RAYWHITE);
        cy += labelHeight + labelGap;

        const float highBlockTop = cy;
        GuiColorPicker({ leftColumnX, cy, colorPickerSize, colorPickerSize }, nullptr, &highColor);
        cy += colorPickerSize;



        DrawRectangleRec({ rightColumnX, highBlockTop, 80.0f, colorBlockHeight }, highColor);
        DrawRectangleLinesEx({rightColumnX, highBlockTop, 80.0f, colorBlockHeight}, 1.0f, LIGHTGRAY);

        // ----------------------------------------------------------- Background section
        y += colorSectionHeight + sectionGap;
        GuiGroupBox({ viewportX + 10.0f, viewportY + y, contentWidth, backgroundSectionHeight }, "Background");

        float cy2 = viewportY + y + titleOffset;

        if (GuiButton({ viewportX + 30.0f, cy2, 180.0f, buttonHeight }, "Choose image")) {
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
        cy2 += buttonHeight + gapAfterButton;

        if (!backgroundSettings.imagePath.empty()) {
            DrawText(backgroundSettings.imagePath.c_str(), (int)(viewportX + 30.0f), (int)cy2, 14, RAYWHITE);
        }
        cy2 += pathTextHeight + gapAfterPathText;

        bool keepAspectValue = backgroundSettings.keepAspectRatio;
        GuiCheckBox({ viewportX + 30.0f, cy2, 20.0f, checkboxHeight }, "Keep aspect ratio", &keepAspectValue);
        backgroundSettings.keepAspectRatio = keepAspectValue;
        cy2 += checkboxHeight + gapAfterCheckbox;

        cy2 += padLabelReserve; // room for drawPositionPad's internal "Image position" label

        drawPositionPad(viewportX + 30.0f, cy2, padW, padH, screenWidth, screenHeight);
        cy2 += padH + gapAfterPad;

        float oldScaleX = backgroundSettings.scaleX;
        float oldScaleY = backgroundSettings.scaleY;
        float scaleXValue = backgroundSettings.scaleX;
        float scaleYValue = backgroundSettings.scaleY;
        GuiSlider({ viewportX + 30.0f, cy2, 180.0f, scaleSliderHeight }, "Scale X", TextFormat("%.2f", scaleXValue), &scaleXValue, 0.1f, 3.0f);
        cy2 += scaleSliderHeight + rowGap;
        GuiSlider({ viewportX + 30.0f, cy2, 180.0f, scaleSliderHeight }, "Scale Y", TextFormat("%.2f", scaleYValue), &scaleYValue, 0.1f, 3.0f);
        cy2 += scaleSliderHeight;

        if (backgroundSettings.keepAspectRatio) {
            if (scaleXValue != oldScaleX)      scaleYValue = scaleXValue;
            else if (scaleYValue != oldScaleY) scaleXValue = scaleYValue;
        }
        backgroundSettings.scaleX = scaleXValue;
        backgroundSettings.scaleY = scaleYValue;

        EndScissorMode();
    }

}