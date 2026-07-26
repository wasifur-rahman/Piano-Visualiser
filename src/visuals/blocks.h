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

// A single note's full on/off timing, used only in reverse (playback)
// mode. Unlike ActiveNote, both ends of the duration are known up front
// (recording already captured them), so reverse mode is driven purely by
// "what time is it" rather than incremental per-frame growth.
struct NoteSpan {
    int pitch;
    int velocity;
    double onTime;
    double offTime;
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

    // --- Reverse (playback) mode ------------------------------------------
    // Live mode: blocks spawn at the keyboard and rise as you play, growing
    // while held (see onNoteOn/onNoteOff/update above).
    // Reverse mode: the reverse - blocks fall from the top of the screen
    // and reach the keyboard exactly when each note is due to play, like a
    // scrolling piano-tutorial video. This only makes sense when replaying
    // a recording where every note's start/end time is already known, so
    // it's driven by setSchedule()+updateReverse() instead of
    // onNoteOn/onNoteOff/update().
    void setReverseMode(bool enabled);
    bool isReverseMode();

    // Hands blocks the full note schedule for whatever recording is loaded
    // for playback. Call this once whenever a new recording is loaded
    // (playback.cpp does this automatically).
    void setSchedule(std::vector<NoteSpan> notes);

    // Call every frame while in reverse mode, with the current playback
    // time (playback::getPlaybackTime()). Recomputes which scheduled notes
    // are currently falling/visible; draw() renders whatever this leaves
    // behind. No-ops if reverse mode isn't enabled.
    void updateReverse(double playbackTime);
}