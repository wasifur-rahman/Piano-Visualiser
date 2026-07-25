#pragma once
#include <vector>
// ActiveNote is public because keyboard.cpp needs to read it (to know
// which keys are currently pressed) but the vector of them stays private
// inside blocks.cpp -- everyone else goes through onNoteOn/onNoteOff.
struct ActiveNote {
    int pitch;
    int velocity;
    float startY;     // y position where the block started rising
    bool held = true; // still being pressed
    float length = 0; // grows while held, frozen once released
};
namespace blocks {
    enum class ShaderStyle {
        None = 0,
        Glossy = 1,
        Metallic = 2,
        Metal2 = 3,  // diamond-plate / checker-plate tread steel
        Neon = 4
    };

    void onNoteOn(int pitch, int velocity);
    void onNoteOff(int pitch);
    void setRoundedness(float r);
    float getRoundedness();
    void setShaderStyle(ShaderStyle style);
    ShaderStyle getShaderStyle();
    void setShaderAmount(float amount); // generic "more" intensity, per-style meaning (see block.fs)
    float getShaderAmount();
    void update(float dt);
    void draw(int screenWidth);
    const std::vector<ActiveNote>& getActive();
}