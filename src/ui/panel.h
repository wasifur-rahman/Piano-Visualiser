#pragma once

#include <string>

// The settings popup panel (volume, customization, etc. to come later).

namespace panel {
    void toggle();
    bool isOpen();

    void update();
    void draw(int screenWidth, int screenHeight);
    void drawBackground(int screenWidth, int screenHeight);

    struct BackgroundSettings {
        std::string imagePath;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float scale = 1.0f;
        bool keepAspectRatio = true;
    };

    const BackgroundSettings& getBackgroundSettings();
    void setBackgroundImage(const char* imagePath);
    void unloadBackgroundImage();
}
