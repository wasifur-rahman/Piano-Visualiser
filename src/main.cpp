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
#include "recording.h"
#include "playback.h"
#include "visuals/layout.h"
#include "visuals/blocks.h"
#include "visuals/particles.h"
#include "visuals/keyboard.h"
#include "ui/toolbar.h"
#include "ui/panel.h"
#include "window_state.h"

#include <iostream>
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <filesystem>

// ---------- Config ----------
int SCREEN_W = 1000;
int SCREEN_H = 700;
Texture2D backgroundTexture; // for customizable background later

// main.cpp lives at <project_root>/src/main.cpp, so its own compile-time
// __FILE__ path lets us find the project root reliably regardless of the
// process's working directory (i.e. regardless of whether this is launched
// via the VS debugger, by double-clicking the exe, etc). This does assume
// we're running on the machine this was built on, since __FILE__ bakes in
// an absolute path at compile time - fine for a dev build, but revisit this
// if the app is ever packaged up for other machines.
static std::filesystem::path getProjectRoot() {
    std::filesystem::path sourceFile = __FILE__; // .../<project_root>/src/main.cpp
    return sourceFile.parent_path().parent_path(); // src/ -> project root
}

// Builds a shared timestamped base name (e.g. "recording_20260726_143205")
// and creates <project_root>/recordings/pvrc/ and .../recordings/midi/,
// so each stopped recording gets its own file in each format instead of
// overwriting the last one, e.g.:
//   recordings/pvrc/recording_20260726_143205.pvrc
//   recordings/midi/recording_20260726_143205.mid
struct RecordingPaths {
    std::string pvrcPath;
    std::string midPath;
};

static RecordingPaths generateRecordingPaths() {
    const std::filesystem::path recordingsDir = getProjectRoot() / "recordings";
    const std::filesystem::path pvrcDir = recordingsDir / "pvrc";
    const std::filesystem::path midiDir = recordingsDir / "midi";

    std::error_code ec;
    std::filesystem::create_directories(pvrcDir, ec);
    if (ec) {
        std::cerr << "[Recording] Couldn't create '" << pvrcDir.string()
                   << "' directory: " << ec.message() << std::endl;
    }
    ec.clear();
    std::filesystem::create_directories(midiDir, ec);
    if (ec) {
        std::cerr << "[Recording] Couldn't create '" << midiDir.string()
                   << "' directory: " << ec.message() << std::endl;
    }

    std::time_t now = std::time(nullptr);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    std::ostringstream oss;
    oss << "recording_" << std::put_time(&localTime, "%Y%m%d_%H%M%S");
    const std::string baseName = oss.str();

    return RecordingPaths{
        (pvrcDir / (baseName + ".pvrc")).string(),
        (midiDir / (baseName + ".mid")).string()
    };
}

int main() {
    audio::init();
    attemptConnection();

    // ---------------- raylib window ----------------
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_W, SCREEN_H, "PianoVis");
    // backgroundTexture = LoadTexture("assets/background.PNG"); // customisable background
    std::cout << "Loaded background texture: " << backgroundTexture.width << "x" << backgroundTexture.height << std::endl;
    SetTargetFPS(120);

    bool wasRecording = false;

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
        recording::update(dt);
        playback::update(dt);
        blocks::updateReverse(playback::getPlaybackTime()); // no-op unless reverse mode is on

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
                    recording::onEvent(ev);
                    audio::noteOn(ev.note, ev.velocity);
                    blocks::onNoteOn(ev.note, ev.velocity);
                }
                else if (isNoteOff) {
                    recording::onEvent(ev);
                    audio::noteOff(ev.note);
                    blocks::onNoteOff(ev.note);
                }
                else if (isControlChange) {
                    // Includes the sustain pedal (CC 64) - needs to be
                    // recorded too, not just dispatched live, or pedal
                    // presses vanish from anything you play back later.
                    recording::onEvent(ev);
                    audio::controlChange(ev.note, ev.velocity);
                }
            }
        }

        // ---------------- Recording save-on-stop ----------------
        // The toolbar's record button flips recording::isRecording() via
        // toggle(); here we just watch for the on->off transition and
        // persist whatever was captured - as both our native .pvrc format
        // and a standard .mid file, in their own subfolders under
        // recordings/ (see generateRecordingPaths()).
        bool isRecordingNow = recording::isRecording();
        if (wasRecording && !isRecordingNow) {
            RecordingPaths paths = generateRecordingPaths();
            if (recording::saveToFile(paths.pvrcPath)) {
                std::cout << "[Recording] Saved to " << paths.pvrcPath << std::endl;
            }
            if (recording::saveToMidFile(paths.midPath)) {
                std::cout << "[Recording] Saved to " << paths.midPath << std::endl;
            }
        }
        wasRecording = isRecordingNow;

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