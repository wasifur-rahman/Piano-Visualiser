#include "panel.h"
#include "../visuals/layout.h"
#include "../visuals/blocks.h"
#include "../visuals/particles.h"
#include "../window_state.h"
#include "../audio.h"

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
            settingsY + 100.0f,
            settingsWidth - 32.0f,
            settingsHeight - 132.0f
        };
    }

    Rectangle getCloseButton(const SettingsLayout& layout) {
        return { layout.x + layout.width - 42.0f, layout.y + 10.0f, 32.0f, 32.0f };
    }

    bool particlesEnabledValue = true;
    float blockRoundednessValue = 0.0f;
    int blockShaderStyleIndex = 0; // 0=None,1=Glossy,2=Metal, 3=Metal2, 4=Neon — mirrors blocks::ShaderStyle

    float shaderAmountValues[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    int draggingAmountIndex = -1;

    // --- Audio tab backing values
    float gainValue = 3.0f;
    float reverbRoomValue = 0.5f;
    float reverbDampValue = 0.0f;
    float reverbWidthValue = 5.0f;
    float reverbLevelValue = 0.0f;
    float chorusVoicesValue = 3.0f; // GuiSlider needs float; cast to int for audio::setChorus
    float chorusLevelValue = 1.2f;
    float chorusSpeedValue = 0.3f;
    float chorusDepthValue = 8.0f;

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

    void loadBackgroundTexture() {
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

    enum class settingsTab { Color, Background, Audio };
    settingsTab currentTab = settingsTab::Color;

    struct tabRects {
        Rectangle color;
        Rectangle background;
        Rectangle audio;
    };

    tabRects getTabRects(const SettingsLayout& layout) {
        const float tabWidth = 150.0f;
        const float tabHeight = 30.0f;
        const float tabY = layout.y + 60.0f;

        return {
            { layout.x + 20.0f, tabY, tabWidth, tabHeight },
            { layout.x + 20.0f + (tabWidth + 10.0f), tabY, tabWidth, tabHeight },
            { layout.x + 20.0f + (tabWidth + 10.0f) * 2.0f, tabY, tabWidth, tabHeight }
        };
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
        loadBackgroundTexture();
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
        const tabRects tabs = getTabRects(layout);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mousePos = GetMousePosition();
            if (CheckCollisionPointRec(mousePos, closeButton)) {
                settingsOpen = false;
            }
            else if (CheckCollisionPointRec(mousePos, tabs.color) && currentTab != settingsTab::Color) {
                currentTab = settingsTab::Color;
                settingsScroll = 0.0f;
            }
            else if (CheckCollisionPointRec(mousePos, tabs.background) && currentTab != settingsTab::Background) {
                currentTab = settingsTab::Background;
                settingsScroll = 0.0f;
            }
            else if (CheckCollisionPointRec(mousePos, tabs.audio) && currentTab != settingsTab::Audio) {
                currentTab = settingsTab::Audio;
                settingsScroll = 0.0f;
            }
        }

        layout::ColorRange range = { lowColor, highColor };
        layout::setColorRange(range);

        particles::setEnabled(particlesEnabledValue);

        blocks::setRoundedness(blockRoundednessValue);
        blocks::setShaderStyle((blocks::ShaderStyle)blockShaderStyleIndex);
        if (blockShaderStyleIndex >= 1 && blockShaderStyleIndex <= 4) {
            blocks::setShaderAmount(shaderAmountValues[blockShaderStyleIndex - 1]);
        }

        audio::setGain(gainValue);
        audio::setReverb(reverbRoomValue, reverbDampValue, reverbWidthValue, reverbLevelValue);
        audio::setChorus((int)chorusVoicesValue, chorusLevelValue, chorusSpeedValue, chorusDepthValue);

        // Per-channel effect sends — without these, the global reverb/chorus
        // bus above has nothing routed into it and the sliders do nothing audible.
        audio::controlChange(91, (int)(reverbLevelValue * 127.0f));       // CC91 = reverb send
        audio::controlChange(93, (int)(chorusLevelValue / 10.0f * 127.0f)); // CC93 = chorus send
    }

    void drawBackground(int screenWidth, int screenHeight) {
        loadBackgroundTexture();
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

void drawVerticalAmountSlider(Rectangle bounds, float& value, int index) {
    const float minVal = 0.0f;
    const float maxVal = 3.0f;

    DrawRectangleRec(bounds, Color{ 45, 45, 50, 255 });
    DrawRectangleLinesEx(bounds, 1.0f, LIGHTGRAY);

    Vector2 mouse = GetMousePosition();
    bool hovering = CheckCollisionPointRec(mouse, bounds);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && hovering) {
        draggingAmountIndex = index;
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        draggingAmountIndex = -1;
    }
    if (draggingAmountIndex == index && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float clampedY = Clamp(mouse.y, bounds.y, bounds.y + bounds.height);
        float t = 1.0f - (clampedY - bounds.y) / bounds.height;
        value = minVal + t * (maxVal - minVal);
    }

    float t = Clamp((value - minVal) / (maxVal - minVal), 0.0f, 1.0f);
    float fillHeight = bounds.height * t;
    DrawRectangleRec({ bounds.x, bounds.y + bounds.height - fillHeight, bounds.width, fillHeight },
        draggingAmountIndex == index ? YELLOW : SKYBLUE);
    DrawRectangle((int)bounds.x - 2, (int)(bounds.y + bounds.height - fillHeight) - 2, (int)bounds.width + 4, 4, RAYWHITE);
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

        const tabRects tabs = getTabRects(layout);
        Color colortabBg = (currentTab == settingsTab::Color) ? Color{ 70, 70, 90, 255 } : Color{ 40, 40, 45, 255 };
        Color backgroundtabBg = (currentTab == settingsTab::Background) ? Color{ 70, 70, 90, 255 } : Color{ 40, 40, 45, 255 };
        Color audiotabBg = (currentTab == settingsTab::Audio) ? Color{ 70, 70, 90, 255 } : Color{ 40, 40, 45, 255 };

        DrawRectangleRec(tabs.color, colortabBg);
        DrawRectangleLinesEx(tabs.color, 1.0f, LIGHTGRAY);
        DrawText("Color", (int)tabs.color.x + 45, (int)tabs.color.y + 6, 18, RAYWHITE);

        DrawRectangleRec(tabs.background, backgroundtabBg);
        DrawRectangleLinesEx(tabs.background, 1.0f, LIGHTGRAY);
        DrawText("Background", (int)tabs.background.x + 20, (int)tabs.background.y + 6, 18, RAYWHITE);

        DrawRectangleRec(tabs.audio, audiotabBg);
        DrawRectangleLinesEx(tabs.audio, 1.0f, LIGHTGRAY);
        DrawText("Audio", (int)tabs.audio.x + 50, (int)tabs.audio.y + 6, 18, RAYWHITE);


        const float viewportX = layout.viewportX;
        const float viewportY = layout.viewportY;
        const float viewportWidth = layout.viewportWidth;
        const float viewportHeight = layout.viewportHeight;
        const float contentWidth = viewportWidth - 20.0f;

        // --- Shared spacing constants -------------------------------------------------
        const float titleOffset   = 34.0f;
        const float sectionGap    = 28.0f;
        const float sectionBottomPad = 20.0f;

        const float labelHeight = 20.0f;
        const float labelGap    = 10.0f;

        const float rowGap       = 14.0f;
        const float colorBlockGap = 26.0f;

        const float colorPickerSize = 140.0f;
        const float colorPickerHuePadding = 10.0f;
        const float colorPickerHueWidth = 16.0f;
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
        const float padLabelReserve    = labelHeight + 12.0f;
        const float gapAfterPad        = 22.0f;
        const float scaleSliderHeight  = 20.0f;

        const float padW = std::min(180.0f, contentWidth - 60.0f);
        const float padH = padW * ((float)screenHeight / (float)screenWidth);

        // Audio section pieces — Gain / Reverb / Chorus sit in three side-by-side
        // columns, each with its own header, so height only needs to fit the
        // tallest column (Reverb and Chorus both have 4 rows).
        const float groupHeaderHeight = 24.0f;
        const float groupHeaderGap = 12.0f;
        const int audioMaxRowsPerColumn = 4;

        const float toggleHeightForLayout = 24.0f;
        const float amtSliderHeightForLayout = 70.0f;
        const float amtSliderGapForLayout = 8.0f;

        const float colorSectionHeight =
            titleOffset +
            (labelHeight + labelGap + colorBlockHeight) + // Low color row
            colorBlockGap +
            (labelHeight + labelGap + colorBlockHeight) + // High color row
            colorBlockGap +
            (labelHeight + labelGap) +                    // "Block Style" label
            amtSliderHeightForLayout + amtSliderGapForLayout +
            toggleHeightForLayout +
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

        const float audioSectionHeight =
            titleOffset +
            groupHeaderHeight + groupHeaderGap +
            audioMaxRowsPerColumn * (labelHeight + labelGap + scaleSliderHeight + rowGap) - rowGap +
            sectionBottomPad;

        float sectionHeight = colorSectionHeight;
        if (currentTab == settingsTab::Background) sectionHeight = backgroundSectionHeight;
        else if (currentTab == settingsTab::Audio) sectionHeight = audioSectionHeight;

        const float contentHeight = 20.0f + sectionHeight + 20.0f;
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

        const float controlsColumnX = rightColumnX + 80.0f + 50.0f;

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

        if (currentTab == settingsTab::Color){
            float cy = viewportY + y + titleOffset;

            DrawText("Low note color", (int)leftColumnX, (int)cy, 18, RAYWHITE);
            cy += labelHeight + labelGap;

            const float lowBlockTop = cy;
            GuiColorPicker({ leftColumnX, cy, colorPickerSize, colorPickerSize }, nullptr, &lowColor);
            cy += colorPickerSize;

            DrawRectangleRec({ rightColumnX, lowBlockTop, 80.0f, colorBlockHeight }, lowColor);
            DrawRectangleLinesEx({ rightColumnX, lowBlockTop, 80.0f, colorBlockHeight }, 1.0f, LIGHTGRAY);

            DrawText("Particles", (int)controlsColumnX, (int)lowBlockTop, 18, RAYWHITE);
            GuiCheckBox({ controlsColumnX, lowBlockTop + labelHeight + labelGap, 20.0f, checkboxHeight }, "Enabled Particles", &particlesEnabledValue);

            cy += colorBlockGap;

            DrawText("High note color", (int)leftColumnX, (int)cy, 18, RAYWHITE);
            cy += labelHeight + labelGap;

            const float highBlockTop = cy;
            GuiColorPicker({ leftColumnX, cy, colorPickerSize, colorPickerSize }, nullptr, &highColor);
            cy += colorPickerSize;

            DrawRectangleRec({ rightColumnX, highBlockTop, 80.0f, colorBlockHeight }, highColor);
            DrawRectangleLinesEx({ rightColumnX, highBlockTop, 80.0f, colorBlockHeight }, 1.0f, LIGHTGRAY);

            DrawText("Block Roundedness", (int)controlsColumnX, (int)highBlockTop, 18, RAYWHITE);
            GuiSlider({ controlsColumnX, highBlockTop + labelHeight + labelGap, 160.0f, scaleSliderHeight }, "", TextFormat("%.2f", blockRoundednessValue), &blockRoundednessValue, 0.0f, 1.0f);

            cy += colorBlockGap;

            // ---- Block Style: full width row under both color pickers ----
            const float toggleWidth  = 65.0f;
            const float toggleHeight = 24.0f;
            const float toggleGap    = 2.0f;   // raygui's default gap between toggle buttons
            const float amtSliderH   = 70.0f;
            const float amtSliderGap = 8.0f;   // gap between slider bottom and button top

            DrawText("Block Style", (int)leftColumnX, (int)cy, 18, RAYWHITE);
            cy += labelHeight + labelGap;

            const float toggleY = cy + amtSliderH + amtSliderGap;
            GuiToggleGroup(
                { leftColumnX, toggleY, toggleWidth, toggleHeight },
                "None;Glossy;Metal;Metal 2;Neon",
                &blockShaderStyleIndex);

            // Sliders sit directly above their matching button (skip "None" at i=0)
            for (int i = 1; i <= 4; ++i) {
                float buttonX = leftColumnX + i * (toggleWidth + toggleGap);
                Rectangle sliderBounds = { buttonX, cy, toggleWidth, amtSliderH };
                drawVerticalAmountSlider(sliderBounds, shaderAmountValues[i - 1], i - 1);
            }

            cy = toggleY + toggleHeight;
        }
        else if (currentTab == settingsTab::Background) {
        // ----------------------------------------------------------- Background section
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

            cy2 += padLabelReserve;

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
        }
        else {
        // ----------------------------------------------------------------- Audio section
        // Grouped horizontally: Gain | Reverb | Chorus, each stacked vertically.
            const float sliderW = 160.0f;
            const float columnGap = 40.0f;

            const float col1X = viewportX + 30.0f;           // Gain
            const float col2X = col1X + sliderW + columnGap; // Reverb
            const float col3X = col2X + sliderW + columnGap; // Chorus

            const float groupTop = viewportY + y + titleOffset;

            DrawText("Gain",   (int)col1X, (int)groupTop, 20, RAYWHITE);
            DrawText("Reverb", (int)col2X, (int)groupTop, 20, RAYWHITE);
            DrawText("Chorus", (int)col3X, (int)groupTop, 20, RAYWHITE);

            auto row = [&](float colX, float& cursorY, const char* label, float& value, float lo, float hi) {
                DrawText(label, (int)colX, (int)cursorY, 16, RAYWHITE);
                cursorY += labelHeight + labelGap;
                GuiSlider({ colX, cursorY, sliderW, scaleSliderHeight }, "", TextFormat("%.2f", value), &value, lo, hi);
                cursorY += scaleSliderHeight + rowGap;
            };

            float gainY   = groupTop + groupHeaderHeight + groupHeaderGap;
            float reverbY = gainY;
            float chorusY = gainY;

            row(col1X, gainY,   "Gain",       gainValue,         0.0f,  10.0f);

            row(col2X, reverbY, "Room Size",  reverbRoomValue,   0.0f,  1.2f);
            row(col2X, reverbY, "Damping",    reverbDampValue,   0.0f,  1.0f);
            row(col2X, reverbY, "Width",      reverbWidthValue,  0.0f,  100.0f);
            row(col2X, reverbY, "Level",      reverbLevelValue,  0.0f,  1.0f);

            row(col3X, chorusY, "Voices",     chorusVoicesValue, 0.0f,  20.0f);
            row(col3X, chorusY, "Level",      chorusLevelValue,  0.0f,  10.0f);
            row(col3X, chorusY, "Speed",      chorusSpeedValue,  0.29f, 5.0f);
            row(col3X, chorusY, "Depth",      chorusDepthValue,  0.0f,  21.0f);
        }
        EndScissorMode();
    }

}