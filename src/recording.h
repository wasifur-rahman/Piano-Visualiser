#pragma once
#include "midi.h"
#include <vector>
#include <string>
#include <cstdint>

// A MIDI event with a timestamp attached, measured in seconds since the
// recording began. This is the shared "currency" between recording and
// playback: recording produces a vector of these, playback consumes one.
struct TimedMidiEvent {
    double timestamp;
    unsigned char status;
    unsigned char note;
    unsigned char velocity;
};

namespace recording {
    // Start a fresh recording (clears any previously recorded events).
    void start();

    // Stop recording. Recorded events remain available via getEvents()
    // until start() or clear() is called again.
    void stop();

    // Convenience toggle for a single UI button.
    void toggle();

    bool isRecording();

    // Call once per frame (e.g. from main.cpp's loop) to advance the
    // internal clock used to timestamp events.
    void update(float dt);

    // Call this from main.cpp's MIDI dispatch block for every note on/off
    // (and optionally CC) event, live or already-transposed - whichever
    // representation you want played back later. No-ops if not recording.
    void onEvent(const MidiEvent& ev);

    double getElapsedTime();
    size_t getEventCount();

    // Read-only access to what's been recorded so far - playback can copy
    // this directly instead of round-tripping through a file.
    const std::vector<TimedMidiEvent>& getEvents();

    void clear();

    // Simple custom binary format (see recording.cpp for the exact layout).
    // Returns false on failure (bad path, write error, etc).
    bool saveToFile(const std::string& path);

    // Exports the current recording as a real Standard MIDI File (.mid)
    // instead of the custom .pvrc format above - see midi_file_io.h. Handy
    // for opening a recording in a DAW or other MIDI-aware software.
    bool saveToMidFile(const std::string& path);

    // Loads a previously-saved recording into the internal buffer (this
    // also works as a lightweight way to hand data to playback: load here,
    // then playback::loadFromEvents(recording::getEvents())).
    bool loadFromFile(const std::string& path);
}