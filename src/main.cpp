// PianoViz - MVP: MIDI in -> FluidSynth audio out -> raylib rising blocks
//
// Architecture:
//   RtMidi callback (runs on its own thread) -> pushes note events into a
//   thread-safe queue -> main thread drains the queue each frame, updates
//   FluidSynth (sound) and the active-notes list (visuals).
//
// This keeps audio-triggering latency low (FluidSynth call happens as soon
// as we drain the queue, not tied to render framerate) while keeping the
// rendering code simple and single-threaded.
//
// main() is just an orchestrator now: each subsystem below owns its own
// state and exposes update()/draw() (or onNoteOn/onNoteOff for event-driven
// ones). See audio.*, midi.*, visuals/*, ui/* for the details.

#include <raylib.h>

#include "midi.h"
#include "audio.h"
#include "visuals/layout.h"
#include "visuals/blocks.h"
#include "visuals/particles.h"
#include "visuals/keyboard.h"
#include "ui/toolbar.h"
#include "ui/panel.h"
#include "window_state.h"

#include <iostream>
#include <string>

// ---------- Config ----------
int SCREEN_W = 1000;
int SCREEN_H = 700;
Texture2D backgroundTexture; // for customizable background later

int main() {
    audio::init();
    attemptConnection();

    // ---------------- raylib window ----------------
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_W, SCREEN_H, "PianoVis");
    // backgroundTexture = LoadTexture("assets/background.PNG"); // customisable background
    std::cout << "Loaded background texture: " << backgroundTexture.width << "x" << backgroundTexture.height << std::endl;
    SetTargetFPS(120);

    while (!WindowShouldClose()) {
        SCREEN_W = GetScreenWidth();
        SCREEN_H = GetScreenHeight();

        float dt = GetFrameTime();

        // ---------------- Input ----------------
        if (IsKeyPressed(KEY_RIGHT)) toolbar::transposeUp();
        if (IsKeyPressed(KEY_LEFT))  toolbar::transposeDown();
        if (IsKeyPressed(KEY_ESCAPE)) break;
        if (IsKeyPressed(KEY_F11)) window_state::setWindowMode(!window_state::isWindowedFullscreen());

        toolbar::update(dt);
        panel::update();

        // ---------------- MIDI device check ----------------
        midiCheckTimer += dt;
        if (midiCheckTimer >= midiCheckInterval) {
            midiCheckTimer = 0.0f;

            if (midiInInitialized) {
                try {
                    const unsigned int portCount = midiIn->getPortCount();
                    if (portCount == 0) {
                        std::cout << "[MIDI] Device disconnected; attempting reconnect." << std::endl;
                        midiInInitialized = false;
                    }
                }
                catch (RtMidiError& e) {
                    std::cout << "[MIDI] Device check failed: " << e.getMessage() << std::endl;
                    midiInInitialized = false;
                }
            }

            if (!midiInInitialized) {
                attemptConnection();
            }
        }

        // ---------------- Drain MIDI queue ----------------
        // Trigger sound immediately, dispatch to whichever visual subsystem cares.
        {
            std::lock_guard<std::mutex> lock(midiMutex);
            while (!midiQueue.empty()) {
                MidiEvent ev = midiQueue.front();
                midiQueue.pop();

                unsigned char statusType = ev.status & 0xF0;
                bool isNoteOn = (statusType == 0x90 && ev.velocity > 0);
                bool isNoteOff = (statusType == 0x80 || (statusType == 0x90 && ev.velocity == 0));
                bool isControlChange = (statusType == 0xB0);

                if (isNoteOn || isNoteOff) {
                    int transposedNote = (int)ev.note + toolbar::getTranspose();

                    if (transposedNote < PIANO_LOW || transposedNote > PIANO_HIGH) {
                        continue; // outside the piano range, ignore
                    }

                    ev.note = (unsigned char)transposedNote;
                }

                if (isNoteOn) {
                    audio::noteOn(ev.note, ev.velocity);
                    blocks::onNoteOn(ev.note, ev.velocity);
                }
                else if (isNoteOff) {
                    audio::noteOff(ev.note);
                    blocks::onNoteOff(ev.note);
                }
                else if (isControlChange) {
                    audio::controlChange(ev.note, ev.velocity);
                }
            }
        }

        // ---------------- Update ----------------
        blocks::update(dt);
        particles::update(dt, SCREEN_W, SCREEN_H, PIANO_HEIGHT);

        // ---------------- Draw ----------------
        BeginDrawing();
        ClearBackground(BLACK); // swap for customizable background later

        float imageAspect = (float)backgroundTexture.width / (float)backgroundTexture.height;
        float imageHeight = (float)SCREEN_H / 2.0f;
        float imageWidth = imageHeight * imageAspect;
        float imageX = ((float)SCREEN_W - imageWidth) / 2.0f;
        float imageY = ((float)SCREEN_H - imageHeight) / 1.5f;

        panel::drawBackground(SCREEN_W, SCREEN_H);

        blocks::draw(SCREEN_W);
        particles::draw();
        keyboard::draw(SCREEN_W, SCREEN_H);
        toolbar::draw(SCREEN_W);
        panel::draw(SCREEN_W, SCREEN_H);

        //DrawFPS(10, 10);
        EndDrawing();
    }

    CloseWindow();

    // ---------------- Cleanup ----------------
    UnloadTexture(backgroundTexture);
    audio::shutdown();

    return 0;
}
