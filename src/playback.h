#pragma once
#include "recording.h" // shares the TimedMidiEvent struct with recording
#include <string>
#include <vector>

namespace playback {
    // Loads a previously-saved recording (same .pvrc format recording.cpp
    // writes). Returns false on failure (bad path, wrong format, etc).
    bool loadFromFile(const std::string& path);

    // Imports a real Standard MIDI File (.mid), converting it to our
    // TimedMidiEvent format via midi_file_io.h, then plays it back exactly
    // like a loaded .pvrc recording. Returns false on failure (bad path,
    // unreadable/corrupt MIDI file, etc).
    bool loadFromMidFile(const std::string& path);

    // Loads events directly, e.g. straight from recording::getEvents(), to
    // play back what you just recorded without a round trip through disk.
    void loadFromEvents(std::vector<TimedMidiEvent> newEvents);

    void play();
    void pause();
    void stop(); // pause + rewind to the start
    void seek(double time); // jump to a specific time in seconds, clamped to [0,duration]
    void skip(double deltaSeconds); // jump forward/backward by a number of seconds, clamped to [0,duration]
    void setSpeed(float multiplier); // 1.0 = normal speed, 2.0 = double speed, 0.5 = half speed, etc
    float getSpeed(); // returns the current speed multiplier (default 1.0)


    void togglePlayback();

    bool isPlaying();
    bool hasLoadedRecording();

    // Call once per frame (e.g. from main.cpp's loop). Advances the
    // playhead and directly triggers audio::/blocks:: for any event whose
    // time has come - see playback.cpp for why this bypasses the shared
    // live-MIDI queue rather than reusing it.
    void update(float dt);

    double getPlaybackTime();
    double getDuration();
    size_t getTotalEventCount();
    size_t getEventsPlayedCount();
}