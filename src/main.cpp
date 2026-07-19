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

#include <rtmidi/RtMidi.h>
#include <fluidsynth.h>
#include <raylib.h>

#include <algorithm>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include <iostream>

// ---------- Config ----------
int SCREEN_W = 1000;
int SCREEN_H = 700;
constexpr int PIANO_LOW = 21;   // A0
constexpr int PIANO_HIGH = 108; // C8
constexpr float RISE_SPEED = 220.0f; // pixels/sec block travel speed
constexpr float PIANO_HEIGHT = 150.0f; // height of the piano keyboard area at the bottom of the screen
constexpr int WHITE_KEYS = 52; // number of white keys on a standard piano

// Soundfont path - point this at your downloaded .sf2 file
const char* SOUNDFONT_PATH = "assets/SalC5Light2.sf2";

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
    std::cout << "status" << (int)ev.status
		<< " note" << (int)ev.note
		<< " velocity" << (int)ev.velocity 
        << " screen width" << SCREEN_W << " screen height" << SCREEN_H << std::endl;
    std::lock_guard<std::mutex> lock(midiMutex);
    midiQueue.push(ev);
}

int transposeSemitones = 0; // user-adjustable transpose amount

// ---------- Visual note representation ----------
struct ActiveNote {
    int pitch;
    int velocity;
    float startY;     // y position where the block started rising
    bool held = true; // still being pressed
    float length = 0; // grows while held, frozen once released
};

struct KeyRect {
    float x;
    float width;
};

std::vector<ActiveNote> activeNotes;

// Simple color-by-pitch scheme (swap this out for your own customization system)
Color colorForPitch(int pitch, int velocity) {
    float t = float(pitch - PIANO_LOW) / float(PIANO_HIGH - PIANO_LOW);
    unsigned char r = (unsigned char)(80 + t * 150);
    unsigned char g = (unsigned char)(120 + (1 - t) * 100);
    unsigned char b = 220;
    unsigned char alpha = (unsigned char)std::min(255, 100 + velocity * 2);
    return {r, g, b, alpha};
}

bool isBlackKey(int pitch) {
	int noteInOctave = pitch % 12;
	return noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 ||
		noteInOctave == 8 || noteInOctave == 10;
}

KeyRect getKeyRect(int pitch, int screenWidth) {
    float whiteKeyWidth = (float)screenWidth / WHITE_KEYS;

    // Count how many white keys come before this pitch
    int whiteKeyIndex = 0;
    for (int p = PIANO_LOW; p < pitch; ++p) {
        if (!isBlackKey(p)) whiteKeyIndex++;
    }

    if (isBlackKey(pitch)) {
        float blackKeyWidth = whiteKeyWidth * 0.6f;
        float x = (whiteKeyIndex * whiteKeyWidth) - (blackKeyWidth / 2.0f);
        return { x, blackKeyWidth };
    }
    else {
        return { whiteKeyIndex * whiteKeyWidth, whiteKeyWidth };
    }
}

void drawPianoKeyboard(int screenWidth, int screenHeight, const std::vector<ActiveNote>& notes) {
    float whiteKeyWidth = (float)screenWidth / WHITE_KEYS;
    float pianoTop = (float)screenHeight - PIANO_HEIGHT;

    int whiteKeyIndex = 0;
    for (int pitch = PIANO_LOW; pitch <= PIANO_HIGH; ++pitch) {
        if (isBlackKey(pitch)) continue; // skip black keys for now

        float x = whiteKeyIndex * whiteKeyWidth;

        bool isPressed = false;
        for (const auto& n : notes) {
            if (n.pitch == pitch && n.held) {
                isPressed = true;
                break;
            }
        }

        Color keyColor = isPressed ? Color{ 225, 220, 100, 255 } : RAYWHITE;
        DrawRectangle((int)x, (int)pianoTop, (int)whiteKeyWidth - 1, (int)PIANO_HEIGHT, keyColor);
        DrawRectangleLines((int)x, (int)pianoTop, (int)whiteKeyWidth - 1, (int)PIANO_HEIGHT, BLACK);

        whiteKeyIndex++;

    }
    whiteKeyIndex = 0;
    for (int pitch = PIANO_LOW; pitch <= PIANO_HIGH; ++pitch) {
        if (isBlackKey(pitch)) {
            float whiteX = whiteKeyIndex * whiteKeyWidth;
            float blackKeyWidth = whiteKeyWidth * 0.6f;
            float blackKeyHeight = PIANO_HEIGHT * 0.6f;
            float x = whiteX - (blackKeyWidth / 2.0f);

            bool isPressed = false;
            for (const auto& n : notes) {
                if (n.pitch == pitch && n.held) {
                    isPressed = true;
                    break;
                }
            }
            Color keyColor = isPressed ? Color{ 225, 220, 100, 255 } : BLACK;
            DrawRectangle((int)x, (int)pianoTop, (int)blackKeyWidth, (int)blackKeyHeight, keyColor);
        }
        else {
            whiteKeyIndex++;
        }
    }
}

int main() {
    // ---------------- FluidSynth setup ----------------
    fluid_settings_t* settings = new_fluid_settings();
    fluid_settings_setnum(settings, "synth.gain", 1.0);
    fluid_settings_setstr(settings, "audio.driver", "wasapi"); // Windows low-latency; use "coreaudio" on Mac, "alsa" on Linux
	fluid_settings_setint(settings, "audio.period-size", 64); // smaller buffer = lower latency, but more CPU usage
	fluid_settings_setint(settings, "audio.periods", 2); // number of periods in the audio buffer
	fluid_settings_setint(settings, "synth.interpolation", 4); // linear interpolation for better sound quality

    fluid_synth_t* synth = new_fluid_synth(settings);
    fluid_audio_driver_t* audioDriver = new_fluid_audio_driver(settings, synth);

    int sfID = fluid_synth_sfload(synth, SOUNDFONT_PATH, 1);
    if (sfID == FLUID_FAILED) {
        // Handle missing soundfont gracefully - app should still run visuals
    }
    fluid_synth_program_select(synth, 0, sfID, 0, 0); // channel 0, bank 0, preset 0 (usually grand piano)

    // Built-in reverb/chorus - this is your "tone shaping" starting point
    fluid_synth_set_reverb(synth, /*roomsize*/10.0, /*damping*/0.0, /*width*/5.0, /*level*/0.0);
    fluid_synth_set_chorus(synth, /*nr*/3, /*level*/1.2, /*speed*/0.3, /*depth*/8.0, FLUID_CHORUS_MOD_SINE);
	fluid_synth_set_gain(synth, 3.0f); // overall volume

    // ---------------- RtMidi setup ----------------
    RtMidiIn midiIn;
    try {
        unsigned int nPorts = midiIn.getPortCount();
        if (nPorts == 0) {
            // No MIDI device found - could still run in "demo" mode
        }
        else {
            // TODO: let user pick the port instead of always using port 0
            midiIn.openPort(0);
            midiIn.setCallback(&midiCallback);
            midiIn.ignoreTypes(false, false, false); // don't ignore sysex/timing/active-sense
        }
	}
	catch (RtMidiError& e) {
		// Handle MIDI initialization error gracefully - app should still run visuals
	}
    // ---------------- raylib window ----------------
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(SCREEN_W, SCREEN_H, "PianoVis");
    SetTargetFPS(120);

    while (!WindowShouldClose()) {
		SCREEN_W = GetScreenWidth();
		SCREEN_H = GetScreenHeight();

        float dt = GetFrameTime();

		if (IsKeyPressed(KEY_RIGHT) && transposeSemitones < 6) transposeSemitones++;
		if (IsKeyPressed(KEY_LEFT) && transposeSemitones > -6 && transposeSemitones > -6) transposeSemitones--;

		if (IsKeyPressed(KEY_ESCAPE)) break;

		if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        // Drain MIDI queue - trigger sound immediately, update visual state
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
                    int transpoedNote = (int)ev.note + transposeSemitones;

					if (transpoedNote < PIANO_LOW || transpoedNote > PIANO_HIGH) {
						// Ignore notes outside the piano range
						continue;
					}

					ev.note = (unsigned char)transpoedNote;
                }

                if (isNoteOn) {
                    fluid_synth_noteon(synth, 0, ev.note, ev.velocity);
					activeNotes.push_back({ ev.note, ev.velocity, (float)(GetScreenHeight() - PIANO_HEIGHT), true, 0.0f });
                } else if (isNoteOff) {
                    fluid_synth_noteoff(synth, 0, ev.note);
                    for (auto& n : activeNotes) {
                        if (n.pitch == ev.note && n.held) {
                            n.held = false; // freeze this block's length, let it keep rising
                        }
                    }
                } else if (isControlChange) {
					fluid_synth_cc(synth, 0, ev.note, ev.velocity); // pass through control changes to synth)
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
			KeyRect kr = getKeyRect(n.pitch, SCREEN_W);
			Color c = colorForPitch(n.pitch, n.velocity);
			DrawRectangle((int)kr.x, (int)n.startY, (int)kr.width - 2, (int)n.length, c);
        }

        drawPianoKeyboard(SCREEN_W, SCREEN_H, activeNotes);

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