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

#include <RtMidi.h>
#include <fluidsynth.h>
#include <raylib.h>

#include <algorithm>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

// ---------- Config ----------
constexpr int SCREEN_W = 1000;
constexpr int SCREEN_H = 700;
constexpr int PIANO_LOW = 21;   // A0
constexpr int PIANO_HIGH = 108; // C8
constexpr float RISE_SPEED = 220.0f; // pixels/sec block travel speed

// Soundfont path - point this at your downloaded .sf2 file
const char* SOUNDFONT_PATH = "assets/SalamanderGrandPiano.sf2";

// ---------- MIDI event queue (thread-safe) ----------
struct MidiEvent {
    unsigned char status;
    unsigned char note;
    unsigned char velocity;
};

std::mutex midiMutex;
std::queue<MidiEvent> midiQueue;

void midiCallback(double /*deltatime*/, std::vector<unsigned char>* message, void* /*userData*/) {
    if (!message || message->size() < 3) return;
    MidiEvent ev{(*message)[0], (*message)[1], (*message)[2]};
    std::lock_guard<std::mutex> lock(midiMutex);
    midiQueue.push(ev);
}

// ---------- Visual note representation ----------
struct ActiveNote {
    int pitch;
    int velocity;
    float startY;     // y position where the block started rising
    bool held = true; // still being pressed
    float length = 0; // grows while held, frozen once released
};

std::vector<ActiveNote> activeNotes;

// Map MIDI pitch (21-108) to an x column on screen
float pitchToX(int pitch) {
    int range = PIANO_HIGH - PIANO_LOW;
    float t = float(pitch - PIANO_LOW) / float(range);
    return t * (SCREEN_W - 20.0f) + 10.0f;
}

// Simple color-by-pitch scheme (swap this out for your own customization system)
Color colorForPitch(int pitch, int velocity) {
    float t = float(pitch - PIANO_LOW) / float(PIANO_HIGH - PIANO_LOW);
    unsigned char r = (unsigned char)(80 + t * 150);
    unsigned char g = (unsigned char)(120 + (1 - t) * 100);
    unsigned char b = 220;
    unsigned char alpha = (unsigned char)std::min(255, 100 + velocity * 2);
    return {r, g, b, alpha};
}

int main() {
    // ---------------- FluidSynth setup ----------------
    fluid_settings_t* settings = new_fluid_settings();
    fluid_settings_setnum(settings, "synth.gain", 1.0);
    fluid_settings_setstr(settings, "audio.driver", "wasapi"); // Windows low-latency; use "coreaudio" on Mac, "alsa" on Linux

    fluid_synth_t* synth = new_fluid_synth(settings);
    fluid_audio_driver_t* audioDriver = new_fluid_audio_driver(settings, synth);

    int sfID = fluid_synth_sfload(synth, SOUNDFONT_PATH, 1);
    if (sfID == FLUID_FAILED) {
        // Handle missing soundfont gracefully - app should still run visuals
    }
    fluid_synth_program_select(synth, 0, sfID, 0, 0); // channel 0, bank 0, preset 0 (usually grand piano)

    // Built-in reverb/chorus - this is your "tone shaping" starting point
    fluid_synth_set_reverb(synth, /*roomsize*/0.4, /*damping*/0.3, /*width*/0.5, /*level*/0.7);
    fluid_synth_set_chorus(synth, /*nr*/3, /*level*/1.2, /*speed*/0.3, /*depth*/8.0, FLUID_CHORUS_MOD_SINE);

    // ---------------- RtMidi setup ----------------
    RtMidiIn midiIn;
    unsigned int nPorts = midiIn.getPortCount();
    if (nPorts == 0) {
        // No MIDI device found - could still run in "demo" mode
    } else {
        // TODO: let user pick the port instead of always using port 0
        midiIn.openPort(0);
        midiIn.setCallback(&midiCallback);
        midiIn.ignoreTypes(false, false, false); // don't ignore sysex/timing/active-sense
    }

    // ---------------- raylib window ----------------
    InitWindow(SCREEN_W, SCREEN_H, "PianoViz");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // Drain MIDI queue - trigger sound immediately, update visual state
        {
            std::lock_guard<std::mutex> lock(midiMutex);
            while (!midiQueue.empty()) {
                MidiEvent ev = midiQueue.front();
                midiQueue.pop();

                unsigned char statusType = ev.status & 0xF0;
                bool isNoteOn = (statusType == 0x90 && ev.velocity > 0);
                bool isNoteOff = (statusType == 0x80 || (statusType == 0x90 && ev.velocity == 0));

                if (isNoteOn) {
                    fluid_synth_noteon(synth, 0, ev.note, ev.velocity);
                    activeNotes.push_back({ev.note, ev.velocity, (float)SCREEN_H, true, 0});
                } else if (isNoteOff) {
                    fluid_synth_noteoff(synth, 0, ev.note);
                    for (auto& n : activeNotes) {
                        if (n.pitch == ev.note && n.held) {
                            n.held = false; // freeze this block's length, let it keep rising
                        }
                    }
                }
            }
        }

        // Update block lengths/positions
        for (auto& n : activeNotes) {
            if (n.held) n.length += RISE_SPEED * dt;
            n.startY -= RISE_SPEED * dt;
        }
        // Remove blocks that have risen off the top of the screen
        activeNotes.erase(
            std::remove_if(activeNotes.begin(), activeNotes.end(),
                [](const ActiveNote& n) { return n.startY + n.length < 0; }),
            activeNotes.end());

        // ---------------- Draw ----------------
        BeginDrawing();
        ClearBackground({18, 18, 24, 255}); // swap for customizable background later

        for (auto& n : activeNotes) {
            float x = pitchToX(n.pitch);
            float w = 8.0f;
            Color c = colorForPitch(n.pitch, n.velocity);
            DrawRectangle((int)x, (int)(n.startY), (int)w, (int)n.length, c);
        }

        DrawFPS(10, 10);
        EndDrawing();
    }

    CloseWindow();

    // ---------------- Cleanup ----------------
    delete_fluid_audio_driver(audioDriver);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);

    return 0;
}
